#ifndef UA_PARSER_H
#define UA_PARSER_H

#include <Arduino.h>

// ============================================
//  User-Agent Parser
//  Extracts: OS, Device, Browser from raw UA
// ============================================

struct ParsedUA {
  String os;
  String device;
  String browser;
  String summary;  // "Android 14 | Samsung A04s | Samsung Browser 29"
};

namespace UAParser {

  String extractBetween(const String& str, const String& start, const String& end) {
    int s = str.indexOf(start);
    if (s == -1) return "";
    s += start.length();
    int e = str.indexOf(end, s);
    if (e == -1) return str.substring(s);
    return str.substring(s, e);
  }

  ParsedUA parse(const String& ua) {
    ParsedUA result;
    result.os = "Unknown";
    result.device = "Unknown";
    result.browser = "Unknown";

    if (ua.length() == 0) {
      result.summary = "Empty UA";
      return result;
    }

    // --- Detect OS ---
    if (ua.indexOf("Android") >= 0) {
      String ver = extractBetween(ua, "Android ", ";");
      if (ver.length() == 0) ver = extractBetween(ua, "Android ", ")");
      result.os = "Android " + ver;
    }
    else if (ua.indexOf("iPhone") >= 0) {
      result.os = "iOS";
      String ver = extractBetween(ua, "iPhone OS ", " ");
      if (ver.length() > 0) {
        ver.replace("_", ".");
        result.os = "iOS " + ver;
      }
    }
    else if (ua.indexOf("iPad") >= 0) {
      result.os = "iPadOS";
    }
    else if (ua.indexOf("Windows NT 10") >= 0) {
      result.os = "Windows 10/11";
    }
    else if (ua.indexOf("Windows") >= 0) {
      result.os = "Windows";
    }
    else if (ua.indexOf("Mac OS X") >= 0) {
      result.os = "macOS";
    }
    else if (ua.indexOf("Linux") >= 0) {
      result.os = "Linux";
    }
    else if (ua.indexOf("Dalvik") >= 0) {
      result.os = "Android (system)";
    }

    // --- Detect Device ---
    if (ua.indexOf("SM-") >= 0) {
      result.device = "Samsung " + extractBetween(ua, "SM-", " ");
      if (result.device.endsWith(")")) {
        result.device = result.device.substring(0, result.device.length() - 1);
      }
    }
    else if (ua.indexOf("Pixel") >= 0) {
      result.device = "Google " + extractBetween(ua, "Pixel", ";");
      result.device = "Google Pixel" + extractBetween(ua, "Pixel", ";");
    }
    else if (ua.indexOf("Redmi") >= 0 || ua.indexOf("POCO") >= 0 || ua.indexOf("Mi ") >= 0) {
      result.device = "Xiaomi";
    }
    else if (ua.indexOf("OPPO") >= 0 || ua.indexOf("CPH") >= 0) {
      result.device = "OPPO";
    }
    else if (ua.indexOf("vivo") >= 0) {
      result.device = "Vivo";
    }
    else if (ua.indexOf("iPhone") >= 0) {
      result.device = "iPhone";
    }
    else if (ua.indexOf("iPad") >= 0) {
      result.device = "iPad";
    }

    // --- Detect Browser ---
    if (ua.indexOf("SamsungBrowser") >= 0) {
      result.browser = "Samsung Browser " + extractBetween(ua, "SamsungBrowser/", " ");
    }
    else if (ua.indexOf("Firefox") >= 0 && ua.indexOf("Seamonkey") < 0) {
      result.browser = "Firefox " + extractBetween(ua, "Firefox/", " ");
    }
    else if (ua.indexOf("OPR") >= 0 || ua.indexOf("Opera") >= 0) {
      result.browser = "Opera";
    }
    else if (ua.indexOf("Edg") >= 0) {
      result.browser = "Edge " + extractBetween(ua, "Edg/", " ");
    }
    else if (ua.indexOf("Chrome") >= 0 && ua.indexOf("Chromium") < 0) {
      result.browser = "Chrome " + extractBetween(ua, "Chrome/", " ");
    }
    else if (ua.indexOf("Safari") >= 0) {
      result.browser = "Safari";
    }
    else if (ua.indexOf("Dalvik") >= 0) {
      result.browser = "Android System";
    }

    // --- Build summary ---
    result.summary = result.os + " | " + result.device + " | " + result.browser;

    return result;
  }
}

#endif
