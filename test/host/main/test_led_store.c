#include "unity.h"
#include "led_store.h"
#include "Mocknvs.h"

#define FAKE_HANDLE  ((nvs_handle_t)13)
#define MASK         (LED_CONFIG_GPIO_BIT(4) | LED_CONFIG_GPIO_BIT(10))

static const led_config_t s_defaults = {.gpio = 10, .count = 16};

static void expect_open_ok(void)
{
    static nvs_handle_t handle = FAKE_HANDLE;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
}

// nvs_get_u8 für key liefert *value bzw. den Fehler result
static void expect_get(const char *key, uint8_t *value, esp_err_t result)
{
    nvs_get_u8_ExpectAndReturn(FAKE_HANDLE, key, NULL, result);
    nvs_get_u8_IgnoreArg_out_value();
    if (result == ESP_OK) {
        nvs_get_u8_ReturnThruPtr_out_value(value);
    }
}

static void expect_load(uint8_t gpio, uint8_t count, esp_err_t result)
{
    static uint8_t s_gpio, s_count;
    s_gpio = gpio;
    s_count = count;
    expect_open_ok();
    expect_get("led_gpio", &s_gpio, result);
    expect_get("led_count", &s_count, result);
    nvs_close_Expect(FAKE_HANDLE);
}

static void test_load_stored_values(void)
{
    led_config_t cfg;
    expect_load(4, 25, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, led_store_load(&cfg, &s_defaults, MASK));
    TEST_ASSERT_EQUAL_UINT8(4, cfg.gpio);
    TEST_ASSERT_EQUAL_UINT8(25, cfg.count);
}

static void test_load_missing_gives_defaults(void)
{
    led_config_t cfg = {.gpio = 1, .count = 1};
    expect_load(0, 0, ESP_ERR_NVS_NOT_FOUND);
    TEST_ASSERT_EQUAL(ESP_OK, led_store_load(&cfg, &s_defaults, MASK));
    TEST_ASSERT_EQUAL_UINT8(10, cfg.gpio);
    TEST_ASSERT_EQUAL_UINT8(16, cfg.count);
}

static void test_load_invalid_gives_defaults(void)
{
    led_config_t cfg;
    // GPIO nicht in der Maske
    expect_load(5, 20, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, led_store_load(&cfg, &s_defaults, MASK));
    TEST_ASSERT_EQUAL_UINT8(10, cfg.gpio);
    TEST_ASSERT_EQUAL_UINT8(16, cfg.count);
    // Anzahl zu groß
    expect_load(4, 26, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, led_store_load(&cfg, &s_defaults, MASK));
    TEST_ASSERT_EQUAL_UINT8(10, cfg.gpio);
    TEST_ASSERT_EQUAL_UINT8(16, cfg.count);
}

static void test_load_nvs_error_gives_defaults(void)
{
    led_config_t cfg = {.gpio = 1, .count = 1};
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_INITIALIZED, led_store_load(&cfg, &s_defaults, MASK));
    TEST_ASSERT_EQUAL_UINT8(10, cfg.gpio);
    TEST_ASSERT_EQUAL_UINT8(16, cfg.count);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_store_load(NULL, &s_defaults, MASK));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_store_load(&cfg, NULL, MASK));
}

static void test_save_writes_values(void)
{
    led_config_t cfg = {.gpio = 4, .count = 20};
    expect_open_ok();
    nvs_set_u8_ExpectAndReturn(FAKE_HANDLE, "led_gpio", 4, ESP_OK);
    nvs_set_u8_ExpectAndReturn(FAKE_HANDLE, "led_count", 20, ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, led_store_save(&cfg, MASK));
}

static void test_save_rejects_invalid(void)
{
    led_config_t cfg = {.gpio = 5, .count = 20};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_store_save(&cfg, MASK));
    cfg = (led_config_t){.gpio = 4, .count = 15};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_store_save(&cfg, MASK));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_store_save(NULL, MASK));
}

static void test_save_reports_write_error(void)
{
    led_config_t cfg = {.gpio = 4, .count = 20};
    expect_open_ok();
    nvs_set_u8_ExpectAndReturn(FAKE_HANDLE, "led_gpio", 4, ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, led_store_save(&cfg, MASK));
}

// Wird von test_main.c aufgerufen
void run_led_store_tests(void)
{
    RUN_TEST(test_load_stored_values);
    RUN_TEST(test_load_missing_gives_defaults);
    RUN_TEST(test_load_invalid_gives_defaults);
    RUN_TEST(test_load_nvs_error_gives_defaults);
    RUN_TEST(test_save_writes_values);
    RUN_TEST(test_save_rejects_invalid);
    RUN_TEST(test_save_reports_write_error);
}
