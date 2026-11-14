#include "unity.h"
#include "led_output.h"
#include "Mockled_strip.h"
#include "Mockled_strip_spi.h"

// Platzhalter-Handle; der Mock greift nie darauf zu
static int s_dummy_strip;
#define FAKE_HANDLE ((led_strip_handle_t)&s_dummy_strip)

// Vom Stub mitgeschriebene Konfiguration
static led_strip_config_t s_seen_strip_config;
static led_strip_spi_config_t s_seen_spi_config;

static esp_err_t new_spi_device_stub(const led_strip_config_t *led_config,
                                     const led_strip_spi_config_t *spi_config,
                                     led_strip_handle_t *ret_strip, int cmock_num_calls)
{
    (void)cmock_num_calls;
    s_seen_strip_config = *led_config;
    s_seen_spi_config = *spi_config;
    *ret_strip = FAKE_HANDLE;
    return ESP_OK;
}

// Hilfsfunktion: led_output erfolgreich mit count LEDs initialisieren
static void init_ok(size_t count)
{
    led_strip_new_spi_device_Stub(new_spi_device_stub);
    TEST_ASSERT_EQUAL(ESP_OK, led_output_init(10, count));
}

static void test_init_passes_strip_config(void)
{
    init_ok(16);
    TEST_ASSERT_EQUAL_INT(10, s_seen_strip_config.strip_gpio_num);
    TEST_ASSERT_EQUAL_UINT32(16, s_seen_strip_config.max_leds);
    TEST_ASSERT_EQUAL(LED_MODEL_WS2812, s_seen_strip_config.led_model);
    TEST_ASSERT_EQUAL_HEX32(LED_STRIP_COLOR_COMPONENT_FMT_GRB.format_id,
                            s_seen_strip_config.color_component_format.format_id);
    TEST_ASSERT_EQUAL(SPI2_HOST, s_seen_spi_config.spi_bus);
    TEST_ASSERT_EQUAL_UINT32(1, s_seen_spi_config.flags.with_dma);
}

static void test_init_twice_fails(void)
{
    init_ok(16);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, led_output_init(10, 16));
}

static void test_init_reports_driver_error(void)
{
    led_strip_new_spi_device_ExpectAnyArgsAndReturn(ESP_ERR_NO_MEM);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, led_output_init(10, 16));
    // Danach ist nichts initialisiert
    led_color_t c = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, led_output_show(&c, 1));
}

static void test_init_rejects_zero_leds(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_output_init(10, 0));
}

static void test_show_before_init_fails(void)
{
    led_color_t c = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, led_output_show(&c, 1));
}

static void test_show_sets_pixels_and_refreshes(void)
{
    init_ok(3);
    led_color_t colors[3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    for (uint32_t i = 0; i < 3; i++) {
        led_strip_set_pixel_ExpectAndReturn(FAKE_HANDLE, i, colors[i].r, colors[i].g, colors[i].b, ESP_OK);
    }
    led_strip_refresh_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, led_output_show(colors, 3));
}

static void test_show_stops_on_pixel_error(void)
{
    init_ok(3);
    led_color_t colors[3] = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    led_strip_set_pixel_ExpectAndReturn(FAKE_HANDLE, 0, 1, 2, 3, ESP_OK);
    led_strip_set_pixel_ExpectAndReturn(FAKE_HANDLE, 1, 4, 5, 6, ESP_FAIL);
    // Kein weiterer set_pixel und kein refresh erwartet
    TEST_ASSERT_EQUAL(ESP_FAIL, led_output_show(colors, 3));
}

static void test_show_reports_refresh_error(void)
{
    init_ok(1);
    led_color_t c = {9, 9, 9};
    led_strip_set_pixel_ExpectAndReturn(FAKE_HANDLE, 0, 9, 9, 9, ESP_OK);
    led_strip_refresh_ExpectAndReturn(FAKE_HANDLE, ESP_ERR_TIMEOUT);
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, led_output_show(&c, 1));
}

static void test_show_rejects_too_many_leds(void)
{
    init_ok(2);
    led_color_t colors[3] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_output_show(colors, 3));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, led_output_show(NULL, 1));
}

static void test_deinit_deletes_strip(void)
{
    init_ok(1);
    led_strip_del_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, led_output_deinit());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, led_output_deinit());
}

// Wird von test_main.c aufgerufen
void run_led_output_tests(void)
{
    RUN_TEST(test_init_passes_strip_config);
    RUN_TEST(test_init_twice_fails);
    RUN_TEST(test_init_reports_driver_error);
    RUN_TEST(test_init_rejects_zero_leds);
    RUN_TEST(test_show_before_init_fails);
    RUN_TEST(test_show_sets_pixels_and_refreshes);
    RUN_TEST(test_show_stops_on_pixel_error);
    RUN_TEST(test_show_reports_refresh_error);
    RUN_TEST(test_show_rejects_too_many_leds);
    RUN_TEST(test_deinit_deletes_strip);
}
