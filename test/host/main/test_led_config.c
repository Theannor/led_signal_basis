#include "unity.h"
#include "led_config.h"

#define MASK  (LED_CONFIG_GPIO_BIT(0) | LED_CONFIG_GPIO_BIT(10) | LED_CONFIG_GPIO_BIT(63))

static void test_gpio_allowed_by_mask(void)
{
    TEST_ASSERT_TRUE(led_config_gpio_allowed(0, MASK));
    TEST_ASSERT_TRUE(led_config_gpio_allowed(10, MASK));
    TEST_ASSERT_TRUE(led_config_gpio_allowed(63, MASK));
    TEST_ASSERT_FALSE(led_config_gpio_allowed(9, MASK));
    TEST_ASSERT_FALSE(led_config_gpio_allowed(-1, MASK));
    TEST_ASSERT_FALSE(led_config_gpio_allowed(64, MASK));
    TEST_ASSERT_FALSE(led_config_gpio_allowed(10, 0));
}

static void test_count_limits(void)
{
    led_config_t cfg = {.gpio = 10, .count = LED_CONFIG_COUNT_MIN};
    TEST_ASSERT_TRUE(led_config_is_valid(&cfg, MASK));
    cfg.count = LED_CONFIG_COUNT_MAX;
    TEST_ASSERT_TRUE(led_config_is_valid(&cfg, MASK));
    cfg.count = LED_CONFIG_COUNT_MIN - 1;
    TEST_ASSERT_FALSE(led_config_is_valid(&cfg, MASK));
    cfg.count = LED_CONFIG_COUNT_MAX + 1;
    TEST_ASSERT_FALSE(led_config_is_valid(&cfg, MASK));
}

static void test_is_valid_checks_gpio_and_null(void)
{
    led_config_t cfg = {.gpio = 9, .count = 20};
    TEST_ASSERT_FALSE(led_config_is_valid(&cfg, MASK));
    TEST_ASSERT_FALSE(led_config_is_valid(NULL, MASK));
}

static void test_parse_uint_accepts_digits(void)
{
    unsigned v = 99;
    TEST_ASSERT_TRUE(led_config_parse_uint("0", 255, &v));
    TEST_ASSERT_EQUAL_UINT(0, v);
    TEST_ASSERT_TRUE(led_config_parse_uint("25", 255, &v));
    TEST_ASSERT_EQUAL_UINT(25, v);
    TEST_ASSERT_TRUE(led_config_parse_uint("255", 255, &v));
    TEST_ASSERT_EQUAL_UINT(255, v);
    TEST_ASSERT_TRUE(led_config_parse_uint("007", 255, &v));
    TEST_ASSERT_EQUAL_UINT(7, v);
}

static void test_parse_uint_rejects_invalid(void)
{
    unsigned v = 42;
    TEST_ASSERT_FALSE(led_config_parse_uint("", 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint("256", 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint("99999999999999999999", 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint("1a", 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint("-1", 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint(" 5", 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint("5", 3, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint(NULL, 255, &v));
    TEST_ASSERT_FALSE(led_config_parse_uint("5", 255, NULL));
    // Bei Fehlern bleibt der Wert unverändert
    TEST_ASSERT_EQUAL_UINT(42, v);
}

// Wird von test_main.c aufgerufen
void run_led_config_tests(void)
{
    RUN_TEST(test_gpio_allowed_by_mask);
    RUN_TEST(test_count_limits);
    RUN_TEST(test_is_valid_checks_gpio_and_null);
    RUN_TEST(test_parse_uint_accepts_digits);
    RUN_TEST(test_parse_uint_rejects_invalid);
}
