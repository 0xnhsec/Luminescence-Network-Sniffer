#ifndef EVENT_BUFFER_H
#define EVENT_BUFFER_H

#include <Arduino.h>

// ============================================
//  Event Buffer — Phase 4
//  Ring buffer feeding the live web dashboard
// ============================================

#ifndef EVENT_BUFFER_SIZE
#define EVENT_BUFFER_SIZE 50
#endif

#ifndef EVENT_DETAIL_MAX
#define EVENT_DETAIL_MAX 320
#endif

enum EventType {
  EVT_CLIENT_CONNECT,
  EVT_CLIENT_DISCONNECT,
  EVT_HTTP_GET,
  EVT_HTTP_POST,
  EVT_PORTAL_CHECK,
  EVT_CREDENTIAL,
  EVT_BIODATA,
  EVT_INFO,
  EVT_BRIDGE_UP,
  EVT_BRIDGE_DOWN
};

struct Event {
  unsigned long id;
  unsigned long ts;
  EventType type;
  String ip;
  String detail;   // pre-serialized JSON object literal, e.g. {"k":"v"}
};

namespace EventBuffer {

  Event events[EVENT_BUFFER_SIZE];
  unsigned long nextId = 1;
  unsigned int writeIdx = 0;
  unsigned int count = 0;

  inline const char* typeStr(EventType t) {
    switch (t) {
      case EVT_CLIENT_CONNECT:    return "connect";
      case EVT_CLIENT_DISCONNECT: return "disconnect";
      case EVT_HTTP_GET:          return "get";
      case EVT_HTTP_POST:         return "post";
      case EVT_PORTAL_CHECK:      return "portal";
      case EVT_CREDENTIAL:        return "credential";
      case EVT_BIODATA:           return "biodata";
      case EVT_INFO:              return "info";
      case EVT_BRIDGE_UP:         return "bridge_up";
      case EVT_BRIDGE_DOWN:       return "bridge_down";
    }
    return "unknown";
  }

  // JSON-escape a string value into a quoted literal
  String jsonStr(const String& s) {
    String out;
    out.reserve(s.length() + 8);
    out += '"';
    for (size_t i = 0; i < s.length(); i++) {
      char c = s[i];
      switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
          if ((uint8_t)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
          } else {
            out += c;
          }
      }
    }
    out += '"';
    return out;
  }

  void push(EventType type, const String& ip, const String& detailJson) {
    Event& e = events[writeIdx];
    e.id = nextId++;
    e.ts = millis();
    e.type = type;
    e.ip = ip;
    // Cap detail to avoid runaway memory
    if (detailJson.length() > EVENT_DETAIL_MAX) {
      e.detail = detailJson.substring(0, EVENT_DETAIL_MAX);
    } else {
      e.detail = detailJson;
    }
    writeIdx = (writeIdx + 1) % EVENT_BUFFER_SIZE;
    if (count < EVENT_BUFFER_SIZE) count++;
  }

  // Build JSON array of events with id > sinceId (chronological)
  String toJSON(unsigned long sinceId) {
    String json = "[";
    bool first = true;
    // Oldest-to-newest iteration over the ring
    for (unsigned int i = 0; i < count; i++) {
      unsigned int idx = (writeIdx + EVENT_BUFFER_SIZE - count + i) % EVENT_BUFFER_SIZE;
      const Event& e = events[idx];
      if (e.id <= sinceId) continue;
      if (!first) json += ',';
      first = false;
      json += "{\"id\":";
      json += e.id;
      json += ",\"ts\":";
      json += e.ts;
      json += ",\"type\":\"";
      json += typeStr(e.type);
      json += "\",\"ip\":";
      json += jsonStr(e.ip);
      json += ",\"detail\":";
      json += (e.detail.length() > 0 ? e.detail : String("{}"));
      json += '}';
    }
    json += ']';
    return json;
  }

  unsigned long lastId() { return (nextId > 0) ? (nextId - 1) : 0; }
}

#endif
