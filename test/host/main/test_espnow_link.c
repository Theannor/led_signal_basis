#include <string.h>
#include "unity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "espnow_link.h"
#include "espnow_proto.h"
#include "Mockesp_now.h"
#include "Mockesp_wifi.h"

static const uint8_t GATEWAY[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static const uint8_t PEER_A[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xa1};
static const uint8_t PEER_B[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xb2};
static const uint8_t STRANGER[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0xee};

// Von den Stubs gemerkte Callbacks und Aufrufe
static esp_now_recv_cb_t s_recv_cb;
static esp_now_send_cb_t s_send_cb;
static esp_now_peer_info_t s_peers[4];
static int s_peer_count;
static int s_sends;
static uint8_t s_last_dest[6];
static uint8_t s_last_data[ESPNOW_PROTO_MAX_LEN];
static size_t s_last_len;
static volatile uint8_t s_channel;
static volatile uint8_t s_gateway_channel;  // Kanal, auf dem das simulierte Gateway antwortet

// Palette des Geräts und gemeldete Paletten
static led_palette_t s_palette;
static led_palette_t s_received;
static volatile int s_received_count;

static void get_palette(led_palette_t *out)
{
    *out = s_palette;
}

static void on_palette(const led_palette_t *palette)
{
    s_received = *palette;
    s_palette = *palette;
    s_received_count++;
}

static esp_err_t recv_reg_stub(esp_now_recv_cb_t cb, int n)
{
    s_recv_cb = cb;
    return ESP_OK;
}

static esp_err_t send_reg_stub(esp_now_send_cb_t cb, int n)
{
    s_send_cb = cb;
    return ESP_OK;
}

static esp_err_t add_peer_stub(const esp_now_peer_info_t *peer, int n)
{
    TEST_ASSERT_LESS_THAN(4, s_peer_count);
    s_peers[s_peer_count++] = *peer;
    return ESP_OK;
}

// Merkt das Paket; antwortet wie der Funk mit einer Bestätigung, wenn der
// Empfänger auf dem Kanal des Gateways ist (bzw. immer beim Gateway-Test)
static esp_err_t send_stub(const uint8_t *addr, const uint8_t *data, size_t len, int n)
{
    s_sends++;
    memcpy(s_last_dest, addr, 6);
    memcpy(s_last_data, data, len);
    s_last_len = len;
    esp_now_send_info_t info = {.des_addr = (uint8_t *)addr};
    bool ack = s_gateway_channel == 0 || s_channel == s_gateway_channel;
    s_send_cb(&info, ack ? ESP_NOW_SEND_SUCCESS : ESP_NOW_SEND_FAIL);
    return ESP_OK;
}

static esp_err_t set_channel_stub(uint8_t primary, wifi_second_chan_t second, int n)
{
    s_channel = primary;
    return ESP_OK;
}

static esp_err_t get_mode_stub(wifi_mode_t *mode, int n)
{
    *mode = WIFI_MODE_AP;
    return ESP_OK;
}

static void reset(void)
{
    s_recv_cb = NULL;
    s_send_cb = NULL;
    s_peer_count = 0;
    s_sends = 0;
    s_last_len = 0;
    s_channel = 0;
    s_gateway_channel = 0;
    s_received_count = 0;
    memset(&s_palette, 0, sizeof(s_palette));
    esp_now_init_ExpectAndReturn(ESP_OK);
    esp_now_register_recv_cb_Stub(recv_reg_stub);
    esp_now_register_send_cb_Stub(send_reg_stub);
    esp_now_add_peer_Stub(add_peer_stub);
    esp_now_send_Stub(send_stub);
}

static void stop(void)
{
    esp_now_unregister_recv_cb_IgnoreAndReturn(ESP_OK);
    esp_now_unregister_send_cb_IgnoreAndReturn(ESP_OK);
    esp_now_deinit_IgnoreAndReturn(ESP_OK);
    espnow_link_stop();
}

// Paket wie vom Funk empfangen an den registrierten Callback geben
static void deliver(const uint8_t src[6], const uint8_t *data, size_t len)
{
    esp_now_recv_info_t info = {.src_addr = (uint8_t *)src};
    s_recv_cb(&info, data, (int)len);
}

static espnow_config_t gateway_cfg(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    memcpy(cfg.peers[0], PEER_A, 6);
    memcpy(cfg.peers[1], PEER_B, 6);
    cfg.peer_count = 2;
    return cfg;
}

static void start_gateway(void)
{
    reset();
    esp_wifi_get_mode_Stub(get_mode_stub);
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_APSTA, ESP_OK);
    espnow_config_t cfg = gateway_cfg();
    TEST_ASSERT_EQUAL(ESP_OK, espnow_link_gateway_start(&cfg, get_palette));
}

static void test_gateway_start_adds_peers_on_sta(void)
{
    start_gateway();
    TEST_ASSERT_EQUAL_INT(2, s_peer_count);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(PEER_A, s_peers[0].peer_addr, 6);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(PEER_B, s_peers[1].peer_addr, 6);
    TEST_ASSERT_EQUAL(WIFI_IF_STA, s_peers[0].ifidx);
    TEST_ASSERT_EQUAL_UINT8(0, s_peers[0].channel);
    TEST_ASSERT_FALSE(s_peers[0].encrypt);
    // Zweiter Start ohne Stopp ist nicht erlaubt
    espnow_config_t cfg = gateway_cfg();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, espnow_link_gateway_start(&cfg, get_palette));
    stop();
}

static void test_gateway_send_reaches_all_peers(void)
{
    start_gateway();
    led_palette_t p = {.count = 1, .colors = {{.r = 1, .g = 2, .b = 3}}};
    TEST_ASSERT_EQUAL(ESP_OK, espnow_link_gateway_send(&p));
    TEST_ASSERT_EQUAL_INT(2, s_sends);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(PEER_B, s_last_dest, 6);
    espnow_msg_t msg;
    TEST_ASSERT_TRUE(espnow_proto_decode(s_last_data, s_last_len, &msg));
    TEST_ASSERT_EQUAL(ESPNOW_MSG_PALETTE, msg.type);
    TEST_ASSERT_EQUAL_UINT8(3, msg.palette.colors[0].b);
    stop();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, espnow_link_gateway_send(&p));
}

static void test_gateway_answers_hello_only_when_needed(void)
{
    start_gateway();
    s_palette = (led_palette_t){.count = 1, .colors = {{.r = 9}}};
    uint8_t hello[ESPNOW_PROTO_HEADER_LEN];

    // Empfänger mit anderem Stand bekommt die Palette
    espnow_proto_encode_hello(0x1234, hello, sizeof(hello));
    deliver(PEER_A, hello, sizeof(hello));
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL_INT(1, s_sends);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(PEER_A, s_last_dest, 6);

    // Gleicher Stand: keine Antwort
    espnow_proto_encode_hello(led_palette_hash(&s_palette), hello, sizeof(hello));
    deliver(PEER_B, hello, sizeof(hello));
    // Unbekannter Absender und kaputtes Paket: ignorieren
    espnow_proto_encode_hello(0x1234, hello, sizeof(hello));
    deliver(STRANGER, hello, sizeof(hello));
    deliver(PEER_A, hello, 3);
    vTaskDelay(pdMS_TO_TICKS(20));
    TEST_ASSERT_EQUAL_INT(1, s_sends);
    stop();
}

static void test_gateway_start_errors(void)
{
    espnow_config_t cfg = gateway_cfg();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, espnow_link_gateway_start(NULL, get_palette));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, espnow_link_gateway_start(&cfg, NULL));

    esp_wifi_get_mode_Stub(get_mode_stub);
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_APSTA, ESP_OK);
    esp_now_init_ExpectAndReturn(ESP_ERR_NO_MEM);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, espnow_link_gateway_start(&cfg, get_palette));
    // Danach ist ein neuer Start möglich
    start_gateway();
    stop();
}

static espnow_config_t receiver_cfg(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    cfg.role = ESPNOW_ROLE_RECEIVER;
    memcpy(cfg.gateway, GATEWAY, 6);
    return cfg;
}

static void start_receiver(uint8_t gateway_channel)
{
    reset();
    s_gateway_channel = gateway_channel;
    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_STA, ESP_OK);
    esp_wifi_set_ps_ExpectAndReturn(WIFI_PS_NONE, ESP_OK);
    esp_wifi_set_country_code_ExpectAndReturn("DE", false, ESP_OK);
    esp_wifi_set_channel_Stub(set_channel_stub);
    espnow_link_receiver_cfg_t rcfg = {
        .scan = {.max_channel = 13, .tries_per_channel = 2, .max_misses = 2,
                 .retry_ms = 2, .keepalive_ms = 20},
        .ack_timeout_ms = 5,
        .country = "DE",
    };
    espnow_config_t cfg = receiver_cfg();
    TEST_ASSERT_EQUAL(ESP_OK, espnow_link_receiver_start(&cfg, &rcfg, get_palette, on_palette));
}

// Wartet bis zu 2 s darauf, dass connected den erwarteten Wert hat
static bool wait_connected(bool expected)
{
    for (int i = 0; i < 200; i++) {
        if (espnow_link_receiver_connected() == expected) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}

static void test_receiver_finds_gateway_channel(void)
{
    start_receiver(6);
    TEST_ASSERT_EQUAL_INT(1, s_peer_count);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(GATEWAY, s_peers[0].peer_addr, 6);
    TEST_ASSERT_TRUE(wait_connected(true));
    TEST_ASSERT_EQUAL_UINT8(6, s_channel);
    // HELLO an das Gateway mit der Prüfsumme der eigenen Palette
    espnow_msg_t msg;
    TEST_ASSERT_EQUAL_UINT8_ARRAY(GATEWAY, s_last_dest, 6);
    TEST_ASSERT_TRUE(espnow_proto_decode(s_last_data, s_last_len, &msg));
    TEST_ASSERT_EQUAL(ESPNOW_MSG_HELLO, msg.type);
    TEST_ASSERT_EQUAL_HEX32(led_palette_hash(&s_palette), msg.hash);
    stop();
    TEST_ASSERT_FALSE(espnow_link_receiver_connected());
}

static void test_receiver_searches_again_when_gateway_moves(void)
{
    start_receiver(3);
    TEST_ASSERT_TRUE(wait_connected(true));
    TEST_ASSERT_EQUAL_UINT8(3, s_channel);
    s_gateway_channel = 11;  // Router hat den Kanal gewechselt
    // Die Suche ist schneller als ein Tick; daher nur das Ergebnis prüfen
    bool moved = false;
    for (int i = 0; i < 200 && !moved; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        moved = s_channel == 11 && espnow_link_receiver_connected();
    }
    TEST_ASSERT_TRUE(moved);
    stop();
}

static void test_receiver_takes_palette_only_from_gateway(void)
{
    start_receiver(1);
    TEST_ASSERT_TRUE(wait_connected(true));
    led_palette_t p = {.count = 2, .colors = {{.r = 10}, {.g = 20}}};
    uint8_t buf[ESPNOW_PROTO_MAX_LEN];
    int len = espnow_proto_encode_palette(&p, buf, sizeof(buf));

    deliver(STRANGER, buf, (size_t)len);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_INT(0, s_received_count);

    deliver(GATEWAY, buf, (size_t)len);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_INT(1, s_received_count);
    TEST_ASSERT_EQUAL_size_t(2, s_received.count);
    TEST_ASSERT_EQUAL_UINT8(20, s_received.colors[1].g);

    // Dieselbe Palette nochmal: nichts zu tun
    deliver(GATEWAY, buf, (size_t)len);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL_INT(1, s_received_count);
    stop();
}

static void test_receiver_start_errors(void)
{
    espnow_link_receiver_cfg_t rcfg = {.ack_timeout_ms = 5};
    espnow_config_t cfg = receiver_cfg();
    espnow_config_t no_gateway;
    espnow_config_default(&no_gateway);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      espnow_link_receiver_start(&no_gateway, &rcfg, get_palette, on_palette));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      espnow_link_receiver_start(&cfg, NULL, get_palette, on_palette));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      espnow_link_receiver_start(&cfg, &rcfg, NULL, on_palette));

    esp_wifi_set_mode_ExpectAndReturn(WIFI_MODE_STA, ESP_FAIL);
    TEST_ASSERT_EQUAL(ESP_FAIL, espnow_link_receiver_start(&cfg, &rcfg, get_palette, on_palette));
    TEST_ASSERT_FALSE(espnow_link_receiver_connected());
}

// Wird von test_main.c aufgerufen
void run_espnow_link_tests(void)
{
    RUN_TEST(test_gateway_start_adds_peers_on_sta);
    RUN_TEST(test_gateway_send_reaches_all_peers);
    RUN_TEST(test_gateway_answers_hello_only_when_needed);
    RUN_TEST(test_gateway_start_errors);
    RUN_TEST(test_receiver_finds_gateway_channel);
    RUN_TEST(test_receiver_searches_again_when_gateway_moves);
    RUN_TEST(test_receiver_takes_palette_only_from_gateway);
    RUN_TEST(test_receiver_start_errors);
}
