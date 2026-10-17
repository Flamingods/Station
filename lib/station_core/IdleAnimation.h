#pragma once
#include <stdint.h>
#include "LedColor.h"

// The idle strip lures people to the buttons: a dim themed ambient that, every
// INVITE_PERIOD_MS, swells up, chases the five button colors along the strip and
// bursts into them before settling back.
constexpr uint8_t IDLE_AMBIENT_LEVEL = 115;  // ~45%: ambient never competes with a press
constexpr uint8_t IDLE_MAX_LEVEL = 205;      // ~80%: invitation peak

// One invitation cycle: ambient, swell, chase, burst back to ambient.
constexpr uint32_t INVITE_PERIOD_MS = 8000;
constexpr uint32_t INVITE_SWELL_START_MS = 4200;
constexpr uint32_t INVITE_CHASE_START_MS = 5200;
constexpr uint32_t INVITE_BURST_START_MS = 7300;

// Pixel `index` of the station's idle animation at nowMs. Each station echoes its
// Flamingo special mode:
//   1 Fire (flames from both ends)    2 Ocean (swells with foam)
//   3 Northern Lights (curtains, stars)  4 Electric (comets, sparks)
// The chase runs the other way every other cycle and the burst's color order
// rotates, so consecutive invitations differ. Unknown station ids get LED_IDLE.
// Deterministic (no RNG) so it is testable.
Rgb idlePixel(uint8_t stationId, uint16_t index, uint16_t count, uint32_t nowMs);

// Fade-in level (0..255) sinceMs after the buttons were released.
uint8_t idleFade(uint32_t sinceMs, uint32_t fadeMs);

// Scales every channel by level/255.
Rgb scaleRgb(Rgb c, uint8_t level);
