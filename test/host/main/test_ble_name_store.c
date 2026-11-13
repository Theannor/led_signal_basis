#include <string.h>
#include "unity.h"
#include "ble_name_store.h"
#include "Mocknvs.h"

#define FAKE_HANDLE  ((nvs_handle_t)11)
#define DEFAULT_NAME "LED Signal"

static void expect_open_ok(void)
{
    static nvs_handle_t handle = FAKE_HANDLE;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
}

// nvs_get_str für "ble_name" liefert value bzw. den Fehler result
static void expect_get(const char *value, esp_err_t result)
{
    static char buffer[80];
    static size_t length;
    nvs_get_str_ExpectAndReturn(FAKE_HANDLE, "ble_name", NULL, NULL, result);
    nvs_get_str_IgnoreArg_out_value();
    nvs_get_str_IgnoreArg_length();
    if (result == ESP_OK) {
        length = strlen(value) + 1;
        memcpy(buffer, value, length);
        nvs_get_str_ReturnArrayThruPtr_out_value(buffer, length);
        nvs_get_str_ReturnThruPtr_length(&length);
    }
    nvs_close_Expect(FAKE_HANDLE);
}

static void test_load_stored_name(void)
{
    char name[21];
    expect_open_ok();
    expect_get("Wohnzimmer", ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, ble_name_store_load(name, sizeof(name), DEFAULT_NAME));
    TEST_ASSERT_EQUAL_STRING("Wohnzimmer", name);
}

static void test_load_missing_gives_default(void)
{
    char name[21] = "xxx";
    expect_open_ok();
    expect_get(NULL, ESP_ERR_NVS_NOT_FOUND);
    TEST_ASSERT_EQUAL(ESP_OK, ble_name_store_load(name, sizeof(name), DEFAULT_NAME));
    TEST_ASSERT_EQUAL_STRING(DEFAULT_NAME, name);
}

static void test_load_invalid_gives_default(void)
{
    char name[21];
    expect_open_ok();
    expect_get(" mit Leerzeichen", ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, ble_name_store_load(name, sizeof(name), DEFAULT_NAME));
    TEST_ASSERT_EQUAL_STRING(DEFAULT_NAME, name);

    expect_open_ok();
    expect_get(NULL, ESP_ERR_NVS_INVALID_LENGTH);
    TEST_ASSERT_EQUAL(ESP_OK, ble_name_store_load(name, sizeof(name), DEFAULT_NAME));
    TEST_ASSERT_EQUAL_STRING(DEFAULT_NAME, name);
}

static void test_load_open_error_and_bad_args(void)
{
    char name[21] = "";
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_INITIALIZED,
                      ble_name_store_load(name, sizeof(name), DEFAULT_NAME));
    TEST_ASSERT_EQUAL_STRING(DEFAULT_NAME, name);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_name_store_load(NULL, 10, DEFAULT_NAME));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_name_store_load(name, 0, DEFAULT_NAME));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_name_store_load(name, sizeof(name), NULL));
}

static void test_save_writes_name(void)
{
    expect_open_ok();
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "ble_name", "Kueche", ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, ble_name_store_save("Kueche"));
}

static void test_save_rejects_invalid_and_reports_errors(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_name_store_save(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_name_store_save(""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ble_name_store_save("viel zu langer Name 123"));

    expect_open_ok();
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "ble_name", "Kueche", ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, ble_name_store_save("Kueche"));
}

// Wird von test_main.c aufgerufen
void run_ble_name_store_tests(void)
{
    RUN_TEST(test_load_stored_name);
    RUN_TEST(test_load_missing_gives_default);
    RUN_TEST(test_load_invalid_gives_default);
    RUN_TEST(test_load_open_error_and_bad_args);
    RUN_TEST(test_save_writes_name);
    RUN_TEST(test_save_rejects_invalid_and_reports_errors);
}
