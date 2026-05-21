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

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "epd_driver.h"
#include "firasans20.h"
#include "firasans36.h"
#include "utilities.h"

const char* WIFI_SSID      = "Wifi Name";
const char* WIFI_PASSWORD  = "Password";
const long  GMT_OFFSET_SEC = -19000; // Enter your offset from GMT Ex: US EST is -18000
const int   DST_OFFSET_SEC = 3600; // If your location uses DST then include 3600, otherwise 0.
const char* FACT_API = "https://uselessfacts.jsph.pl/api/v2/facts/random?language=en";

uint8_t* framebuffer = NULL;

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

uint64_t secondsUntilMidnight() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    int elapsed = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
    return (uint64_t)(86400 - elapsed);
}

void drawWrappedText(const char* text, int x, int* y, int maxWidth, int lineHeight) {
    String src = String(text);
    String line = "";
    int start = 0;
    while (start <= (int)src.length()) {
        int spaceIdx = src.indexOf(' ', start);
        if (spaceIdx == -1) spaceIdx = src.length();
        String word = src.substring(start, spaceIdx);
        String test = (line.length() == 0) ? word : line + " " + word;
        
        // Pass real cursor positions, not nullptr
        int32_t cx = 0, cy = 0;
        int32_t x1, y1, tw, th;
        get_text_bounds((GFXfont*)&FiraSans20, test.c_str(), &cx, &cy, &x1, &y1, &tw, &th, nullptr);
        
        if (tw > maxWidth && line.length() > 0) {
            int wcx = x;
            writeln((GFXfont*)&FiraSans20, line.c_str(), &wcx, y, framebuffer);
            *y += lineHeight;
            line = word;
        } else {
            line = test;
        }
        start = spaceIdx + 1;
    }
    if (line.length() > 0) {
        int wcx = x;
        writeln((GFXfont*)&FiraSans20, line.c_str(), &wcx, y, framebuffer);
        *y += lineHeight;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    epd_init();
    epd_poweron();
    epd_clear();
    epd_poweroff();

    // Allocate framebuffer AFTER epd_init
    framebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer) { Serial.println("PSRAM alloc failed!"); while(1); }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries++ < 30) {
        delay(500); Serial.print(".");
    }

    char dateStr[40] = "Date unavailable";
    String fact = "Could not connect to WiFi.";

    if (WiFi.status() == WL_CONNECTED) {
        configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org", "time.nist.gov");
        delay(2000);
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        strftime(dateStr, sizeof(dateStr), "%A, %B %d %Y", &t);
        fact = fetchFact();
        WiFi.disconnect(true);
    }
// ── Draw to framebuffer ──────────────────────────────────

const int MARGIN  = 30;
const int LINE_H  = FiraSans20.advance_y + 8;
const int MAX_W   = EPD_WIDTH - MARGIN * 2;

// --- Outer border ---
// epd_draw_rect signature: (x, y, w, h, color, fb)
epd_draw_rect(10, 10, EPD_WIDTH - 35, EPD_HEIGHT - 35, 0, framebuffer);

// --- Date (top, large) ---
int cx = MARGIN;
int cy = MARGIN + FiraSans20.advance_y + FiraSans20.descender;
writeln((GFXfont*)&FiraSans20, dateStr, &cx, &cy, framebuffer);

// --- Divider line under date ---
int lineY = cy + 12;
epd_draw_hline(MARGIN, lineY, MAX_W, 0, framebuffer);

// --- "Did you know?" label ---
cy = lineY + 10 + FiraSans20.advance_y;
cx = MARGIN;
writeln((GFXfont*)&FiraSans20, "Did you know?", &cx, &cy, framebuffer);

// --- Fact (word-wrapped) ---
cy += 50; // --spacing between Did You Know? and intial fact
drawWrappedText(fact.c_str(), MARGIN, &cy, MAX_W - 100, LINE_H);  // narrower to avoid the day box

// --- Day-of-month box in bottom-right ---
{
    // Get just the day number from the time struct
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    char dayStr[4];
    snprintf(dayStr, sizeof(dayStr), "%d", t.tm_mday);

    // Box dimensions
    const int BOX_W = 150;
    const int BOX_H = 150;
    const int BOX_X = EPD_WIDTH  - BOX_W - 30;
    const int BOX_Y = EPD_HEIGHT - BOX_H - 30;

    // Draw box (double border for emphasis)
    epd_draw_rect(BOX_X,     BOX_Y,     BOX_W,     BOX_H,     0, framebuffer);
    epd_draw_rect(BOX_X + 3, BOX_Y + 3, BOX_W - 6, BOX_H - 6, 0, framebuffer);

    // Define a header band inside the top of the box
const int HEADER_H = 40;  // tweak height


// Build & uppercase the month
char monthStr[8];
strftime(monthStr, sizeof(monthStr), "%b", &t);
for (int i = 0; monthStr[i]; i++) monthStr[i] = toupper(monthStr[i]);

// Center month inside the header band
// NOTE: writeln draws in black; to get white text on a black band you'd need
// a font drawn light-on-dark, OR just leave the band white with a thin border.
int32_t mtx = 0, mty = 0, mx1, my1, mtw, mth;
get_text_bounds((GFXfont*)&FiraSans20, monthStr, &mtx, &mty, &mx1, &my1, &mtw, &mth, nullptr);
int monthX = BOX_X + (BOX_W - mtw) / 2 - mx1;
int monthY = BOX_Y + (HEADER_H + FiraSans20.advance_y) / 2 - FiraSans20.descender;
writeln((GFXfont*)&FiraSans20, monthStr, &monthX, &monthY, framebuffer);

//epd_draw_hline(BOX_X, BOX_Y + HEADER_H, BOX_W, 0, framebuffer);

   // Measure the day number to center it in the box
int32_t tx = 0, ty = 0, x1, y1, tw, th;
get_text_bounds((GFXfont*)&FiraSans36, dayStr, &tx, &ty, &x1, &y1, &tw, &th, nullptr);
int textX = BOX_X + (BOX_W - tw) / 2 - x1;
int textY = BOX_Y + (BOX_H + FiraSans36.advance_y) / 2 - FiraSans36.descender;
writeln((GFXfont*)&FiraSans36, dayStr, &textX, &textY, framebuffer);
}

epd_poweron();
epd_draw_grayscale_image(epd_full_screen(), framebuffer);
epd_poweroff();

    free(framebuffer);
    framebuffer = NULL;

    uint64_t sleepSecs = secondsUntilMidnight();
    Serial.printf("Sleeping %llu seconds\n", sleepSecs);
    esp_deep_sleep(sleepSecs * 1000000ULL);
}

void loop() {}
