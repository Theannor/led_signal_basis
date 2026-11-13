#include <string.h>
#include "unity.h"
#include "espnow_config.h"

static const uint8_t MAC_A[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01};
static const uint8_t MAC_B[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};

static void test_mac_parse_accepts_formats(void)
{
    uint8_t mac[6];
    TEST_ASSERT_TRUE(espnow_mac_parse("aa:bb:cc:dd:ee:01", mac));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_A, mac, 6);
    TEST_ASSERT_TRUE(espnow_mac_parse("AA-BB-CC-DD-EE-01", mac));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_A, mac, 6);
    TEST_ASSERT_TRUE(espnow_mac_parse("  10:20:30:40:50:60 ", mac));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_B, mac, 6);
}

static void test_mac_parse_rejects_bad_input(void)
{
    uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    const uint8_t unchanged[6] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_FALSE(espnow_mac_parse("aa:bb:cc:dd:ee", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse("aa:bb:cc:dd:ee:0", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse("aa:bb:cc:dd:ee:011", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse("aa:bb-cc:dd:ee:01", mac));   // gemischte Trenner
    TEST_ASSERT_FALSE(espnow_mac_parse("aa.bb.cc.dd.ee.01", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse("gg:bb:cc:dd:ee:01", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse("ff:ff:ff:ff:ff:ff", mac));   // Broadcast
    TEST_ASSERT_FALSE(espnow_mac_parse("01:00:5e:00:00:01", mac));   // Multicast
    TEST_ASSERT_FALSE(espnow_mac_parse("00:00:00:00:00:00", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse("", mac));
    TEST_ASSERT_FALSE(espnow_mac_parse(NULL, mac));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(unchanged, mac, 6);
}

static void test_mac_format_and_is_set(void)
{
    char text[ESPNOW_MAC_STR_LEN];
    espnow_mac_format(MAC_A, text);
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:01", text);
    const uint8_t zero[6] = {0};
    TEST_ASSERT_FALSE(espnow_mac_is_set(zero));
    TEST_ASSERT_FALSE(espnow_mac_is_set(NULL));
    TEST_ASSERT_TRUE(espnow_mac_is_set(MAC_B));
}

static void test_peer_list_parse(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    TEST_ASSERT_TRUE(espnow_peer_list_parse(" aa:bb:cc:dd:ee:01 ,10-20-30-40-50-60,", &cfg));
    TEST_ASSERT_EQUAL_size_t(2, cfg.peer_count);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_A, cfg.peers[0], 6);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_B, cfg.peers[1], 6);
    TEST_ASSERT_TRUE(espnow_config_has_peer(&cfg, MAC_B));

    // Doppelte nur einmal, leere Liste erlaubt
    TEST_ASSERT_TRUE(espnow_peer_list_parse("aa:bb:cc:dd:ee:01, AA:BB:CC:DD:EE:01", &cfg));
    TEST_ASSERT_EQUAL_size_t(1, cfg.peer_count);
    TEST_ASSERT_TRUE(espnow_peer_list_parse(" , ", &cfg));
    TEST_ASSERT_EQUAL_size_t(0, cfg.peer_count);
    TEST_ASSERT_FALSE(espnow_config_has_peer(&cfg, MAC_A));
}

static void test_peer_list_parse_rejects_bad_input(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    TEST_ASSERT_TRUE(espnow_peer_list_parse("aa:bb:cc:dd:ee:01", &cfg));
    TEST_ASSERT_FALSE(espnow_peer_list_parse("aa:bb:cc:dd:ee:01, kaputt", &cfg));
    TEST_ASSERT_FALSE(espnow_peer_list_parse("aa:bb:cc:dd:ee:01 10:20:30:40:50:60", &cfg));
    TEST_ASSERT_FALSE(espnow_peer_list_parse(NULL, &cfg));
    // Unverändert nach Fehler
    TEST_ASSERT_EQUAL_size_t(1, cfg.peer_count);

    // Mehr als ESPNOW_PEERS_MAX
    char text[400] = "";
    for (int i = 0; i <= ESPNOW_PEERS_MAX; i++) {
        char mac[24];
        snprintf(mac, sizeof(mac), "%s02:00:00:00:00:%02x", i ? "," : "", i + 1);
        strcat(text, mac);
    }
    TEST_ASSERT_FALSE(espnow_peer_list_parse(text, &cfg));
    TEST_ASSERT_EQUAL_size_t(1, cfg.peer_count);
}

static void test_peer_list_format(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    char text[ESPNOW_PEER_LIST_STR_MAX];
    TEST_ASSERT_EQUAL_INT(0, espnow_peer_list_format(&cfg, text, sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("", text);
    TEST_ASSERT_TRUE(espnow_peer_list_parse("aa:bb:cc:dd:ee:01,10:20:30:40:50:60", &cfg));
    TEST_ASSERT_EQUAL_INT(36, espnow_peer_list_format(&cfg, text, sizeof(text)));
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:dd:ee:01, 10:20:30:40:50:60", text);
    TEST_ASSERT_EQUAL_INT(-1, espnow_peer_list_format(&cfg, text, 20));

    // Volle Liste passt in ESPNOW_PEER_LIST_STR_MAX
    cfg.peer_count = ESPNOW_PEERS_MAX;
    TEST_ASSERT_GREATER_THAN(0, espnow_peer_list_format(&cfg, text, sizeof(text)));
}

static void test_validate(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, cfg.role);
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_OK, espnow_config_validate(&cfg, MAC_A));

    // Gateway darf sich nicht selbst als Empfänger eintragen
    TEST_ASSERT_TRUE(espnow_peer_list_parse("aa:bb:cc:dd:ee:01", &cfg));
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_ERR_PEERS, espnow_config_validate(&cfg, MAC_A));
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_OK, espnow_config_validate(&cfg, MAC_B));
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_OK, espnow_config_validate(&cfg, NULL));

    // Empfänger braucht ein Gateway, nicht sich selbst
    cfg.role = ESPNOW_ROLE_RECEIVER;
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_ERR_GATEWAY, espnow_config_validate(&cfg, MAC_B));
    memcpy(cfg.gateway, MAC_B, 6);
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_ERR_GATEWAY, espnow_config_validate(&cfg, MAC_B));
    memcpy(cfg.gateway, MAC_A, 6);
    cfg.peer_count = 0;
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_OK, espnow_config_validate(&cfg, MAC_B));

    cfg.role = (espnow_role_t)7;
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_ERR_ROLE, espnow_config_validate(&cfg, NULL));
    TEST_ASSERT_EQUAL(ESPNOW_CONFIG_ERR_ROLE, espnow_config_validate(NULL, NULL));
}

static void test_bytes_roundtrip(void)
{
    espnow_config_t cfg, back;
    espnow_config_default(&cfg);
    cfg.role = ESPNOW_ROLE_RECEIVER;
    memcpy(cfg.gateway, MAC_A, 6);
    TEST_ASSERT_TRUE(espnow_peer_list_parse("10:20:30:40:50:60", &cfg));
    uint8_t buf[ESPNOW_CONFIG_BYTES_MAX];
    int len = espnow_config_to_bytes(&cfg, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(3 + 6 + 6, len);
    TEST_ASSERT_TRUE(espnow_config_from_bytes(buf, (size_t)len, &back));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_RECEIVER, back.role);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_A, back.gateway, 6);
    TEST_ASSERT_EQUAL_size_t(1, back.peer_count);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_B, back.peers[0], 6);

    // Volle Liste passt in ESPNOW_CONFIG_BYTES_MAX, zu kleiner Puffer nicht
    cfg.peer_count = ESPNOW_PEERS_MAX;
    TEST_ASSERT_EQUAL_INT(ESPNOW_CONFIG_BYTES_MAX, espnow_config_to_bytes(&cfg, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, espnow_config_to_bytes(&cfg, buf, sizeof(buf) - 1));
}

static void test_from_bytes_rejects_bad_data(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    uint8_t buf[ESPNOW_CONFIG_BYTES_MAX];
    int len = espnow_config_to_bytes(&cfg, buf, sizeof(buf));
    espnow_config_t out = {.peer_count = 99};

    TEST_ASSERT_FALSE(espnow_config_from_bytes(buf, (size_t)len - 1, &out));  // zu kurz
    TEST_ASSERT_FALSE(espnow_config_from_bytes(buf, (size_t)len + 6, &out));  // Länge passt nicht zur Anzahl
    buf[0] = 99;                                                               // falsches Format
    TEST_ASSERT_FALSE(espnow_config_from_bytes(buf, (size_t)len, &out));
    buf[0] = 1;
    buf[1] = 5;                                                                // falsche Rolle
    TEST_ASSERT_FALSE(espnow_config_from_bytes(buf, (size_t)len, &out));
    buf[1] = 0;
    buf[8] = ESPNOW_PEERS_MAX + 1;                                             // zu viele Peers
    TEST_ASSERT_FALSE(espnow_config_from_bytes(buf, (size_t)len, &out));
    TEST_ASSERT_EQUAL_size_t(99, out.peer_count);
}

// Wird von test_main.c aufgerufen
void run_espnow_config_tests(void)
{
    RUN_TEST(test_mac_parse_accepts_formats);
    RUN_TEST(test_mac_parse_rejects_bad_input);
    RUN_TEST(test_mac_format_and_is_set);
    RUN_TEST(test_peer_list_parse);
    RUN_TEST(test_peer_list_parse_rejects_bad_input);
    RUN_TEST(test_peer_list_format);
    RUN_TEST(test_validate);
    RUN_TEST(test_bytes_roundtrip);
    RUN_TEST(test_from_bytes_rejects_bad_data);
}
