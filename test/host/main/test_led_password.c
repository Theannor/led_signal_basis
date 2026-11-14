// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "led_password.h"

// Feste "Zufallsfolge" 0, 1, 2, 3, ... für reproduzierbare Tests
static uint32_t s_next;
static uint32_t fake_rand(void)
{
    return s_next++;
}

static void test_generate_follows_rng(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    s_next = 0;
    led_password_generate(pw, fake_rand);
    TEST_ASSERT_EQUAL_STRING("rgbrgbrgbrgbrgbr", pw);
}

static void test_generate_gives_valid_password(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    s_next = 12345;
    led_password_generate(pw, fake_rand);
    TEST_ASSERT_EQUAL_size_t(LED_PASSWORD_LEN, strlen(pw));
    TEST_ASSERT_TRUE(led_password_is_valid(pw));
}

static void test_is_valid_rejects_bad_input(void)
{
    TEST_ASSERT_FALSE(led_password_is_valid(NULL));
    TEST_ASSERT_FALSE(led_password_is_valid(""));
    TEST_ASSERT_FALSE(led_password_is_valid("rgbrgbrgbrgbrgb"));    // 15
    TEST_ASSERT_FALSE(led_password_is_valid("rgbrgbrgbrgbrgbrg"));  // 17
    TEST_ASSERT_FALSE(led_password_is_valid("rgbrgbrgbrgbrgbX"));
    TEST_ASSERT_FALSE(led_password_is_valid("RGBRGBRGBRGBRGBR"));
    TEST_ASSERT_TRUE(led_password_is_valid("bbbbbbbbbbbbbbbb"));
}

static void test_frame_maps_letters_to_colors(void)
{
    led_color_t out[LED_PASSWORD_LEN];
    led_password_frame("rgbrgbrgbrgbrgbr", LED_PASSWORD_LEN, 255, out);
    TEST_ASSERT_EQUAL_UINT8(255, out[0].r);
    TEST_ASSERT_EQUAL_UINT8(0, out[0].g);
    TEST_ASSERT_EQUAL_UINT8(255, out[1].g);
    TEST_ASSERT_EQUAL_UINT8(0, out[1].r);
    TEST_ASSERT_EQUAL_UINT8(255, out[2].b);
    TEST_ASSERT_EQUAL_UINT8(0, out[2].g);
}

static void test_frame_highlight_is_white(void)
{
    led_color_t out[LED_PASSWORD_LEN];
    led_password_frame("rrrrrrrrrrrrrrrr", 5, 255, out);
    TEST_ASSERT_EQUAL_UINT8(255, out[5].r);
    TEST_ASSERT_EQUAL_UINT8(255, out[5].g);
    TEST_ASSERT_EQUAL_UINT8(255, out[5].b);
    // Nachbarn behalten ihre Farbe
    TEST_ASSERT_EQUAL_UINT8(0, out[4].g);
    TEST_ASSERT_EQUAL_UINT8(0, out[6].g);
}

static void test_frame_respects_brightness(void)
{
    led_color_t out[LED_PASSWORD_LEN];
    led_password_frame("rgbrgbrgbrgbrgbr", 0, 25, out);
    TEST_ASSERT_EQUAL_UINT8(25, out[0].g);  // weiß, skaliert
    for (int i = 0; i < LED_PASSWORD_LEN; i++) {
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(25, out[i].r);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(25, out[i].g);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(25, out[i].b);
    }
}

static void test_frame_without_highlight_has_no_white(void)
{
    led_color_t out[LED_PASSWORD_LEN];
    led_password_frame("gggggggggggggggg", 99, 255, out);
    for (int i = 0; i < LED_PASSWORD_LEN; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, out[i].r);
        TEST_ASSERT_EQUAL_UINT8(255, out[i].g);
    }
}

// Wird von test_main.c aufgerufen
void run_led_password_tests(void)
{
    RUN_TEST(test_generate_follows_rng);
    RUN_TEST(test_generate_gives_valid_password);
    RUN_TEST(test_is_valid_rejects_bad_input);
    RUN_TEST(test_frame_maps_letters_to_colors);
    RUN_TEST(test_frame_highlight_is_white);
    RUN_TEST(test_frame_respects_brightness);
    RUN_TEST(test_frame_without_highlight_has_no_white);
}
