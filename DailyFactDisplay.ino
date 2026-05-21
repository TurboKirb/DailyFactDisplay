#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM"
#endif

// Pin definitions for LilyGo T5 4.7" ESP32-S3
#define EPD_MOSI 15
#define EPD_MISO 16
#define EPD_CLK  14
#define EPD_CS   9
#define EPD_DC   28
#define EPD_RES  27
#define EPD_BUSY 25
#define EPD_PWR  26
// define button via GPIO mapping
#define WIFI_RESET_BUTTON 21


// Must be a valid deep-sleep wake-capable pin.
#define NEXT_BUTTON_PIN 0

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiManager.h> // included to manage wifi connection via portal prompt
#include <vector>
#include <ctype.h>

#include "epd_driver.h"
#include "firasans20.h" // default font
#include "firasans36.h" // modified font using fontconvert.py upsizing to 36
#include "utilities.h"

const long GMT_OFFSET_SEC = -19000;
const int  DST_OFFSET_SEC = 3600;

const char* FACT_API = "https://uselessfacts.jsph.pl/api/v2/facts/random?language=en";

uint8_t* framebuffer = NULL;

// Stored across deep sleep, but not full power loss.
RTC_DATA_ATTR char savedFact[2048] = "";
RTC_DATA_ATTR char savedDateStr[48] = "Date unavailable";
RTC_DATA_ATTR int currentPage = 0;
RTC_DATA_ATTR bool savedWifiOk = false;
RTC_DATA_ATTR int savedWifiRSSI = 0;



// Layout
const int MARGIN = 30;
const int LINE_H = FiraSans20.advance_y + 8;

const int BOX_W = 150;
const int BOX_H = 150;
const int BOX_X = EPD_WIDTH - BOX_W - 30;
const int BOX_Y = EPD_HEIGHT - BOX_H - 30;

const int FACT_X = MARGIN;
const int FACT_Y = 205;
const int FACT_W = EPD_WIDTH - MARGIN * 2 - 110;
const int FACT_H = EPD_HEIGHT - FACT_Y - 40;

String fetchFact() {
    HTTPClient http;
    http.begin(FACT_API);
    http.setTimeout(8000);

    String fact = "No fact available today.";

    if (http.GET() == 200) {
        JsonDocument doc;
        if (!deserializeJson(doc, http.getString())) {
            fact = doc["text"].as<String>();
        }
    }

    http.end();
    return fact;
}

bool connectWifiWithPortal() {
    WiFi.mode(WIFI_STA);

    WiFiManager wm;
    wm.setDebugOutput(true);

    // 0 = stay open until configured
    wm.setConfigPortalTimeout(0);

    bool connected = wm.autoConnect("DeskCalendarSetup");

    return connected;
}

void updateDateString(char* buffer, size_t bufferSize) {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    if (now > 100000) {
        strftime(buffer, bufferSize, "%A, %B %d %Y", &t);
    } else {
        strncpy(buffer, "Date unavailable", bufferSize);
    }
}

uint64_t secondsUntilMidnight() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    int elapsed = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    return (uint64_t)(86400 - elapsed);
}

int availableWidthForLine(int lineIndex) {
    int y = FACT_Y + (lineIndex * LINE_H);

    bool overlapsCalendar =
        y >= (BOX_Y - LINE_H) &&
        y <= (BOX_Y + BOX_H);

    if (overlapsCalendar) {
        return BOX_X - FACT_X - 20; 
    }

    return FACT_W;
}

std::vector<String> wrapTextToLines(const String& text) {
    std::vector<String> lines;

    String src = text;
    String line = "";
    int start = 0;
    int lineIndex = 0;

    while (start <= (int)src.length()) {
        int spaceIdx = src.indexOf(' ', start);
        if (spaceIdx == -1) spaceIdx = src.length();

        String word = src.substring(start, spaceIdx);
        String test = line.length() == 0 ? word : line + " " + word;

        int maxWidth = availableWidthForLine(lineIndex);

        int32_t cx = 0;
        int32_t cy = 0;
        int32_t x1, y1, tw, th;

        get_text_bounds(
            (GFXfont*)&FiraSans20,
            test.c_str(),
            &cx,
            &cy,
            &x1,
            &y1,
            &tw,
            &th,
            nullptr
        );

        if (tw > maxWidth && line.length() > 0) {
            lines.push_back(line);
            lineIndex++;
            line = word;
        } else {
            line = test;
        }

        start = spaceIdx + 1;
    }

    if (line.length() > 0) {
        lines.push_back(line);
    }

    return lines;
}

int linesPerPage() {
    return FACT_H / LINE_H;
}

void drawBorder() {
    epd_draw_rect(10, 10, EPD_WIDTH - 35, EPD_HEIGHT - 35, 0, framebuffer);
}

void resetWifiSettings() {
    WiFiManager wm;
    wm.resetSettings();
    delay(500);
    ESP.restart();
}

void drawHeader(const char* dateStr) {
    int cx = MARGIN;
    int cy = MARGIN + FiraSans20.advance_y + FiraSans20.descender;

    writeln((GFXfont*)&FiraSans20, dateStr, &cx, &cy, framebuffer);
    drawStatusLine();

    int lineY = cy + 12;
    epd_draw_hline(MARGIN, lineY, EPD_WIDTH - MARGIN * 2, 0, framebuffer);

    cy = lineY + 10 + FiraSans20.advance_y;
    cx = MARGIN;
    writeln((GFXfont*)&FiraSans20, "Did you know?", &cx, &cy, framebuffer);
}

void drawFactPage(const String& fact, int page) {
    std::vector<String> lines = wrapTextToLines(fact);

    int maxLines = linesPerPage();
    int totalPages = max(1, (int)((lines.size() + maxLines - 1) / maxLines));

    if (page >= totalPages) {
        currentPage = 0;
        page = 0;
    }

    int startLine = page * maxLines;
    int y = FACT_Y;

    for (int i = 0; i < maxLines && startLine + i < (int)lines.size(); i++) {
        int x = FACT_X;

        writeln(
            (GFXfont*)&FiraSans20,
            lines[startLine + i].c_str(),
            &x,
            &y,
            framebuffer
        );

        y += LINE_H;
    }

    bool hasMorePages = page < totalPages - 1;

    if (hasMorePages) {
        int ex = FACT_X;
        int ey = y + 10;

        writeln(
            (GFXfont*)&FiraSans20,
            "...",
            &ex,
            &ey,
            framebuffer
        );
    }
}
    

void drawCalendarBox() {
    struct tm t;

    char dayStr[4] = "--";
    char monthStr[8] = "---";

    if (getLocalTime(&t, 1000)) {
        snprintf(dayStr, sizeof(dayStr), "%d", t.tm_mday);
        strftime(monthStr, sizeof(monthStr), "%b", &t);

    for (int i = 0; monthStr[i]; i++) {
            monthStr[i] = toupper(monthStr[i]);
        }
    }

    epd_draw_rect(BOX_X, BOX_Y, BOX_W, BOX_H, 0, framebuffer);
    epd_draw_rect(BOX_X + 3, BOX_Y + 3, BOX_W - 6, BOX_H - 6, 0, framebuffer);

    const int HEADER_H = 40;

    int32_t mtx = 0, mty = 0, mx1, my1, mtw, mth;
    get_text_bounds((GFXfont*)&FiraSans20, monthStr, &mtx, &mty, &mx1, &my1, &mtw, &mth, nullptr);

    int monthX = BOX_X + (BOX_W - mtw) / 2 - mx1;
    int monthY = BOX_Y + (HEADER_H + FiraSans20.advance_y) / 2 - FiraSans20.descender;

    writeln((GFXfont*)&FiraSans20, monthStr, &monthX, &monthY, framebuffer);

    int32_t tx = 0, ty = 0, x1, y1, tw, th;
    get_text_bounds((GFXfont*)&FiraSans36, dayStr, &tx, &ty, &x1, &y1, &tw, &th, nullptr);

    int textX = BOX_X + (BOX_W - tw) / 2 - x1;
    int textY = BOX_Y + (BOX_H + FiraSans36.advance_y) / 2 - FiraSans36.descender;

    writeln((GFXfont*)&FiraSans36, dayStr, &textX, &textY, framebuffer);
}

void drawScreen(const char* dateStr, const String& fact, int page) {
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    drawBorder();
    drawHeader(dateStr);
    drawFactPage(fact, page);


    // Draw last so it remains visually static.
    drawCalendarBox();
}

void displayFramebuffer() {
    epd_poweron();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
}

void drawMessageScreen(const char* line1, const char* line2 = "") {
    epd_poweron();
    epd_clear();
    epd_poweroff();

    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    int x = 40;
    int y = 140;

    writeln((GFXfont*)&FiraSans20, line1, &x, &y, framebuffer);

    if (strlen(line2) > 0) {
        y += 50;
        x = 40;
        writeln((GFXfont*)&FiraSans20, line2, &x, &y, framebuffer);
    }

    displayFramebuffer();
}
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(NEXT_BUTTON_PIN, INPUT_PULLUP);
    pinMode(WIFI_RESET_BUTTON, INPUT_PULLUP);

    epd_init();
    epd_poweron();
    epd_clear();
    epd_poweroff();

    framebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);

    if (!framebuffer) {
        Serial.println("PSRAM alloc failed!");
        while (1);
    }

    // Put the IO21 WiFi reset hold check HERE
    if (digitalRead(WIFI_RESET_BUTTON) == LOW) {
    Serial.println("WiFi reset button detected...");
    Serial.println("Hold for 5 seconds to confirm.");

    unsigned long holdStart = millis();

    while (digitalRead(WIFI_RESET_BUTTON) == LOW) {
        delay(50);

        if (millis() - holdStart >= 5000) {
            drawMessageScreen(
                "Resetting WiFi...",
                "Restarting setup portal."
            );

            WiFiManager wm;
            wm.resetSettings();
            WiFi.disconnect(true, true);

            delay(1500);
            ESP.restart();
        }
    }

    Serial.println("WiFi reset cancelled.");
}

    esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

    String fact;

    bool nextPageWake = wakeupReason == ESP_SLEEP_WAKEUP_EXT0;

    if (nextPageWake && strlen(savedFact) > 0) {
        Serial.println("Wake reason: next page button");

        currentPage++;
        fact = String(savedFact);
    } else {
        Serial.println("Wake reason: new daily refresh / reset");

        currentPage = 0;
        fact = "Could not connect to WiFi.";

        drawMessageScreen(
    "Connecting to WiFi...",
    "Portal opens if needed."
);

        if (connectWifiWithPortal()) {
            drawMessageScreen("WiFi Connected", "Fetching today's fact...");

    configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    bool timeReady = getLocalTime(&timeinfo, 10000);

    if (timeReady) {
        strftime(savedDateStr, sizeof(savedDateStr), "%A, %B %d %Y", &timeinfo);
    } else {
        strncpy(savedDateStr, "Date unavailable", sizeof(savedDateStr));
    }

    fact = fetchFact();
    // fact =
    // "This is a deliberately long test fact designed to verify how the page "
    // "wrapping and next-page behavior works on the e-paper display. It should "
    // "continue for several sentences so that it exceeds the available fact area "
    // "and forces the sketch to create multiple pages. The calendar box should "
    // "remain fixed in the bottom-right corner while the visible fact text changes "
    // "from page one to page two. If this works correctly, pressing the next-page "
    // "button should advance through the wrapped text without fetching a new fact.";
    fact.toCharArray(savedFact, sizeof(savedFact));

    savedWifiOk = true;
    savedWifiRSSI = WiFi.RSSI();
    

    WiFi.disconnect(true);

} else {

    strncpy(savedFact, fact.c_str(), sizeof(savedFact));
    strncpy(savedDateStr, "WiFi unavailable", sizeof(savedDateStr));
    savedWifiOk = false;
    savedWifiRSSI = 0;
   
}
    }

    epd_poweron();
epd_clear();
epd_poweroff();

drawScreen(savedDateStr, fact, currentPage);
displayFramebuffer();

    free(framebuffer);
    framebuffer = NULL;

    // Wake either at midnight timer or button press.
    esp_sleep_enable_ext0_wakeup((gpio_num_t)NEXT_BUTTON_PIN, 0);

    uint64_t sleepSecs = secondsUntilMidnight();
    Serial.printf("Sleeping %llu seconds\n", sleepSecs);

    esp_sleep_enable_timer_wakeup(sleepSecs * 1000000ULL);
    esp_deep_sleep_start();
}
void drawStatusLine() {
    char status[32];

    if (savedWifiOk) {
        snprintf(status, sizeof(status), "WiFi %ddBm", savedWifiRSSI);
    } else {
        snprintf(status, sizeof(status), "No WiFi");
    }

    int32_t cx = 0, cy = 0, x1, y1, tw, th;

    get_text_bounds(
        (GFXfont*)&FiraSans20,
        status,
        &cx,
        &cy,
        &x1,
        &y1,
        &tw,
        &th,
        nullptr
    );

    int x = EPD_WIDTH - MARGIN - tw;
    int y = MARGIN + FiraSans20.advance_y + FiraSans20.descender;

    writeln((GFXfont*)&FiraSans20, status, &x, &y, framebuffer);
}

void loop() {}
