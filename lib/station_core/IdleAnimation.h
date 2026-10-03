#pragma once
#include <stdint.h>
#include "LedColor.h"

// Idle animations stay dim (~40%) so they invite play without competing with
// the button colors.
constexpr uint8_t IDLE_MAX_LEVEL = 102;

// Pixel `index` of the station's idle animation at nowMs, already capped to
// IDLE_MAX_LEVEL. Each station echoes its Flamingo special mode:
//   1 Embers (red/orange flicker)   2 Tide (blue/cyan waves)
//   3 Aurora (green/teal/purple)    4 Pulse (magenta/cyan comet)
// Unknown station ids get LED_IDLE. Deterministic (no RNG) so it is testable.
Rgb idlePixel(uint8_t stationId, uint16_t index, uint16_t count, uint32_t nowMs);

// Fade-in level (0..255) sinceMs after the buttons were released.
uint8_t idleFade(uint32_t sinceMs, uint32_t fadeMs);

// Scales every channel by level/255.
Rgb scaleRgb(Rgb c, uint8_t level);
