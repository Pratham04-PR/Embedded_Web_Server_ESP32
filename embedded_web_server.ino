/*
 * ================================================================
 *  Embedded Web Server (ESP32) - Medium Level Project
 * ================================================================
 * WHAT THIS PROJECT DOES
 * -----------------------
 * The ESP32 hosts its own web server (no external cloud/backend).
 * A browser on the same WiFi network can:
 *   1. View live sensor data (temperature via NTC/LM35 on ADC pin,
 *      or just the raw analog value if no sensor is wired).
 *   2. Toggle two GPIO outputs (e.g., LEDs/relays) via buttons.
 *   3. Hit a JSON REST API (/api/status) for programmatic access.
 *   4. See data auto-refresh every 2s using JavaScript fetch(),
 *      demonstrating AJAX without a page reload.
 *
 * HARDWARE
 * --------
 *   - ESP32 dev board
 *   - LED 1 -> GPIO 2  (through 220ohm resistor to GND)
 *   - LED 2 -> GPIO 4  (through 220ohm resistor to GND)
 *   - Optional analog sensor (LM35 / NTC thermistor / potentiometer)
 *     -> GPIO 34 (ADC1_CH6, input only pin)
 *
 * LIBRARIES NEEDED (Arduino Library Manager)
 * -------------------------------------------
 *   - ArduinoJson (by Benoit Blanchon)  -> for clean JSON building
 *   (WiFi.h and WebServer.h are bundled with the ESP32 board package)
 *
 * HOW IT WORKS (read this before your interview!)
 * --------------------------------------------------
 *   - WiFi.begin() connects the ESP32 to your router as a STATION.
 *   - WebServer listens on port 80 and maps URL paths ("routes") to
 *     handler functions using server.on("/path", handlerFunction).
 *   - server.handleClient() must be called every loop() iteration;
 *     it checks for incoming TCP connections and dispatches them to
 *     the correct handler. This is a synchronous, single-threaded
 *     server -> simple to reason about, but a slow handler blocks
 *     everything else. (Mention this trade-off if asked!)
 *   - The HTML page embeds JavaScript that polls /api/status every
 *     2 seconds and updates the DOM without reloading the page.
 * ================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// ---------------- USER CONFIG ----------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const int LED1_PIN   = 2;
const int LED2_PIN   = 4;
const int SENSOR_PIN = 34;   // ADC1_CH6, input-only pin on most ESP32 boards
// ----------------------------------------------

WebServer server(80);

bool led1State = false;
bool led2State = false;

// Simple moving-average filter for the ADC reading so the value
// doesn't jitter wildly on the dashboard.
const int SAMPLES = 10;
int adcBuffer[SAMPLES];
int adcIndex = 0;

int readFilteredAdc() {
  adcBuffer[adcIndex] = analogRead(SENSOR_PIN);
  adcIndex = (adcIndex + 1) % SAMPLES;
  long sum = 0;
  for (int i = 0; i < SAMPLES; i++) sum += adcBuffer[i];
  return sum / SAMPLES;
}

// Convert a 12-bit ADC reading (0-4095, 0-3.3V) to an approximate
// Celsius value assuming an LM35 (10mV/°C) fed through a simple
// voltage divider. Adjust the formula to match your actual sensor.
float adcToCelsius(int adcValue) {
  float voltage = (adcValue / 4095.0) * 3.3;
  float celsius = voltage * 100.0; // LM35: 10mV per degree C
  return celsius;
}

// ---------------- HTML PAGE (stored in flash via PROGMEM-friendly R"()" ) ----------------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Embedded Web Server</title>
  <style>
    body { font-family: Arial, sans-serif; background:#111; color:#eee; text-align:center; padding-top:30px; }
    .card { background:#1e1e1e; border-radius:12px; padding:20px; margin:15px auto; width:320px; box-shadow:0 4px 10px rgba(0,0,0,0.4); }
    button { padding:12px 20px; margin:6px; border:none; border-radius:8px; font-size:16px; cursor:pointer; }
    .on  { background:#2ecc71; color:#000; }
    .off { background:#555;   color:#fff; }
    h1 { color:#4ea8de; }
    .value { font-size:28px; font-weight:bold; color:#f1c40f; }
  </style>
</head>
<body>
  <h1>ESP32 Control Dashboard</h1>

  <div class="card">
    <h3>LED 1 (GPIO 2)</h3>
    <button id="btn1" onclick="toggleLed(1)">Loading...</button>
  </div>

  <div class="card">
    <h3>LED 2 (GPIO 4)</h3>
    <button id="btn2" onclick="toggleLed(2)">Loading...</button>
  </div>

  <div class="card">
    <h3>Sensor Reading</h3>
    <div class="value" id="temp">-- °C</div>
    <div id="raw">raw ADC: --</div>
  </div>

  <div class="card">
    <div id="uptime">Uptime: --</div>
  </div>

<script>
async function refreshStatus() {
  try {
    const res = await fetch('/api/status');
    const data = await res.json();
    setBtn('btn1', data.led1);
    setBtn('btn2', data.led2);
    document.getElementById('temp').innerText = data.temperature.toFixed(1) + ' °C';
    document.getElementById('raw').innerText = 'raw ADC: ' + data.rawAdc;
    document.getElementById('uptime').innerText = 'Uptime: ' + data.uptimeSeconds + ' s';
  } catch (e) {
    console.log('status fetch failed', e);
  }
}

function setBtn(id, state) {
  const btn = document.getElementById(id);
  btn.innerText = state ? 'ON' : 'OFF';
  btn.className = state ? 'on' : 'off';
}

async function toggleLed(num) {
  await fetch('/api/toggle?led=' + num, { method: 'POST' });
  refreshStatus();
}

refreshStatus();
setInterval(refreshStatus, 2000);
</script>
</body>
</html>
)rawliteral";

// ---------------- ROUTE HANDLERS ----------------

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleStatus() {
  int rawAdc = readFilteredAdc();
  float tempC = adcToCelsius(rawAdc);

  StaticJsonDocument<256> doc;
  doc["led1"] = led1State;
  doc["led2"] = led2State;
  doc["rawAdc"] = rawAdc;
  doc["temperature"] = tempC;
  doc["uptimeSeconds"] = millis() / 1000;
  doc["freeHeap"] = ESP.getFreeHeap();

  String response;
  serializeJson(doc, response);
  server.send(200, "application/json", response);
}

void handleToggle() {
  if (!server.hasArg("led")) {
    server.send(400, "text/plain", "Missing 'led' argument");
    return;
  }
  int led = server.arg("led").toInt();
  if (led == 1) {
    led1State = !led1State;
    digitalWrite(LED1_PIN, led1State ? HIGH : LOW);
  } else if (led == 2) {
    led2State = !led2State;
    digitalWrite(LED2_PIN, led2State ? HIGH : LOW);
  } else {
    server.send(400, "text/plain", "Invalid led id");
    return;
  }
  server.send(200, "text/plain", "OK");
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Route not found");
}

// ---------------- SETUP / LOOP ----------------

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED1_PIN, OUTPUT);
  pinMode(LED2_PIN, OUTPUT);
  digitalWrite(LED1_PIN, LOW);
  digitalWrite(LED2_PIN, LOW);

  analogReadResolution(12); // ESP32 ADC: 0-4095
  for (int i = 0; i < SAMPLES; i++) adcBuffer[i] = 0;

  Serial.printf("Connecting to WiFi SSID: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("Open in browser: http://");
    Serial.println(WiFi.localIP());
  } else {
    // Fallback: become its own Access Point so the demo still works
    // even without a known WiFi network nearby.
    Serial.println("\nWiFi failed - starting Access Point fallback");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32-WebServer", "12345678");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  server.on("/", handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/toggle", HTTP_POST, handleToggle);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
  // Non-blocking: no delay() here on purpose so the server stays responsive.
}
