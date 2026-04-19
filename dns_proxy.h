#ifndef DNS_PROXY_H
#define DNS_PROXY_H

// ============================================
//  DNS Proxy — Phase 5
//  Bridge-aware selective DNS resolution:
//    - Captive portal probe domains → AP local IP (keeps popup working)
//    - All other domains → forward to 8.8.8.8 via NAPT (real internet)
//    - Bridge DOWN → all domains → local IP (same as original DNSServer)
//
//  Replaces Arduino DNSServer which can only do wildcard redirect.
//  Non-blocking: pending upstream queries are resolved asynchronously.
// ============================================

#include <Arduino.h>
#include <WiFiUdp.h>

namespace DNSProxy {

  // ---- Config ----
  static const int      MAX_PENDING  = 8;
  static const uint32_t TIMEOUT_MS   = 3000;
  static const IPAddress UPSTREAM_DNS(8, 8, 8, 8);

  struct Pending {
    bool      active;
    uint16_t  origId;
    uint32_t  ts;
    IPAddress clientIP;
    uint16_t  clientPort;
  };

  static WiFiUDP  _srv;                // :53 — receives client queries
  static WiFiUDP  _fwd;                // :5354 — upstream DNS comms
  static uint8_t  _buf[512];
  static Pending  _pend[MAX_PENDING];
  static uint32_t _localIP;           // AP IP in Arduino uint32_t form

  // Captive portal probe domains → always answer with local IP
  static const char* PORTAL_HOSTS[] = {
    "connectivitycheck.gstatic.com",
    "clients3.google.com",
    "connectivitycheck.android.com",
    "captive.apple.com",
    "www.apple.com",
    "appleiphonecell.com",
    "www.msftconnecttest.com",
    "www.msftncsi.com",
    "detectportal.firefox.com",
    "networkcheck.kde.org",
    "nmcheck.gnome.org",
    nullptr
  };

  static bool isPortal(const char* host) {
    for (int i = 0; PORTAL_HOSTS[i]; i++)
      if (strcasecmp(host, PORTAL_HOSTS[i]) == 0) return true;
    return false;
  }

  // Parse DNS QNAME at pkt[offset] into dotted string
  static void parseName(const uint8_t* pkt, int pktLen,
                        int offset, char* out, int maxOut) {
    int pos = 0, guard = 0;
    while (offset < pktLen && guard++ < 64) {
      uint8_t c = pkt[offset];
      if (c == 0) break;
      if ((c & 0xC0) == 0xC0) {                      // compression pointer
        if (offset + 1 >= pktLen) break;
        offset = ((c & 0x3F) << 8) | pkt[offset + 1];
        continue;
      }
      offset++;
      if (pos > 0 && pos < maxOut - 1) out[pos++] = '.';
      for (int i = 0; i < c && offset < pktLen && pos < maxOut - 1; i++)
        out[pos++] = (char)pkt[offset++];
    }
    if (pos < maxOut) out[pos] = '\0';
  }

  // Build and send a minimal A-record DNS reply
  static void sendLocal(const uint8_t* q, int qLen,
                        const IPAddress& dst, uint16_t port) {
    if (qLen + 16 > (int)sizeof(_buf)) return;
    uint8_t r[512];
    memcpy(r, q, qLen);
    r[2] = 0x84; r[3] = 0x00;   // QR=1, AA=1, RCODE=0
    r[6] = 0x00; r[7] = 0x01;   // ANCOUNT = 1
    int n = qLen;
    r[n++] = 0xC0; r[n++] = 0x0C;  // NAME → question (compression)
    r[n++] = 0x00; r[n++] = 0x01;  // TYPE A
    r[n++] = 0x00; r[n++] = 0x01;  // CLASS IN
    r[n++] = 0x00; r[n++] = 0x00;  // TTL
    r[n++] = 0x00; r[n++] = 0x3C;  // TTL = 60 s
    r[n++] = 0x00; r[n++] = 0x04;  // RDLENGTH
    r[n++] = _localIP        & 0xFF;
    r[n++] = (_localIP >>  8) & 0xFF;
    r[n++] = (_localIP >> 16) & 0xFF;
    r[n++] = (_localIP >> 24) & 0xFF;
    _srv.beginPacket(dst, port);
    _srv.write(r, n);
    _srv.endPacket();
  }

  static int freePendingSlot() {
    unsigned long now = millis();
    for (int i = 0; i < MAX_PENDING; i++) {
      if (!_pend[i].active) return i;
      if (now - _pend[i].ts > TIMEOUT_MS) {
        _pend[i].active = false;
        return i;
      }
    }
    return -1;
  }

  // Call once in setup()
  void begin(uint32_t localIP) {
    _localIP = localIP;
    memset(_pend, 0, sizeof(_pend));
    _srv.begin(53);
    _fwd.begin(5354);
  }

  // Call every loop() — non-blocking
  // bridgeActive=false  →  all domains → local IP (AP-only mode)
  // bridgeActive=true   →  portal probes → local IP, rest → 8.8.8.8
  void processNext(bool bridgeActive) {
    // --- Check for upstream DNS responses ---
    int rLen = _fwd.parsePacket();
    if (rLen > 0 && rLen <= (int)sizeof(_buf)) {
      int rn = _fwd.read(_buf, sizeof(_buf));
      if (rn >= 2) {
        uint16_t id = ((uint16_t)_buf[0] << 8) | _buf[1];
        for (int i = 0; i < MAX_PENDING; i++) {
          if (_pend[i].active && _pend[i].origId == id) {
            _srv.beginPacket(_pend[i].clientIP, _pend[i].clientPort);
            _srv.write(_buf, rn);
            _srv.endPacket();
            _pend[i].active = false;
            break;
          }
        }
      }
    }

    // --- Handle incoming client DNS query ---
    int qLen = _srv.parsePacket();
    if (qLen <= 0 || qLen > (int)sizeof(_buf)) return;
    int n = _srv.read(_buf, sizeof(_buf));
    if (n < 12) return;
    if ((_buf[2] & 0xF8) != 0x00) return;  // skip non-standard queries

    IPAddress client = _srv.remoteIP();
    uint16_t  cport  = _srv.remotePort();

    char host[128] = {};
    parseName(_buf, n, 12, host, sizeof(host));

    bool forward = bridgeActive && !isPortal(host);

    if (!forward) {
      sendLocal(_buf, n, client, cport);
    } else {
      int slot = freePendingSlot();
      if (slot >= 0) {
        _pend[slot].active     = true;
        _pend[slot].origId     = ((uint16_t)_buf[0] << 8) | _buf[1];
        _pend[slot].ts         = millis();
        _pend[slot].clientIP   = client;
        _pend[slot].clientPort = cport;
        _fwd.beginPacket(UPSTREAM_DNS, 53);
        _fwd.write(_buf, n);
        _fwd.endPacket();
      }
      // if no slot → client will auto-retry DNS
    }
  }

}  // namespace DNSProxy

#endif
