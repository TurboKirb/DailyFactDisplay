# Desk Calendar Display (ESP32 + E-Paper)

A minimalist desktop calendar and daily fact display powered by an ESP32-S3 and the LilyGo T5 4.7" e-paper display.

Designed to function more like a small appliance than a traditional microcontroller project, the display refreshes daily with a new fact while maintaining ultra-low power consumption using deep sleep.

---

## Features

* Daily random fact pulled from an online API
* Large persistent calendar/date display
* Automatic Wi-Fi onboarding using captive portal
* Deep sleep power optimization
* Multi-page support for long facts
* Static calendar layout during page navigation
* Wi-Fi status indicator with signal strength
* Dedicated Wi-Fi reset button
* Persistent page state during sleep
* Adaptive text wrapping around calendar box
* E-paper friendly refresh handling

---

# Hardware

Developed for:

* LILYGO T5 4.7" ESP32-S3 E-Paper Display

Core components:

* ESP32-S3
* 4.7" grayscale e-paper display
* PSRAM enabled
* onboard BOOT button (page advance)
* onboard IO21 button (Wi-Fi reset/setup)

---

# Behavior Overview

| Action                  | Result                                |
| ----------------------- | ------------------------------------- |
| Power on / Reset        | Fetches a new daily fact              |
| Midnight wake           | Automatically refreshes with new fact |
| BOOT button press       | Advances to next page for long facts  |
| Hold IO21 for 5 seconds | Clears Wi-Fi settings                 |
| No saved Wi-Fi          | Launches captive portal               |

---

# Wi-Fi Setup

The device uses WiFiManager for wireless onboarding.

If no Wi-Fi credentials are saved, the device creates a hotspot:

```text
DeskCalendarSetup
```

Connect to this network from a phone or computer and complete setup through the captive portal.

Default portal address:

```text
192.168.4.1
```

---

# Controls

## BOOT Button (IO0)

Used for:

* advancing long fact pages

The current fact is preserved between page changes using RTC memory.

---

## IO21 Button

Used for:

* resetting Wi-Fi configuration

### To reset Wi-Fi:

1. Hold IO21 during boot
2. Continue holding for 5 seconds
3. Device clears saved credentials
4. Captive portal becomes available again

---

# Power Management

The device uses ESP32 deep sleep to minimize power usage.

Behavior:

* wakes at midnight automatically
* refreshes content
* redraws display
* returns to sleep

The e-paper display retains the image even while the ESP32 sleeps.

---

# Text Rendering

The project includes a custom text wrapping system designed specifically for the layout.

Features include:

* dynamic line wrapping
* collision avoidance with calendar box
* automatic pagination for long facts
* continuation indicators (`...`)
* persistent page state during sleep

---

# Status Indicator

Top-right status display includes:

* Wi-Fi connection state
* Wi-Fi signal strength (RSSI)

Example:

```text
WiFi -58dBm
```

---

# Important Notes

## E-Paper Refreshing

E-paper displays refresh differently from LCD/OLED displays.

During updates you may notice:

* flashing
* full-screen refresh cycles
* brief inversion effects

This is normal behavior.

---

## Deep Sleep Behavior

The ESP32 enters deep sleep after drawing the display.

This means:

* serial logging stops during sleep
* the display remains visible
* the processor is effectively off until wake

---

## RTC Memory

The current fact and page number are stored using RTC memory.

This allows:

* page persistence during sleep
* quick page navigation without re-fetching data

RTC memory survives deep sleep but does not survive:

* full power loss
* firmware reflashing

---

# Libraries Used

* WiFiManager
* ArduinoJson
* HTTPClient
* LilyGo EPD driver libraries

---

# Future Ideas

Potential future enhancements:

* OTA firmware updates
* weather integration
* cached offline facts
* quote/category modes
* moon phase display
* Home Assistant integration
* multiple visual themes
* partial refresh optimization

---

# License

MIT License

---

# Credits

Built using:

* ESP32 Arduino framework
* LilyGo display libraries
* WiFiManager captive portal system
* uselessfacts.jsph.pl API
