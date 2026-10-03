#include <unity.h>
#include "IdleAnimation.h"

void setUp() {}
void tearDown() {}

static const uint16_t COUNT = 30;

static bool samePixel(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static bool sameFrame(uint8_t stationA, uint8_t stationB, uint32_t tA, uint32_t tB) {
    for (uint16_t i = 0; i < COUNT; i++) {
        if (!samePixel(idlePixel(stationA, i, COUNT, tA), idlePixel(stationB, i, COUNT, tB))) return false;
    }
    return true;
}

// Calls check(pixel) for every station pixel over 20 s of animation.
template <typename F>
static void forEachPixel(uint8_t station, F check) {
    for (uint32_t t = 0; t < 20000; t += 37) {
        for (uint16_t i = 0; i < COUNT; i++) check(idlePixel(station, i, COUNT, t));
    }
}

static uint8_t max2(uint8_t a, uint8_t b) { return a > b ? a : b; }

void test_each_station_has_its_own_animation() {
    for (uint8_t a = 1; a <= 4; a++) {
        for (uint8_t b = a + 1; b <= 4; b++) {
            TEST_ASSERT_FALSE_MESSAGE(sameFrame(a, b, 5000, 5000), "two stations share an animation");
        }
    }
}

void test_idle_stays_under_brightness_cap() {
    for (uint8_t s = 1; s <= 4; s++) {
        forEachPixel(s, [](Rgb c) {
            TEST_ASSERT_TRUE(c.r <= IDLE_MAX_LEVEL && c.g <= IDLE_MAX_LEVEL && c.b <= IDLE_MAX_LEVEL);
        });
    }
}

void test_station1_embers_red_to_orange() {
    forEachPixel(1, [](Rgb c) {
        TEST_ASSERT_EQUAL_UINT8(0, c.b);
        TEST_ASSERT_TRUE(c.g <= c.r);
        TEST_ASSERT_TRUE(c.r > 0);  // embers never go fully dark
    });
}

void test_station2_tide_blue_to_cyan() {
    forEachPixel(2, [](Rgb c) {
        TEST_ASSERT_EQUAL_UINT8(0, c.r);
        TEST_ASSERT_TRUE(c.b >= c.g);
        TEST_ASSERT_TRUE(c.b > 0);
    });
}

void test_station3_aurora_green_teal_purple() {
    forEachPixel(3, [](Rgb c) { TEST_ASSERT_TRUE(c.r <= max2(c.g, c.b)); });
}

void test_station4_pulse_magenta_or_cyan_comet() {
    forEachPixel(4, [](Rgb c) {
        TEST_ASSERT_FALSE(c.r > 0 && c.g > 0);  // never mixes magenta and cyan
        TEST_ASSERT_TRUE(c.b >= max2(c.r, c.g));
    });
    int lit = 0;
    for (uint16_t i = 0; i < COUNT; i++) {
        Rgb c = idlePixel(4, i, COUNT, 5000);
        if (c.r || c.g || c.b) lit++;
    }
    TEST_ASSERT_TRUE(lit > 0 && lit < COUNT);  // a comet, not a full strip
}

void test_every_station_animates() {
    for (uint8_t s = 1; s <= 4; s++) {
        TEST_ASSERT_FALSE_MESSAGE(sameFrame(s, s, 1000, 3000), "idle animation is static");
    }
}

void test_keeps_moving_across_millis_wraparound() {
    for (uint8_t s = 1; s <= 4; s++) {
        TEST_ASSERT_FALSE(sameFrame(s, s, 0xFFFFFF00u, 0x00000100u));
    }
}

void test_unknown_station_falls_back_to_glow() {
    TEST_ASSERT_TRUE(samePixel(LED_IDLE, idlePixel(0, 3, COUNT, 1234)));
    TEST_ASSERT_TRUE(samePixel(LED_IDLE, idlePixel(9, 3, COUNT, 1234)));
}

void test_zero_length_strip_is_safe() {
    for (uint8_t s = 1; s <= 4; s++) {
        Rgb c = idlePixel(s, 0, 0, 1000);
        TEST_ASSERT_TRUE(c.r <= IDLE_MAX_LEVEL && c.g <= IDLE_MAX_LEVEL && c.b <= IDLE_MAX_LEVEL);
    }
}

void test_fade_in_after_release() {
    TEST_ASSERT_EQUAL_UINT8(0, idleFade(0, 1000));
    TEST_ASSERT_EQUAL_UINT8(127, idleFade(500, 1000));
    TEST_ASSERT_EQUAL_UINT8(255, idleFade(1000, 1000));
    TEST_ASSERT_EQUAL_UINT8(255, idleFade(5000, 1000));
    TEST_ASSERT_EQUAL_UINT8(255, idleFade(0, 0));
}

void test_scale_rgb() {
    Rgb c = scaleRgb(Rgb{255, 100, 0}, 128);
    TEST_ASSERT_EQUAL_UINT8(128, c.r);
    TEST_ASSERT_EQUAL_UINT8(50, c.g);
    TEST_ASSERT_EQUAL_UINT8(0, c.b);
    TEST_ASSERT_TRUE(samePixel(Rgb{0, 0, 0}, scaleRgb(Rgb{255, 255, 255}, 0)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_each_station_has_its_own_animation);
    RUN_TEST(test_idle_stays_under_brightness_cap);
    RUN_TEST(test_station1_embers_red_to_orange);
    RUN_TEST(test_station2_tide_blue_to_cyan);
    RUN_TEST(test_station3_aurora_green_teal_purple);
    RUN_TEST(test_station4_pulse_magenta_or_cyan_comet);
    RUN_TEST(test_every_station_animates);
    RUN_TEST(test_keeps_moving_across_millis_wraparound);
    RUN_TEST(test_unknown_station_falls_back_to_glow);
    RUN_TEST(test_zero_length_strip_is_safe);
    RUN_TEST(test_fade_in_after_release);
    RUN_TEST(test_scale_rgb);
    return UNITY_END();
}
