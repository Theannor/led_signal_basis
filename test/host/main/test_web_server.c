// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_server.h"
#include "Mockesp_http_server.h"
#include "Mocknvs.h"

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)
#define FAKE_NVS    ((nvs_handle_t)3)

// Vom Stub gemerkte Handler
static esp_err_t (*s_get_handler)(httpd_req_t *req);
static esp_err_t (*s_post_handler)(httpd_req_t *req);

// Body der simulierten Anfrage und gesendete Antwort
static const char *s_body;
static char s_sent[4096];
static char s_status[40];
static int s_restarts;

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    TEST_ASSERT_EQUAL_UINT16(80, config->server_port);
    *handle = FAKE_SERVER;
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/") == 0 && uri->method == HTTP_GET) {
        s_get_handler = uri->handler;
    } else if (strcmp(uri->uri, "/config") == 0 && uri->method == HTTP_POST) {
        s_post_handler = uri->handler;
    }
    return ESP_OK;
}

static int recv_stub(httpd_req_t *req, char *buf, size_t len, int cmock_num_calls)
{
    // Absichtlich in kleinen Stücken liefern, um die Leseschleife zu prüfen
    static size_t offset;
    if (cmock_num_calls == 0) {
        offset = 0;
    }
    size_t rest = strlen(s_body) - offset;
    size_t n = rest < 10 ? rest : 10;
    n = n < len ? n : len;
    memcpy(buf, s_body + offset, n);
    offset += n;
    return (int)n;
}

static esp_err_t send_stub(httpd_req_t *req, const char *buf, ssize_t len, int cmock_num_calls)
{
    memcpy(s_sent, buf, (size_t)len);
    s_sent[len] = '\0';
    return ESP_OK;
}

static esp_err_t set_status_stub(httpd_req_t *req, const char *status, int cmock_num_calls)
{
    strlcpy(s_status, status, sizeof(s_status));
    return ESP_OK;
}

static void fake_ip(char *buf, size_t len)
{
    strlcpy(buf, "192.168.1.50", len);
}

static void fake_restart(void)
{
    s_restarts++;
}

// Server mit gespeicherter Konfiguration starten
static void start_server(void)
{
    web_server_ctx_t ctx = {
        .client_mode = true,
        .port = 80,
        .config = {.ssid = "Heim", .password = "altesPW123", .hostname = "lampe"},
        .get_ip = fake_ip,
        .schedule_restart = fake_restart,
    };
    s_restarts = 0;
    s_sent[0] = '\0';
    s_status[0] = '\0';
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    TEST_ASSERT_NOT_NULL(s_get_handler);
    TEST_ASSERT_NOT_NULL(s_post_handler);

    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_IgnoreAndReturn(ESP_OK);
    httpd_resp_send_Stub(send_stub);
}

static void stop_server(void)
{
    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

// POST /config mit body ausführen
static void post(const char *body)
{
    httpd_req_t req = {0};
    s_body = body;
    req.content_len = strlen(body);
    httpd_req_recv_Stub(recv_stub);
    TEST_ASSERT_EQUAL(ESP_OK, s_post_handler(&req));
}

// Erwartet das Speichern von ssid/password/hostname im NVS
static void expect_save(const char *ssid, const char *password, const char *hostname)
{
    static nvs_handle_t handle = FAKE_NVS;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
    nvs_set_str_ExpectAndReturn(FAKE_NVS, "sta_ssid", ssid, ESP_OK);
    nvs_set_str_ExpectAndReturn(FAKE_NVS, "sta_pass", password, ESP_OK);
    nvs_set_str_ExpectAndReturn(FAKE_NVS, "hostname", hostname, ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_NVS, ESP_OK);
    nvs_close_Expect(FAKE_NVS);
}

static void test_get_shows_page(void)
{
    start_server();
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_get_handler(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "192.168.1.50"));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"Heim\""));
    TEST_ASSERT_NULL(strstr(s_sent, "altesPW123"));
    stop_server();
}

static void test_post_saves_and_restarts(void)
{
    start_server();
    expect_save("Neu WLAN", "neuesPW123", "lampe2");
    post("ssid=Neu+WLAN&password=neuesPW123&hostname=lampe2");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "Gespeichert"));
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    stop_server();
}

static void test_post_empty_password_keeps_old(void)
{
    start_server();
    expect_save("Heim", "altesPW123", "lampe");
    post("ssid=Heim&password=&hostname=lampe");
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    stop_server();
}

static void test_post_open_network_clears_password(void)
{
    start_server();
    expect_save("Cafe", "", "lampe");
    post("ssid=Cafe&password=&open=1&hostname=lampe");
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    stop_server();
}

static void test_post_invalid_hostname_is_rejected(void)
{
    start_server();
    post("ssid=Heim&password=&hostname=mit_unterstrich");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "Hostname"));
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_post_short_password_is_rejected(void)
{
    start_server();
    post("ssid=Heim&password=kurz&hostname=lampe");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "Passwort"));
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_post_missing_ssid_is_rejected(void)
{
    start_server();
    post("hostname=lampe");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_post_too_large_body_is_rejected(void)
{
    start_server();
    httpd_req_t req = {0};
    req.content_len = 2000;
    TEST_ASSERT_EQUAL(ESP_OK, s_post_handler(&req));
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_start_reports_httpd_error(void)
{
    web_server_ctx_t ctx = {.port = 80};
    httpd_start_ExpectAnyArgsAndReturn(ESP_ERR_HTTPD_TASK);
    TEST_ASSERT_EQUAL(ESP_ERR_HTTPD_TASK, web_server_start(&ctx));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, web_server_stop());
}

static void test_post_trims_ssid_and_hostname(void)
{
    start_server();
    expect_save("Heim", "altesPW123", "lampe");
    post("ssid=Heim+&password=&hostname=+lampe+");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    stop_server();
}

static void test_post_keeps_spaces_in_password(void)
{
    start_server();
    expect_save("Heim", " pass wort ", "lampe");
    post("ssid=Heim&password=+pass+wort+&hostname=lampe");
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    stop_server();
}

// Wird von test_main.c aufgerufen
void run_web_server_tests(void)
{
    RUN_TEST(test_get_shows_page);
    RUN_TEST(test_post_saves_and_restarts);
    RUN_TEST(test_post_empty_password_keeps_old);
    RUN_TEST(test_post_open_network_clears_password);
    RUN_TEST(test_post_invalid_hostname_is_rejected);
    RUN_TEST(test_post_short_password_is_rejected);
    RUN_TEST(test_post_missing_ssid_is_rejected);
    RUN_TEST(test_post_too_large_body_is_rejected);
    RUN_TEST(test_start_reports_httpd_error);
    RUN_TEST(test_post_trims_ssid_and_hostname);
    RUN_TEST(test_post_keeps_spaces_in_password);
}
