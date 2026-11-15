// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_server.h"
#include "web_form.h"
#include "Mockesp_http_server.h"

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)

// Vom Stub gemerkter Handler für GET /api/version
static esp_err_t (*s_api_version)(httpd_req_t *req);

static char s_sent[256];
static char s_status[40];
static char s_type[40];

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    *handle = FAKE_SERVER;
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/api/version") == 0 && uri->method == HTTP_GET) {
        s_api_version = uri->handler;
    }
    return ESP_OK;
}

static esp_err_t send_stub(httpd_req_t *req, const char *buf, ssize_t len, int cmock_num_calls)
{
    TEST_ASSERT_LESS_THAN(sizeof(s_sent), (size_t)len);
    memcpy(s_sent, buf, (size_t)len);
    s_sent[len] = '\0';
    return ESP_OK;
}

static esp_err_t set_status_stub(httpd_req_t *req, const char *status, int cmock_num_calls)
{
    strlcpy(s_status, status, sizeof(s_status));
    return ESP_OK;
}

static esp_err_t set_type_stub(httpd_req_t *req, const char *type, int cmock_num_calls)
{
    strlcpy(s_type, type, sizeof(s_type));
    return ESP_OK;
}

// Server mit der angegebenen Version starten und GET /api/version aufrufen
static void get_version(const char *version)
{
    web_server_ctx_t ctx = {
        .client_mode = true,
        .port = 80,
        .config = {.ssid = "Heim", .hostname = "lampe"},
        .version = version,
    };
    s_api_version = NULL;
    s_sent[0] = s_status[0] = s_type[0] = '\0';
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    // Keine Cookie- und Accept-Language-Header -> deutsche Seiten
    httpd_req_get_hdr_value_str_IgnoreAndReturn(ESP_ERR_NOT_FOUND);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    TEST_ASSERT_NOT_NULL(s_api_version);

    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_Stub(set_type_stub);
    httpd_resp_send_Stub(send_stub);
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_api_version(&req));

    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

static void test_version_json(void)
{
    char buf[64];
    int n = web_version_json("2026.1", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"version\":\"2026.1\"}", buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    TEST_ASSERT_GREATER_THAN(0, web_version_json("2026.1+3.gabc1234.dirty", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"version\":\"2026.1+3.gabc1234.dirty\"}", buf);
}

static void test_version_json_rejects_invalid(void)
{
    char buf[64];
    TEST_ASSERT_EQUAL_INT(-1, web_version_json(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, web_version_json("1\"2", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, web_version_json("1\\2", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, web_version_json("1\n2", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, web_version_json("2026.1", NULL, 10));
    TEST_ASSERT_EQUAL_INT(-1, web_version_json("2026.1", buf, 0));
    // {"version":"2026.1"} hat 20 Zeichen + '\0'
    TEST_ASSERT_EQUAL_INT(-1, web_version_json("2026.1", buf, 20));
    TEST_ASSERT_EQUAL_INT(20, web_version_json("2026.1", buf, 21));
}

static void test_page_shows_version(void)
{
    char buf[4096];
    web_page_info_t info = {.ip = "1.2.3.4", .hostname = "h", .ssid = "s",
                            .version = "2026.1+3.gabc1234"};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<dt>Version</dt><dd>2026.1+3.gabc1234</dd>"));
}

static void test_page_without_version(void)
{
    char buf[4096];
    web_page_info_t info = {.ip = "1.2.3.4", .hostname = "h", .ssid = "s"};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NULL(strstr(buf, "<dt>Version</dt>"));
}

static void test_page_escapes_version(void)
{
    char buf[4096];
    web_page_info_t info = {.ip = "1.2.3.4", .hostname = "h", .ssid = "s",
                            .version = "<b>"};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<dd>&lt;b&gt;</dd>"));
}

static void test_api_version_is_served(void)
{
    get_version("2026.1");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_STRING("application/json", s_type);
    TEST_ASSERT_EQUAL_STRING("{\"version\":\"2026.1\"}", s_sent);
}

static void test_api_version_unknown(void)
{
    get_version(NULL);
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_STRING("{\"version\":\"0+unknown\"}", s_sent);
}

static void test_api_version_invalid(void)
{
    get_version("bad\"version");
    TEST_ASSERT_EQUAL_STRING("500 Internal Server Error", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "\"error\""));
}

// Wird von test_main.c aufgerufen
void run_web_version_tests(void)
{
    RUN_TEST(test_version_json);
    RUN_TEST(test_version_json_rejects_invalid);
    RUN_TEST(test_page_shows_version);
    RUN_TEST(test_page_without_version);
    RUN_TEST(test_page_escapes_version);
    RUN_TEST(test_api_version_is_served);
    RUN_TEST(test_api_version_unknown);
    RUN_TEST(test_api_version_invalid);
}
