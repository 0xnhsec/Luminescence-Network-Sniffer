#ifndef CONFIG_H
#define CONFIG_H

// ============================================
//  Luminescence Network Sniffer — Configuration
//  Phase 2 — Enhanced Logging + SPIFFS
//  Author: Bangkit (0xnhsec)
// ============================================

// --- WiFi Access Point ---
#define AP_SSID     "Luminescence"
#define AP_PASS     "Luminescence"    // min 8 chars for WPA2
#define AP_CHANNEL  6
#define AP_MAX_CONN 4

// --- Network ---
#define AP_LOCAL_IP   IPAddress(192, 168, 4, 1)
#define AP_GATEWAY    IPAddress(192, 168, 4, 1)
#define AP_SUBNET     IPAddress(255, 255, 255, 0)

// --- HTTP Server ---
#define HTTP_PORT 80

// --- Serial Monitor ---
#define SERIAL_BAUD 115200

// --- DNS (Phase 3→5: handled by dns_proxy.h, port 53 hardcoded there) ---

// --- Dashboard (Phase 4) ---
#define DASHBOARD_PATH "/dashboard"
#define EVENT_BUFFER_SIZE 50
#define EVENT_DETAIL_MAX  320

// --- Bridge Mode (Phase 5: AP+STA+NAPT) ---
// Set BRIDGE_MODE to 1 to connect ESP32 to upstream WiFi and NAT traffic.
// Set to 0 to run AP-only (Phase 4 behaviour).
#define BRIDGE_MODE 1
#if BRIDGE_MODE
  #define STA_SSID     "admin123"   // << edit before flashing
  #define STA_PASS     "admin123"   // << edit before flashing
  #define STA_RETRY_MS  30000               // reconnect interval (ms)
#endif

// --- Headers to collect ---
// WebServer needs explicit list of headers to track
#define NUM_HEADERS 6
const char* TRACKED_HEADERS[NUM_HEADERS] = {
  "User-Agent",
  "Referer",
  "Content-Type",
  "Accept-Language",
  "Host",
  "Cookie"
};

#endif
