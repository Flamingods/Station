#pragma once
#include <stdint.h>
#include "Protocol.h"

#ifndef STATION_ID
#error "STATION_ID is not defined - build a station env, e.g. `pio run -e station1`"
#elif STATION_ID < 1 || STATION_ID > 4
#error "STATION_ID must be between 1 and 4"
#endif

#define STATION_STR_(x) #x
#define STATION_STR(x) STATION_STR_(x)
#define STATION_NAME "station-" STATION_STR(STATION_ID)

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

// Buttons, indexed by mask bit (see Protocol.h). Wired to GND, INPUT_PULLUP.
// Avoid GPIO 34-39 for buttons: they have no internal pull-up.
constexpr uint8_t BUTTON_PINS[] = {15, 32, 4, 12, 14};
static_assert(sizeof(BUTTON_PINS) == BUTTON_COUNT, "one pin per button");
constexpr uint32_t SAMPLE_INTERVAL_MS = 5;
constexpr uint32_t DEBOUNCE_MS = 50;

// Stream to the Flamingo. FLAMINGO_HOST may be a hostname or an IP literal;
// override with -DFLAMINGO_HOST=\"...\" (e.g. to point at tools/listen.py).
#ifndef FLAMINGO_HOST
#define FLAMINGO_HOST "flamingo-esp32.local"
#endif
constexpr uint16_t FLAMINGO_PORT = 5000;
constexpr uint32_t STREAM_INTERVAL_MS = 20;
constexpr uint8_t RELEASE_REPEATS = 3;
constexpr uint32_t FLAMINGO_RESOLVE_RETRY_MS = 5000;

// Network
#define WIFI_SSID "DiMax Residency 2.4Ghz"
#define WIFI_PASSWORD "33355555DM"
constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
constexpr uint32_t NETWORK_RETRY_MS = 10000;

// ESP32-ETH01 v1.4 (LAN8720A). Names avoid ETH.h's ETH_PHY_* macros.
constexpr uint8_t ETH01_PHY_ADDR = 1;
constexpr int ETH01_PHY_POWER_PIN = 16;
constexpr int ETH01_MDC_PIN = 23;
constexpr int ETH01_MDIO_PIN = 18;
constexpr int ETH01_POWER_ENABLE_PIN = 5;

// OTA
#define OTA_PASSWORD "flamingods2024"

// Web test page simulated hold
constexpr uint32_t SIM_DEFAULT_MS = 1000;
constexpr uint32_t SIM_MAX_MS = 10000;

// LED strip (WS2812B, GRB) mirroring the pressed buttons. Must be an output-capable
// pin (GPIO 34-39 are input-only).
constexpr uint8_t LED_PIN = 2;
constexpr uint16_t LED_COUNT = 30;  // set to the real strip length
constexpr uint8_t LED_BRIGHTNESS = 100;
constexpr uint32_t LED_FRAME_MS = 20;  // idle/rainbow refresh interval (50 fps)
constexpr uint32_t IDLE_FADE_MS = 1000;  // idle animation fade-in after release
