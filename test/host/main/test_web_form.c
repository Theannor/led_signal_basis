// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_form.h"

static void test_get_field_simple(void)
{
    char out[40];
    TEST_ASSERT_TRUE(web_form_get_field("ssid=Heim&hostname=led", "hostname", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("led", out);
    TEST_ASSERT_TRUE(web_form_get_field("ssid=Heim&hostname=led", "ssid", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("Heim", out);
}

static void test_get_field_decodes(void)
{
    char out[40];
    TEST_ASSERT_TRUE(web_form_get_field("ssid=Mein+WLAN%21%c3%a4", "ssid", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("Mein WLAN!\xc3\xa4", out);
}

static void test_get_field_empty_value(void)
{
    char out[40] = "x";
    TEST_ASSERT_TRUE(web_form_get_field("password=&ssid=a", "password", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
}

static void test_get_field_missing_or_prefix_key(void)
{
    char out[40];
    TEST_ASSERT_FALSE(web_form_get_field("ssid=a", "password", out, sizeof(out)));
    // "ssid2" darf nicht als "ssid" erkannt werden
    TEST_ASSERT_FALSE(web_form_get_field("ssid2=a", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(web_form_get_field("", "ssid", out, sizeof(out)));
    TEST_ASSERT_FALSE(web_form_get_field(NULL, "ssid", out, sizeof(out)));
}

static void test_get_field_too_long(void)
{
    char out[4];
    TEST_ASSERT_TRUE(web_form_get_field("k=abc", "k", out, sizeof(out)));
    TEST_ASSERT_FALSE(web_form_get_field("k=abcd", "k", out, sizeof(out)));
}

static void test_get_field_bad_encoding(void)
{
    char out[40];
    TEST_ASSERT_FALSE(web_form_get_field("k=%zz", "k", out, sizeof(out)));
    TEST_ASSERT_FALSE(web_form_get_field("k=ab%4", "k", out, sizeof(out)));
    TEST_ASSERT_FALSE(web_form_get_field("k=%", "k", out, sizeof(out)));
}

static void test_escape(void)
{
    char out[64];
    TEST_ASSERT_TRUE(web_html_escape("<a href=\"x\">&'", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("&lt;a href=&quot;x&quot;&gt;&amp;&#39;", out);
    TEST_ASSERT_TRUE(web_html_escape(NULL, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("", out);
}

static void test_escape_too_small(void)
{
    char out[4];
    TEST_ASSERT_FALSE(web_html_escape("<<", out, sizeof(out)));
    TEST_ASSERT_TRUE(web_html_escape("abc", out, sizeof(out)));
}

static void test_render_contains_info_and_form(void)
{
    char buf[4096];
    web_page_info_t info = {
        .client_mode = true, .ip = "192.168.1.50", .hostname = "led-signal-basis",
        .ssid = "Heim", .message = NULL,
    };
    int n = web_page_render(buf, sizeof(buf), &info);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_size_t((size_t)n, strlen(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "WLAN-Client"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "192.168.1.50"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "value=\"Heim\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "value=\"led-signal-basis\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "action=\"/config\""));
    TEST_ASSERT_NULL(strstr(buf, "class=\"msg"));
}

static void test_render_escapes_and_shows_error(void)
{
    char buf[4096];
    web_page_info_t info = {
        .client_mode = false, .ip = "192.168.4.1", .hostname = "h",
        .ssid = "\"><script>", .message = "Fehler <1>", .message_is_error = true,
    };
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Access Point"));
    TEST_ASSERT_NULL(strstr(buf, "<script>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "&quot;&gt;&lt;script&gt;"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "class=\"msg err\">Fehler &lt;1&gt;"));
}

static void test_render_too_small_buffer(void)
{
    char buf[100];
    web_page_info_t info = {.ip = "1.2.3.4", .hostname = "h", .ssid = "s"};
    TEST_ASSERT_EQUAL_INT(-1, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_EQUAL_INT(-1, web_page_render(buf, sizeof(buf), NULL));
}

static void test_trim_both_ends(void)
{
    char s[] = "  \tHeim \n";
    web_form_trim(s);
    TEST_ASSERT_EQUAL_STRING("Heim", s);
}

static void test_trim_keeps_inner_spaces(void)
{
    char s[] = "Mein WLAN ";
    web_form_trim(s);
    TEST_ASSERT_EQUAL_STRING("Mein WLAN", s);
}

static void test_trim_only_spaces_and_nothing_to_do(void)
{
    char a[] = "   ";
    web_form_trim(a);
    TEST_ASSERT_EQUAL_STRING("", a);
    char b[] = "abc";
    web_form_trim(b);
    TEST_ASSERT_EQUAL_STRING("abc", b);
    char c[] = "";
    web_form_trim(c);
    TEST_ASSERT_EQUAL_STRING("", c);
    web_form_trim(NULL);
}

// Wird von test_main.c aufgerufen
void run_web_form_tests(void)
{
    RUN_TEST(test_get_field_simple);
    RUN_TEST(test_get_field_decodes);
    RUN_TEST(test_get_field_empty_value);
    RUN_TEST(test_get_field_missing_or_prefix_key);
    RUN_TEST(test_get_field_too_long);
    RUN_TEST(test_get_field_bad_encoding);
    RUN_TEST(test_escape);
    RUN_TEST(test_escape_too_small);
    RUN_TEST(test_render_contains_info_and_form);
    RUN_TEST(test_render_escapes_and_shows_error);
    RUN_TEST(test_render_too_small_buffer);
    RUN_TEST(test_trim_both_ends);
    RUN_TEST(test_trim_keeps_inner_spaces);
    RUN_TEST(test_trim_only_spaces_and_nothing_to_do);
}
