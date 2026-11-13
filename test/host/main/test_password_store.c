#include <string.h>
#include "unity.h"
#include "password_store.h"
#include "led_password.h"
#include "Mocknvs.h"

#define FAKE_HANDLE  ((nvs_handle_t)42)

// Feste "Zufallsfolge" 0, 1, 2, ... -> "rgbrgbrgbrgbrgbr"
static uint32_t s_next;
static uint32_t fake_rand(void)
{
    return s_next++;
}

// nvs_open erfolgreich, Handle FAKE_HANDLE
static void expect_open_ok(void)
{
    static nvs_handle_t handle = FAKE_HANDLE;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
}

// nvs_get_str liefert value mit Rückgabewert ret
static void expect_get(const char *value, esp_err_t ret)
{
    static size_t length;
    static char buffer[64];
    nvs_get_str_ExpectAndReturn(FAKE_HANDLE, "ap_password", NULL, NULL, ret);
    nvs_get_str_IgnoreArg_out_value();
    nvs_get_str_IgnoreArg_length();
    if (value != NULL) {
        length = strlen(value) + 1;
        memcpy(buffer, value, length);
        nvs_get_str_ReturnArrayThruPtr_out_value(buffer, length);
        nvs_get_str_ReturnThruPtr_length(&length);
    }
}

static void test_uses_stored_password(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    expect_open_ok();
    expect_get("bbbbggggrrrrbgrb", ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, password_store_get(pw, fake_rand));
    TEST_ASSERT_EQUAL_STRING("bbbbggggrrrrbgrb", pw);
}

static void test_creates_and_saves_when_missing(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    s_next = 0;
    expect_open_ok();
    expect_get(NULL, ESP_ERR_NVS_NOT_FOUND);
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "ap_password", "rgbrgbrgbrgbrgbr", ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, password_store_get(pw, fake_rand));
    TEST_ASSERT_EQUAL_STRING("rgbrgbrgbrgbrgbr", pw);
}

static void test_replaces_invalid_stored_password(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    s_next = 0;
    expect_open_ok();
    expect_get("hallo", ESP_OK);
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "ap_password", "rgbrgbrgbrgbrgbr", ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, password_store_get(pw, fake_rand));
    TEST_ASSERT_EQUAL_STRING("rgbrgbrgbrgbrgbr", pw);
}

static void test_open_error_is_returned(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_INITIALIZED, password_store_get(pw, fake_rand));
}

static void test_save_error_is_returned_but_password_usable(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    s_next = 0;
    expect_open_ok();
    expect_get(NULL, ESP_ERR_NVS_NOT_FOUND);
    nvs_set_str_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, password_store_get(pw, fake_rand));
    TEST_ASSERT_TRUE(led_password_is_valid(pw));
}

static void test_rejects_null_args(void)
{
    char pw[LED_PASSWORD_LEN + 1];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, password_store_get(NULL, fake_rand));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, password_store_get(pw, NULL));
}

// Wird von test_main.c aufgerufen
void run_password_store_tests(void)
{
    RUN_TEST(test_uses_stored_password);
    RUN_TEST(test_creates_and_saves_when_missing);
    RUN_TEST(test_replaces_invalid_stored_password);
    RUN_TEST(test_open_error_is_returned);
    RUN_TEST(test_save_error_is_returned_but_password_usable);
    RUN_TEST(test_rejects_null_args);
}
