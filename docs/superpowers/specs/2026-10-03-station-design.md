# Station ESP32 — Design

Date: 2026-10-03
Reference: `~/gits/Flamingods/esps/station` (previous firmware), `~/gits/Flamingods/esps/flamingo` (server)

## Goal

New firmware for the Flamingods Station: an ESP32 button panel that detects which
button(s) are pressed and streams that state to the Flamingo ESP over UDP. Clean
rewrite of the reference with the **same wire protocol**, so the Flamingo needs no
changes.

## Hardware (unchanged from reference)

- Board: ESP32-ETH01 v1.4 (LAN8720A PHY: addr 1, power GPIO 16, MDC 23, MDIO 18,
  clock GPIO0_IN; PHY power enable on GPIO 5).
- 5 momentary buttons, each between a GPIO and GND, `INPUT_PULLUP` (pressed = LOW).

| Button | GPIO | Mask bit |
|--------|------|----------|
| Red    | 15   | 0 (0x01) |
| Green  | 32   | 1 (0x02) |
| Blue   | 4    | 2 (0x04) |
| Yellow | 12   | 3 (0x08) |
| White  | 14   | 4 (0x10) |

Pins are taken from the reference `src/main.cpp` (the reference docs are stale).
Bit order matches the Flamingo's `getBlendedColorFromPackets()`.

## Protocol (must stay compatible)

- Destination: `flamingo-esp32.local`, UDP port 5000.
- Packet: 2 bytes `[station_id (1..4), button_mask]`.
- Mask `0x1F` (all five) triggers the station's special mode on the Flamingo.

## Stream behavior

- Mask non-zero: send immediately on any mask change, otherwise every 20 ms.
- Mask transitions to zero: send `0` three times, 20 ms apart, then go silent
  (redundancy against UDP loss; reference sent once).
- Idle (mask zero, release burst done): send nothing (Flamingo treats `0` streams
  as flicker sources).

## Debounce

Per-button 50 ms stable-state debounce, sampled every ~5 ms in `loop()`.

## Modules

```
platformio.ini        envs station1..station4 (USB) and station1-ota..station4-ota;
                      each sets -DSTATION_ID=N. Env `native` for unit tests.
src/config.h          pins, host/port, timings, WiFi creds, OTA password, STATION_NAME
                      derived from STATION_ID ("station-N"); #error if STATION_ID missing
                      or outside 1..4.
lib/station_core/     framework-free logic (testable on host):
  Debouncer.h/.cpp    update(rawPressedBits, nowMs) -> stable mask
  Streamer.h/.cpp     tick(mask, nowMs) -> bool shouldSend (+ value); implements the
                      stream behavior above
src/network.h/.cpp    ETH first (10 s), WiFi fallback (10 s); runtime: if both down,
                      retry every 10 s without blocking loop; mDNS station-N.local;
                      resolve Flamingo IP, re-resolve every 5 s while unresolved and
                      every 60 s once known (keeps last address on failure); ETH lost
                      DHCP lease counts as ETH down.
src/web.h/.cpp        GET / test page; GET /status JSON; POST /press?mask=0xNN&ms=1000
                      (simulated hold, default 1000 ms, max 10000).
src/ota.h/.cpp        ArduinoOTA, hostname station-N, password from config.
src/main.cpp          setup/loop wiring only.
tools/listen.py       fake Flamingo: binds UDP 5000, prints station/mask/colors.
```

The effective mask sent = physical mask OR simulated mask (while a web hold is active).

## /status JSON

`station_id`, `station_name`, `firmware_version`, `network_type`
(ethernet|wifi|disconnected), `ip`, `mac`, `rssi` (wifi only), `uptime_s`,
`current_mask`, `press_count`, `packets_sent`, `flamingo_host`, `flamingo_ip`,
`flamingo_port`.

## Error handling

- No network or unresolved Flamingo: skip sends silently, keep reading buttons.
- Boot never blocks more than ~10 s per interface.
- OTA in progress: the upload blocks the loop, so streaming stops; a release (mask 0)
  is sent at OTA start and the LEDs show a blue progress bar.

## Testing

- `pio test -e native`: Debouncer (bounce rejection, 50 ms threshold, multi-button)
  and Streamer (20 ms cadence, immediate send on change, 3× release burst, silence
  when idle).
- `pio run`: all station envs build.
- Manual: `python3 tools/listen.py`, point station at it, press buttons.

## LED strip (added 2026-10-03)

- WS2812B strip, data on **GPIO 2** (pins ran out; green button moved from 2 to 32). `LED_COUNT` in `config.h` (default 30 until the real count is known),
  `LED_BRIGHTNESS` 100/255, GRB order.
- Mirrors the effective mask (physical | web-simulated) locally, independent of the network:
  - idle (mask 0): per-station animation at ≤40% (`IDLE_MAX_LEVEL` 102), 50 fps, fading
    in over 1 s after release: 1 Embers (red/orange flicker), 2 Tide (blue/cyan waves),
    3 Aurora (green/teal/purple + shimmer), 4 Pulse (magenta/cyan comet, 8-LED tail).
    Logic in `lib/station_core/IdleAnimation.*` (host-tested).
  - other masks: whole strip = the Flamingo's `getBlendedColorFromPackets()` blend:
    average of pressed red 255,0,0 / green 0,255,0 / blue 0,0,255 / yellow 180,180,0;
    white alone = 191,191,191; white with colors lightens the average to
    70% color + 30% white (255)
  - all five (0x1F): moving rainbow while held
- Color math lives in `lib/station_core/LedColor.*` (host-tested); FastLED driving in
  `src/leds.*`, refreshed at most every 20 ms.

## Out of scope

Changes to the Flamingo, new transports (ESP-NOW), more than 4 stations, HTTP JSON
endpoints from the reference (deprecated by the Flamingo).
