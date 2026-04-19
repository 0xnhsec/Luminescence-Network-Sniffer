# Luminescence Network Sniffer

> Rogue AP + HTTP Traffic Sniffer — Educational cybersecurity lab tool

**Author:** Bangkit Eldhianpranata ([0xnhsec](https://github.com/0xnhsec))

**Hardware:** ESP32 DevKit V1

**License:** GPL-3.0

**Dependencies:** None — uses only built-in ESP32 core libraries

## What It Does

ESP32 creates a fake WiFi access point with a captive-style login page.
When a device connects and submits data via HTTP forms, all plaintext
traffic is captured with full metadata: parsed User-Agent, headers,
timestamps, and request statistics.

**This demonstrates why HTTPS exists.**

> ⚠ Read **[SCOPE.md](SCOPE.md)** before flashing. This is a research
> instrument, not an attack tool. Authorized use only.

## Phase 5 — Before Flashing

Edit **`config.h`** and set the upstream (real) WiFi credentials:

```c
#define STA_SSID  "NamaWiFiRouterKamu"   // ← bukan LabTestAP!
#define STA_PASS  "PasswordRouterKamu"
```

> ⚠ Jangan isi `STA_SSID` dengan `"LabTestAP"` — itu SSID AP kamu sendiri.
> ESP32 akan mencoba connect ke dirinya sendiri dan bridge tidak akan berjalan.
> Set `BRIDGE_MODE 0` di `config.h` jika tidak ingin pakai bridge.

---

## Build & Flash (fish — ESP32 Arduino Core 3.x)

> **Note:** `arduino-cli` versi baru tidak mendukung upload SPIFFS langsung.
> Gunakan `mkspiffs` + `esptool` yang sudah include di Arduino15 packages.

```fish
# One-time: add user to dialout
sudo usermod -aG dialout $USER
# lalu logout/login, atau: newgrp dialout

# One-time: install ESP32 core
arduino-cli core install esp32:esp32

# --- Tool paths (ESP32 Core 3.3.8) ---
set MKSPIFFS  ~/.arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs
set ESPTOOL   ~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool
set PORT      /dev/ttyUSB0
set FQBN      esp32:esp32:esp32

# 1. Compile firmware
arduino-cli compile --fqbn $FQBN .

# 2. Upload firmware (hold BOOT saat "Connecting..." muncul)
arduino-cli upload -p $PORT --fqbn $FQBN .

# 3. Buat SPIFFS image dari folder data/
#    Partition default: offset 0x290000, size 0x160000 (1.375 MB)
$MKSPIFFS -c data -b 4096 -p 256 -s 0x160000 /tmp/spiffs.bin

# 4. Flash SPIFFS image
$ESPTOOL --chip esp32 --port $PORT --baud 921600 \
    write_flash 0x290000 /tmp/spiffs.bin

# 5. Monitor serial
arduino-cli monitor -p $PORT --config baudrate=115200
```

### Fish helpers (optional)

Simpan di `~/.config/fish/functions/`:

```fish
function esp-spiffs --description 'Build and flash SPIFFS image'
    set -l mkspiffs ~/.arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs
    set -l esptool  ~/.arduino15/packages/esp32/tools/esptool_py/5.2.0/esptool
    $mkspiffs -c data -b 4096 -p 256 -s 0x160000 /tmp/spiffs.bin \
      and $esptool --chip esp32 --port /dev/ttyUSB0 --baud 921600 \
            write_flash 0x290000 /tmp/spiffs.bin \
      and echo "✓ SPIFFS flashed"
end

function esp-flash --description 'Compile + upload firmware'
    arduino-cli compile --fqbn esp32:esp32:esp32 . \
      and arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32 . \
      and echo "✓ firmware flashed"
end

function esp-mon --description 'Serial monitor at 115200'
    arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200
end

function esp-go --description 'Full flash (firmware + SPIFFS) then monitor'
    esp-flash; and esp-spiffs; and esp-mon
end
```

```fish
funcsave esp-spiffs esp-flash esp-mon esp-go
```

Setelah itu cukup jalankan `esp-go` untuk compile, flash firmware, flash SPIFFS, lalu monitor.

> **Note:** Jika SPIFFS upload gagal, firmware otomatis fallback ke inline HTML.
> Masih bisa dipakai, tampilan saja yang lebih sederhana.

## Test

1. Serial Monitor shows banner + AP info
2. Phone → WiFi → connect **"LabTestAP"** (password: `lab12345`)
3. Browser → `http://192.168.4.1`
4. Submit login + biodata form with **dummy data** (see SCOPE.md)
5. Watch captured data in Serial Monitor
6. **Phase 4:** From a second device, open `http://192.168.4.1/dashboard` for the live feed
7. **Phase 5:** Victims get real internet after submitting the form (bridge is active when `Bridge: UP` shows in the dashboard)

### Smoke-test the API from fish

```fish
# Should return 200 + JSON with stats + events (empty events array if no activity yet)
curl -s http://192.168.4.1/api/poll?since=0 | jq

# Phase 5: bridge status snapshot
curl -s http://192.168.4.1/api/bridge | jq

# Legacy endpoints still work (kept for backward compat + curl debugging)
curl -s http://192.168.4.1/api/stats | jq
curl -s http://192.168.4.1/api/events?since=0 | jq
```

## Project Structure

```
esp32-netcapture-lab/
├── esp32-netcapture-lab.ino   # Main firmware
├── config.h                    # WiFi & server constants (set STA_SSID/STA_PASS here)
├── logger.h                    # Enhanced serial logger + stats + event feed
├── event_buffer.h              # Ring buffer + JSON serializer
├── ua_parser.h                 # User-Agent parser (OS/device/browser)
├── data/                       # SPIFFS web pages
│   ├── login.html
│   ├── biodata.html
│   ├── success.html
│   ├── dashboard.html          # Live dashboard (Phase 5 — shows bridge status)
│   └── style.css
├── SCOPE.md                    # Rules of engagement + rules of usage
└── README.md
```

## Features ( EDUCATIONAL SECUIRTY PURPOSE ONLY )

- **WiFi Bridge Mode** — ESP32 runs as AP+STA simultaneously (`WIFI_AP_STA`)
- **NAPT / Internet Access** — `ip_napt_enable()` makes AP clients route through the upstream WiFi; victims get real internet (less suspicious)
- **Auto-Reconnect** — If upstream drops, STA retries every `STA_RETRY_MS` (default 30 s); AP stays up throughout
- **Bridge Status API** — `GET /api/bridge` returns `{enabled, active, ssid, sta_ip, ap_ip}`
- **Dashboard Bridge Widget** — Stat card shows Bridge UP/DOWN + STA IP in real time; `bridge_up` / `bridge_down` events appear in the live feed
- **Graceful Degradation** — Set `BRIDGE_MODE 0` in `config.h` to revert to AP-only (Phase 4 behaviour) without touching any other code
- **Requires** ESP32 Arduino Core ≥ 3.x (uses `esp_netif_napt_enable()` from IDF 5 — tested on 3.3.8)
- **Live Dashboard** — Real-time web UI at `http://192.168.4.1/dashboard`
- **Event Ring Buffer** — Last 50 events (connects, requests, captures) in RAM
- **Merged JSON API** — `/api/poll?since=<id>` returns `{stats, events}` in one request
- **Legacy endpoints** — `/api/events?since=<id>` and `/api/stats` retained for curl debugging
- **Adaptive polling** — 1s when healthy, exponential backoff (1→2→4→8→16→30s) on failure, paused when tab hidden
- **Zero Dependencies** — HTTP polling over built-in WebServer, no external library
- **Dashboard Excluded** — `/dashboard` + `/api/*` don't trigger captive-portal redirect and aren't logged into the feed

> **Design note:** This phase uses HTTP polling instead of WebSockets to
> preserve the project's zero-dependency policy. The merged `/api/poll`
> endpoint + adaptive delay + visibility pause keeps the load on ESP32
> proportional to actual usage — idle tab = zero traffic.

- **DNS Captive Portal** — All domains resolve to `192.168.4.1`
- **Auto Popup** — Android, iOS, Windows, Firefox portal probes all trigger the "Sign in to network" popup on connect
- **SPIFFS** — HTML/CSS served from flash storage, editable without recompile
- **UA Parsing** — Detects OS (Android 14), device (Samsung A04s), browser (Chrome 136)
- **Full Header Logging** — User-Agent, Referer, Content-Type, Accept-Language, Cookie
- **Client MAC Logging** — Shows MAC address on connect/disconnect events
- **Request Statistics** — Running count of GET/POST requests and captured credentials
- **Email Field** — Added to biodata form
- **Fallback HTML** — Works even if SPIFFS is empty



