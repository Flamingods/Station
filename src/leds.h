#pragma once
#include <stdint.h>

// Starts the idle animation; call once in setup().
void leds_begin();
// Call every loop with the effective mask: station idle animation, solid blend,
// or rainbow for all five.
void leds_update(uint8_t mask, uint32_t nowMs);
