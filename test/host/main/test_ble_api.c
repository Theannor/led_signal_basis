// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "ble_api.h"
#include "Mocknvs.h"

#define FAKE_HANDLE ((nvs_handle_t)13)

// Palette der "Anwendung" und Zähler für die Rückmeldung
static led_palette_t s_app_palette;
static led_palette_t s_changed;
static int s_changed_calls;

static void fake_get_palette(led_palette_t *out)
{
    *out = s_app_palette;
}

static void fake_changed(const led_palette_t *palette)
{
    s_changed = *palette;
    s_changed_calls++;
}

static void init_api(void)
{
    ble_api_ctx_t ctx = {.get_palette = fake_get_palette, .on_palette_changed = fake_changed};
    s_changed_calls = 0;
    memset(&s_changed, 0, sizeof(s_changed));
    TEST_ASSERT_EQUAL(ESP_OK, ble_api_init(&ctx));
}

// palette_store_save: NVS öffnen, Blob schreiben, commit, schließen
static void expect_save(esp_err_t set_result)
{
    static nvs_handle_t handle = FAKE_HANDLE;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
    nvs_set_blob_ExpectAnyArgsAndReturn(set_result);
    if (set_result == ESP_OK) {
        nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    }
    nvs_close_Expect(FAKE_HANDLE);
}

static void test_init_rejects_missing_getter(void)
{
    ble_api_ctx_t ctx = {.get_palette = NULL};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_api_init(&ctx));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_api_init(NULL));
}

static void test_read_returns_current_palette(void)
{
    init_api();
    s_app_palette = (led_palette_t){.count = 2, .colors = {{.r = 255}, {.g = 1, .b = 2}}};
    uint8_t buf[BLE_API_PALETTE_MAX_LEN];
    TEST_ASSERT_EQUAL_INT(6, ble_api_palette_read(buf, sizeof(buf)));
    const uint8_t expected[] = {255, 0, 0, 0, 1, 2};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, 6);

    s_app_palette.count = 0;
    TEST_ASSERT_EQUAL_INT(0, ble_api_palette_read(buf, sizeof(buf)));
}

static void test_write_saves_and_notifies(void)
{
    init_api();
    const uint8_t data[] = {0x10, 0x20, 0x30};
    expect_save(ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, ble_api_palette_write(data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(1, s_changed_calls);
    TEST_ASSERT_EQUAL_size_t(1, s_changed.count);
    TEST_ASSERT_EQUAL_UINT8(0x10, s_changed.colors[0].r);
    TEST_ASSERT_EQUAL_UINT8(0x20, s_changed.colors[0].g);
    TEST_ASSERT_EQUAL_UINT8(0x30, s_changed.colors[0].b);
}

static void test_write_empty_palette(void)
{
    init_api();
    expect_save(ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, ble_api_palette_write(NULL, 0));
    TEST_ASSERT_EQUAL_INT(1, s_changed_calls);
    TEST_ASSERT_EQUAL_size_t(0, s_changed.count);
}

static void test_write_rejects_bad_length(void)
{
    init_api();
    uint8_t data[BLE_API_PALETTE_MAX_LEN + 3] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ble_api_palette_write(data, 4));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ble_api_palette_write(data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_changed_calls);
}

static void test_write_reports_nvs_error(void)
{
    init_api();
    const uint8_t data[] = {1, 2, 3};
    expect_save(ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, ble_api_palette_write(data, sizeof(data)));
    TEST_ASSERT_EQUAL_INT(0, s_changed_calls);
}

// Wird von test_main.c aufgerufen
void run_ble_api_tests(void)
{
    RUN_TEST(test_init_rejects_missing_getter);
    RUN_TEST(test_read_returns_current_palette);
    RUN_TEST(test_write_saves_and_notifies);
    RUN_TEST(test_write_empty_palette);
    RUN_TEST(test_write_rejects_bad_length);
    RUN_TEST(test_write_reports_nvs_error);
}
