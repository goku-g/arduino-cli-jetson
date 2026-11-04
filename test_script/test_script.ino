/*
 * ESP32 RGB LED Switcher Web Portal
 * -------------------------------------------------------------
 * Control an RGB LED connected to ESP32 using a web portal hosted
 * directly on the ESP32 (as a Wi-Fi Access Point / Web Gateway).
 *
 * Pins:
 *   - Red   : D0 (GPIO 0 on standard ESP32 / D0 on Xiao & mini boards)
 *   - Green : D1 (GPIO 1 on standard ESP32 / D1 on Xiao & mini boards)
 *   - Blue  : D2 (GPIO 2 on standard ESP32 / D2 on Xiao & mini boards)
 *
 * Features:
 *   - Creates its own Wi-Fi Access Point (Hotspot: "ESP32-RGB-Controller")
 *   - Optional: Can also connect to home Wi-Fi if credentials are provided
 *   - Built-in Web Server with responsive, mobile-friendly Color Palette UI
 *   - Captive Portal / mDNS (http://led.local or http://192.168.4.1)
 *   - Live HTML5 color wheel picker, preset color swatches, RGB & brightness sliders
 *   - Common Anode / Common Cathode toggle
 * -------------------------------------------------------------
 */
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
typedef WebServer WebServerType;

#include <DNSServer.h>

#include "test_lib.h" // Example library usage

// =============================================================
// Configuration
// =============================================================

// Wi-Fi Access Point (SoftAP) Configuration
const char* AP_SSID = "ESP32-RGB-Controller";
const char* AP_PASS = "12345678"; // At least 8 characters (set to "" for open network)

// Optional: Home Wi-Fi credentials (leave empty if using AP mode only)
const char* STA_SSID = "";
const char* STA_PASS = "";

const int PIN_RED   = 4;
const int PIN_GREEN = 16;
const int PIN_BLUE  = 17;

// LED Type: Set to true if Common Anode (active LOW), false if Common Cathode (active HIGH)
const bool IS_COMMON_ANODE = false;

// =============================================================
// Global Objects and State
// =============================================================
WebServerType server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// Current Color State (0 - 255)
uint8_t currentR = 255;
uint8_t currentG = 0;
uint8_t currentB = 0;
uint8_t brightness = 100; // 0 - 100 %

// =============================================================
// Embedded Web Page (HTML / CSS / JavaScript)
// =============================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>ESP32 RGB Controller</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: #1e293b;
      --card-border: #334155;
      --text: #f8fafc;
      --text-muted: #94a3b8;
      --accent: #38bdf8;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      -webkit-tap-highlight-color: transparent;
    }
    body {
      background: var(--bg);
      color: var(--text);
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
      padding: 16px;
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      border-radius: 20px;
      padding: 24px;
      width: 100%;
      max-width: 420px;
      box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5), 0 8px 10px -6px rgba(0, 0, 0, 0.4);
    }
    header {
      text-align: center;
      margin-bottom: 20px;
    }
    header h1 {
      font-size: 1.4rem;
      font-weight: 700;
      letter-spacing: -0.5px;
    }
    header p {
      font-size: 0.85rem;
      color: var(--text-muted);
      margin-top: 4px;
    }
    .preview-container {
      display: flex;
      flex-direction: column;
      align-items: center;
      margin-bottom: 22px;
    }
    .color-preview {
      width: 90px;
      height: 90px;
      border-radius: 50%;
      border: 4px solid #fff;
      box-shadow: 0 0 24px rgba(255, 0, 0, 0.4);
      transition: background-color 0.15s ease, box-shadow 0.15s ease;
      margin-bottom: 10px;
    }
    .color-hex {
      font-size: 1.1rem;
      font-weight: 600;
      font-family: monospace;
      letter-spacing: 1px;
    }
    .section-title {
      font-size: 0.8rem;
      font-weight: 600;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--text-muted);
      margin-bottom: 10px;
    }
    /* Color Wheel Picker Input */
    .picker-btn-wrapper {
      position: relative;
      margin-bottom: 20px;
    }
    .custom-picker-btn {
      width: 100%;
      height: 48px;
      background: linear-gradient(90deg, #ff0000, #ffff00, #00ff00, #00ffff, #0000ff, #ff00ff, #ff0000);
      border: none;
      border-radius: 12px;
      color: #fff;
      font-weight: 700;
      font-size: 0.95rem;
      text-shadow: 0 1px 3px rgba(0,0,0,0.8);
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      box-shadow: 0 4px 12px rgba(0,0,0,0.3);
    }
    #colorPicker {
      position: absolute;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      opacity: 0;
      cursor: pointer;
    }
    /* Quick Swatches Grid */
    .palette-grid {
      display: grid;
      grid-template-columns: repeat(6, 1fr);
      gap: 10px;
      margin-bottom: 22px;
    }
    .swatch {
      width: 100%;
      aspect-ratio: 1;
      border-radius: 10px;
      border: 2px solid transparent;
      cursor: pointer;
      box-shadow: 0 2px 5px rgba(0,0,0,0.2);
      transition: transform 0.12s, border-color 0.12s;
    }
    .swatch:active {
      transform: scale(0.92);
    }
    .swatch.active {
      border-color: #fff;
      box-shadow: 0 0 10px #fff;
    }
    /* Sliders */
    .slider-group {
      margin-bottom: 14px;
    }
    .slider-header {
      display: flex;
      justify-content: space-between;
      font-size: 0.85rem;
      font-weight: 500;
      margin-bottom: 6px;
    }
    input[type=range] {
      -webkit-appearance: none;
      width: 100%;
      height: 7px;
      background: #334155;
      border-radius: 4px;
      outline: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: var(--text);
      cursor: pointer;
      box-shadow: 0 2px 6px rgba(0,0,0,0.5);
    }
    #sliderR::-webkit-slider-thumb { background: #ef4444; }
    #sliderG::-webkit-slider-thumb { background: #22c55e; }
    #sliderB::-webkit-slider-thumb { background: #3b82f6; }
    #sliderBri::-webkit-slider-thumb { background: #facc15; }

    /* Action Buttons */
    .btn-row {
      display: flex;
      gap: 10px;
      margin-top: 18px;
    }
    .btn {
      flex: 1;
      padding: 12px;
      border-radius: 10px;
      border: none;
      font-size: 0.9rem;
      font-weight: 600;
      cursor: pointer;
      transition: opacity 0.15s, transform 0.1s;
    }
    .btn:active {
      transform: scale(0.96);
    }
    .btn-off {
      background: #334155;
      color: #f8fafc;
    }
    .btn-white {
      background: #f8fafc;
      color: #0f172a;
    }
    .footer-info {
      text-align: center;
      font-size: 0.75rem;
      color: var(--text-muted);
      margin-top: 18px;
    }
  </style>
</head>
<body>

  <div class="card">
    <header>
      <h1>ESP32 RGB Switcher</h1>
      <p>Gateway IP: <span id="gatewayIp">Loading...</span></p>
    </header>

    <!-- Preview & Current Hex -->
    <div class="preview-container">
      <div id="preview" class="color-preview" style="background-color: #ff0000;"></div>
      <div id="hexValue" class="color-hex">#FF0000</div>
    </div>

    <!-- Color Palette Input Button -->
    <div class="section-title">Color Palette</div>
    <div class="picker-btn-wrapper">
      <button class="custom-picker-btn">Open Full Color Palette</button>
      <input type="color" id="colorPicker" value="#ff0000">
    </div>

    <!-- Quick Color Preset Swatches -->
    <div class="section-title">Quick Presets</div>
    <div class="palette-grid">
      <div class="swatch" style="background: #FF0000;" onclick="selectHex('#FF0000')"></div>
      <div class="swatch" style="background: #FF7F00;" onclick="selectHex('#FF7F00')"></div>
      <div class="swatch" style="background: #FFFF00;" onclick="selectHex('#FFFF00')"></div>
      <div class="swatch" style="background: #00FF00;" onclick="selectHex('#00FF00')"></div>
      <div class="swatch" style="background: #00FFFF;" onclick="selectHex('#00FFFF')"></div>
      <div class="swatch" style="background: #007FFF;" onclick="selectHex('#007FFF')"></div>
      <div class="swatch" style="background: #0000FF;" onclick="selectHex('#0000FF')"></div>
      <div class="swatch" style="background: #8B00FF;" onclick="selectHex('#8B00FF')"></div>
      <div class="swatch" style="background: #FF00FF;" onclick="selectHex('#FF00FF')"></div>
      <div class="swatch" style="background: #FF1493;" onclick="selectHex('#FF1493')"></div>
      <div class="swatch" style="background: #FFE4B5;" onclick="selectHex('#FFE4B5')"></div>
      <div class="swatch" style="background: #FFFFFF;" onclick="selectHex('#FFFFFF')"></div>
    </div>

    <!-- RGB Sliders -->
    <div class="section-title">RGB Channels</div>
    <div class="slider-group">
      <div class="slider-header"><span>Red</span><span id="valR">255</span></div>
      <input type="range" id="sliderR" min="0" max="255" value="255">
    </div>
    <div class="slider-group">
      <div class="slider-header"><span>Green</span><span id="valG">0</span></div>
      <input type="range" id="sliderG" min="0" max="255" value="0">
    </div>
    <div class="slider-group">
      <div class="slider-header"><span>Blue</span><span id="valB">0</span></div>
      <input type="range" id="sliderB" min="0" max="255" value="0">
    </div>

    <!-- Master Brightness -->
    <div class="section-title" style="margin-top: 14px;">Brightness</div>
    <div class="slider-group">
      <div class="slider-header"><span>Intensity</span><span id="valBri">100%</span></div>
      <input type="range" id="sliderBri" min="0" max="100" value="100">
    </div>

    <!-- Quick Action Buttons -->
    <div class="btn-row">
      <button class="btn btn-off" onclick="turnOff()">Turn Off</button>
      <button class="btn btn-white" onclick="selectHex('#FFFFFF')">Pure White</button>
    </div>

    <div class="footer-info">
      Connected Pins: R=D0, G=D1, B=D2
    </div>
  </div>

  <script>
    let r = 255, g = 0, b = 0, bri = 100;
    let sendTimeout = null;

    const preview = document.getElementById('preview');
    const hexVal = document.getElementById('hexValue');
    const colorPicker = document.getElementById('colorPicker');
    const sR = document.getElementById('sliderR');
    const sG = document.getElementById('sliderG');
    const sB = document.getElementById('sliderB');
    const sBri = document.getElementById('sliderBri');

    // Display gateway IP
    document.getElementById('gatewayIp').innerText = window.location.hostname || '192.168.4.1';

    function componentToHex(c) {
      const hex = Number(c).toString(16);
      return hex.length === 1 ? "0" + hex : hex;
    }

    function rgbToHex(r, g, b) {
      return "#" + componentToHex(r) + componentToHex(g) + componentToHex(b);
    }

    function hexToRgb(hex) {
      const bigint = parseInt(hex.replace("#", ""), 16);
      return {
        r: (bigint >> 16) & 255,
        g: (bigint >> 8) & 255,
        b: bigint & 255
      };
    }

    function updateUI(sendNow = false) {
      const hex = rgbToHex(r, g, b);
      hexVal.innerText = hex.toUpperCase();
      colorPicker.value = hex;

      // Adjust preview by brightness
      const pR = Math.round(r * (bri / 100));
      const pG = Math.round(g * (bri / 100));
      const pB = Math.round(b * (bri / 100));
      const effectiveHex = rgbToHex(pR, pG, pB);

      preview.style.backgroundColor = effectiveHex;
      preview.style.boxShadow = `0 0 25px ${effectiveHex}88`;

      sR.value = r;
      sG.value = g;
      sB.value = b;
      sBri.value = bri;

      document.getElementById('valR').innerText = r;
      document.getElementById('valG').innerText = g;
      document.getElementById('valB').innerText = b;
      document.getElementById('valBri').innerText = bri + '%';

      if (sendNow) {
        sendColor();
      } else {
        debounceSend();
      }
    }

    function debounceSend() {
      if (sendTimeout) clearTimeout(sendTimeout);
      sendTimeout = setTimeout(sendColor, 50); // 50ms debounce for smooth responsiveness
    }

    function sendColor() {
      fetch(`/set?r=${r}&g=${g}&b=${b}&bri=${bri}`)
        .catch(err => console.log('HTTP request failed:', err));
    }

    function selectHex(hex) {
      const rgb = hexToRgb(hex);
      r = rgb.r;
      g = rgb.g;
      b = rgb.b;
      updateUI(true);
    }

    function turnOff() {
      r = 0; g = 0; b = 0;
      updateUI(true);
    }

    // Color picker change event
    colorPicker.addEventListener('input', (e) => {
      selectHex(e.target.value);
    });

    // Sliders input events
    sR.addEventListener('input', (e) => { r = parseInt(e.target.value); updateUI(); });
    sG.addEventListener('input', (e) => { g = parseInt(e.target.value); updateUI(); });
    sB.addEventListener('input', (e) => { b = parseInt(e.target.value); updateUI(); });
    sBri.addEventListener('input', (e) => { bri = parseInt(e.target.value); updateUI(); });

    // Initial state fetch from ESP32
    fetch('/status')
      .then(res => res.json())
      .then(data => {
        r = data.r; g = data.g; b = data.b; bri = data.bri;
        updateUI();
      })
      .catch(() => updateUI());
  </script>
</body>
</html>
)rawliteral";

// =============================================================
// Hardware Control Functions
// =============================================================

// Apply PWM output to a given pin with brightness scaling and LED polarity
void writeChannel(int pin, uint8_t value, uint8_t briPercent) {
  // Scale value by master brightness (0 - 100%)
  uint32_t scaled = ((uint32_t)value * (uint32_t)briPercent) / 100;
  
  if (IS_COMMON_ANODE) {
    scaled = 255 - scaled;
  }
  
  // analogWrite() is standard across ESP32 Arduino Core 2.x and 3.x
  analogWrite(pin, (int)scaled);
}

void applyColors() {
  writeChannel(PIN_RED, currentR, brightness);
  writeChannel(PIN_GREEN, currentG, brightness);
  writeChannel(PIN_BLUE, currentB, brightness);
}

// =============================================================
// Web Server Route Handlers
// =============================================================

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleSetColor() {
  if (server.hasArg("r")) {
    currentR = constrain(server.arg("r").toInt(), 0, 255);
  }
  if (server.hasArg("g")) {
    currentG = constrain(server.arg("g").toInt(), 0, 255);
  }
  if (server.hasArg("b")) {
    currentB = constrain(server.arg("b").toInt(), 0, 255);
  }
  if (server.hasArg("bri")) {
    brightness = constrain(server.arg("bri").toInt(), 0, 100);
  }

  applyColors();
  server.send(200, "text/plain", "OK");
}

void handleStatus() {
  char json[100];
  snprintf(json, sizeof(json), "{\"r\":%d,\"g\":%d,\"b\":%d,\"bri\":%d}", 
           currentR, currentG, currentB, brightness);
  server.send(200, "application/json", json);
}

// Captive Portal Redirect Handler
void handleCaptivePortal() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "Redirecting to ESP32 Gateway (wait)...");
}

// =============================================================
// Setup & Loop
// =============================================================

void setup() {
  testFunction(); // Call the function from the library
  
  Serial.begin(115200);
  delay(500);

  Serial.println("\n--- ESP32 RGB LED Switcher Starting ---");

  // Configure output pins
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);

#if defined(ESP8266)
  analogWriteRange(255); // Match 8-bit (0-255) resolution on ESP8266
#endif

  // Apply initial default color (Red at 100% brightness)
  applyColors();

  // Set Wi-Fi Mode
  // If STA credentials provided, use AP + STA; otherwise AP only
  if (strlen(STA_SSID) > 0) {
    WiFi.mode(WIFI_AP_STA);
    Serial.printf("Connecting to Wi-Fi SSID: %s...\n", STA_SSID);
    WiFi.begin(STA_SSID, STA_PASS);
  } else {
    WiFi.mode(WIFI_AP);
  }

  // Configure SoftAP (Hotspot)
  WiFi.softAP(AP_SSID, strlen(AP_PASS) >= 8 ? AP_PASS : NULL);
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("SoftAP Created! SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Access Portal Gateway IP: http://");
  Serial.println(apIP);

  // Start Captive Portal DNS Server (redirects all requests to ESP32 IP)
  dnsServer.start(DNS_PORT, "*", apIP);

  // Start mDNS responder (http://led.local)
  if (MDNS.begin("led")) {
    Serial.println("mDNS responder started: http://led.local");
  }

  // Web Server Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_GET, handleSetColor);
  server.on("/status", HTTP_GET, handleStatus);

  // Android, iOS, Windows captive portal detection redirects
  server.on("/generate_204", handleCaptivePortal);
  server.on("/gen_204", handleCaptivePortal);
  server.on("/hotspot-detect.html", handleCaptivePortal);
  server.on("/ncsi.txt", handleCaptivePortal);
  server.onNotFound(handleCaptivePortal);

  // Start Web Server
  server.begin();
  Serial.println("HTTP Web Server Started.");
}

void loop() {
  // Process Captive Portal DNS queries
  dnsServer.processNextRequest();

  // Handle incoming HTTP client requests
  server.handleClient();

#if defined(ESP8266)
  MDNS.update();
#endif

  // Optional: Check and print STA connection if configured
  static bool staConnected = false;
  if (WiFi.status() == WL_CONNECTED && !staConnected) {
    staConnected = true;
    Serial.print("Station connected! Local Network IP: http://");
    Serial.println(WiFi.localIP());
  }
}
