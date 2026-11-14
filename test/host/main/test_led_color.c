// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include "unity.h"
#include "led_color.h"

// Prüft alle drei Kanäle auf einmal
static void assert_color(uint8_t r, uint8_t g, uint8_t b, led_color_t actual)
{
    TEST_ASSERT_EQUAL_UINT8(r, actual.r);
    TEST_ASSERT_EQUAL_UINT8(g, actual.g);
    TEST_ASSERT_EQUAL_UINT8(b, actual.b);
}

static void test_scale_zero_gives_black(void)
{
    led_color_t c = {.r = 255, .g = 128, .b = 1};
    assert_color(0, 0, 0, led_color_scale(c, 0));
}

static void test_scale_full_keeps_color(void)
{
    led_color_t c = {.r = 255, .g = 128, .b = 1};
    assert_color(255, 128, 1, led_color_scale(c, 255));
}

static void test_scale_white_to_max_brightness(void)
{
    led_color_t c = {.r = 255, .g = 255, .b = 255};
    assert_color(25, 25, 25, led_color_scale(c, 25));
}

static void test_scale_rounds_mixed_values(void)
{
    // 200*128/255 = 100.39 -> 100; 100*128/255 = 50.2 -> 50; 1*128/255 = 0.502 -> 1
    led_color_t c = {.r = 200, .g = 100, .b = 1};
    assert_color(100, 50, 1, led_color_scale(c, 128));
}

static void test_hue_primary_colors(void)
{
    assert_color(255, 0, 0, led_color_from_hue(0));
    assert_color(0, 255, 0, led_color_from_hue(120));
    assert_color(0, 0, 255, led_color_from_hue(240));
}

static void test_hue_60_is_yellow(void)
{
    assert_color(255, 255, 0, led_color_from_hue(60));
}

static void test_hue_wraps_at_360(void)
{
    assert_color(255, 0, 0, led_color_from_hue(360));
    led_color_t a = led_color_from_hue(30);
    led_color_t b = led_color_from_hue(390);
    assert_color(a.r, a.g, a.b, b);
}

static void test_rainbow_first_led_has_offset_hue(void)
{
    led_color_t colors[16];
    led_rainbow_fill(colors, 16, 120, 255);
    assert_color(0, 255, 0, colors[0]);
}

static void test_rainbow_respects_brightness(void)
{
    led_color_t colors[16];
    led_rainbow_fill(colors, 16, 0, 25);
    for (int i = 0; i < 16; i++) {
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(25, colors[i].r);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(25, colors[i].g);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(25, colors[i].b);
    }
}

static void test_rainbow_hue_steps_across_strip(void)
{
    // 16 LEDs: Abstand 22 bzw. 23 Grad, LED 8 liegt bei 180 Grad (Cyan)
    led_color_t colors[16];
    led_rainbow_fill(colors, 16, 0, 255);
    led_color_t expected1 = led_color_from_hue(22);
    assert_color(expected1.r, expected1.g, expected1.b, colors[1]);
    assert_color(0, 255, 255, colors[8]);
}

static void test_rainbow_offset_shifts_pattern(void)
{
    // Bei 12 LEDs (30 Grad Abstand) verschiebt offset 30 genau um eine LED
    led_color_t base[12];
    led_color_t shifted[12];
    led_rainbow_fill(base, 12, 0, 255);
    led_rainbow_fill(shifted, 12, 30, 255);
    for (int i = 0; i < 11; i++) {
        assert_color(base[i + 1].r, base[i + 1].g, base[i + 1].b, shifted[i]);
    }
    assert_color(base[0].r, base[0].g, base[0].b, shifted[11]);
}

static void test_rainbow_ignores_empty_input(void)
{
    led_color_t c = {.r = 7, .g = 7, .b = 7};
    led_rainbow_fill(NULL, 16, 0, 255);
    led_rainbow_fill(&c, 0, 0, 255);
    assert_color(7, 7, 7, c);
}

// Wird von test_main.c aufgerufen
void run_led_color_tests(void)
{
    RUN_TEST(test_scale_zero_gives_black);
    RUN_TEST(test_scale_full_keeps_color);
    RUN_TEST(test_scale_white_to_max_brightness);
    RUN_TEST(test_scale_rounds_mixed_values);
    RUN_TEST(test_hue_primary_colors);
    RUN_TEST(test_hue_60_is_yellow);
    RUN_TEST(test_hue_wraps_at_360);
    RUN_TEST(test_rainbow_first_led_has_offset_hue);
    RUN_TEST(test_rainbow_respects_brightness);
    RUN_TEST(test_rainbow_hue_steps_across_strip);
    RUN_TEST(test_rainbow_offset_shifts_pattern);
    RUN_TEST(test_rainbow_ignores_empty_input);
}
