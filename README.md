# Embedded Web Server (ESP32)

**Author:** Pratham Pathak
**Intern ID:** CITS2620
**Internship:** CodTech IT Solutions Pvt. Ltd.
**Project duration:** 13 June - 26 June 2026



A self-hosted control + monitoring dashboard running entirely on the ESP32 —
no cloud service, no external backend.

## Features
- ESP32 connects to WiFi (station mode), with automatic Access Point
  fallback (`ESP32-WebServer` / `12345678`) if no network is found.
- Serves a single-page HTML/CSS/JS dashboard directly from flash.
- Two GPIO outputs (LED1/LED2) controllable from the browser.
- Live analog sensor readings (temperature-style conversion), smoothed
  with a moving-average filter.
- `/api/status` JSON REST endpoint for programmatic polling.
- `/api/toggle?led=1` REST endpoint (POST) to flip an output.
- Dashboard auto-refreshes every 2 seconds via `fetch()` (AJAX, no page reload).

## Hardware
| Component        | ESP32 Pin |
|-------------------|-----------|
| LED 1 (+resistor) | GPIO 2    |
| LED 2 (+resistor) | GPIO 4    |
| Analog sensor     | GPIO 34   |

## Libraries
- `ArduinoJson` (install via Arduino Library Manager)
- `WiFi.h`, `WebServer.h` (bundled with ESP32 board package)

## Setup
1. Open `embedded_web_server.ino` in Arduino IDE.
2. Set `WIFI_SSID` / `WIFI_PASSWORD` at the top of the file.
3. Select your ESP32 board + correct COM port, upload.
4. Open Serial Monitor at 115200 baud to see the assigned IP address.
5. Visit that IP in a browser on the same network.

## Talking points for interviews
- Why `WebServer` here is synchronous/blocking vs `AsyncWebServer`, and
  the trade-offs (simplicity vs concurrent request handling).
- Why the ADC reading is smoothed with a moving average (ADC noise).
- Why GPIO 34 was chosen (input-only ADC1 pin, doesn't conflict with WiFi
  which uses ADC2 internally).
- How the REST API separates data (JSON) from presentation (HTML/JS).

## Possible extensions (good "future work" section for your repo)
- Add WebSocket-based push updates instead of polling.
- Add HTTPS via self-signed cert.
- Persist LED state across reboots using ESP32 NVS/Preferences.
