#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "espnow_config.h"

// Version des Binärformats (bei Änderungen erhöhen)
#define ESPNOW_CONFIG_FORMAT  1

void espnow_config_default(espnow_config_t *cfg)
{
    if (cfg != NULL) {
        memset(cfg, 0, sizeof(*cfg));
        cfg->role = ESPNOW_ROLE_GATEWAY;
    }
}

// Wert einer Hex-Ziffer, -1 bei ungültigem Zeichen
static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = (char)tolower((unsigned char)c);
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

// Liest eine MAC aus text[0..len); Leerraum am Rand wird ignoriert
static bool parse_mac(const char *text, size_t len, uint8_t mac[ESPNOW_MAC_LEN])
{
    while (len > 0 && isspace((unsigned char)*text)) {
        text++;
        len--;
    }
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        len--;
    }
    if (len != ESPNOW_MAC_STR_LEN - 1) {
        return false;
    }
    uint8_t tmp[ESPNOW_MAC_LEN];
    for (size_t i = 0; i < ESPNOW_MAC_LEN; i++) {
        const char *p = text + 3 * i;
        int hi = hex_value(p[0]);
        int lo = hex_value(p[1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        // Trenner überall gleich: ':' oder '-'
        if (i < ESPNOW_MAC_LEN - 1 && (p[2] != text[2] || (p[2] != ':' && p[2] != '-'))) {
            return false;
        }
        tmp[i] = (uint8_t)(hi * 16 + lo);
    }
    // Gruppenadressen (Bit 0 im ersten Byte) und 00:..:00 sind keine Geräte
    if ((tmp[0] & 0x01) != 0 || !espnow_mac_is_set(tmp)) {
        return false;
    }
    memcpy(mac, tmp, ESPNOW_MAC_LEN);
    return true;
}

bool espnow_mac_parse(const char *text, uint8_t mac[ESPNOW_MAC_LEN])
{
    if (text == NULL || mac == NULL) {
        return false;
    }
    return parse_mac(text, strlen(text), mac);
}

void espnow_mac_format(const uint8_t mac[ESPNOW_MAC_LEN], char out[ESPNOW_MAC_STR_LEN])
{
    snprintf(out, ESPNOW_MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool espnow_mac_is_set(const uint8_t mac[ESPNOW_MAC_LEN])
{
    if (mac == NULL) {
        return false;
    }
    for (size_t i = 0; i < ESPNOW_MAC_LEN; i++) {
        if (mac[i] != 0) {
            return true;
        }
    }
    return false;
}

bool espnow_config_has_peer(const espnow_config_t *cfg, const uint8_t mac[ESPNOW_MAC_LEN])
{
    if (cfg == NULL || mac == NULL) {
        return false;
    }
    for (size_t i = 0; i < cfg->peer_count && i < ESPNOW_PEERS_MAX; i++) {
        if (memcmp(cfg->peers[i], mac, ESPNOW_MAC_LEN) == 0) {
            return true;
        }
    }
    return false;
}

// Nur Leerraum in text[0..len)?
static bool is_blank(const char *text, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (!isspace((unsigned char)text[i])) {
            return false;
        }
    }
    return true;
}

bool espnow_peer_list_parse(const char *text, espnow_config_t *cfg)
{
    if (text == NULL || cfg == NULL) {
        return false;
    }
    espnow_config_t tmp = *cfg;
    tmp.peer_count = 0;
    const char *p = text;
    while (true) {
        const char *end = strchr(p, ',');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (!is_blank(p, len)) {
            uint8_t mac[ESPNOW_MAC_LEN];
            if (!parse_mac(p, len, mac)) {
                return false;
            }
            if (!espnow_config_has_peer(&tmp, mac)) {
                if (tmp.peer_count >= ESPNOW_PEERS_MAX) {
                    return false;
                }
                memcpy(tmp.peers[tmp.peer_count++], mac, ESPNOW_MAC_LEN);
            }
        }
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    cfg->peer_count = tmp.peer_count;
    memcpy(cfg->peers, tmp.peers, sizeof(cfg->peers));
    return true;
}

int espnow_peer_list_format(const espnow_config_t *cfg, char *out, size_t len)
{
    if (cfg == NULL || out == NULL || len == 0 || cfg->peer_count > ESPNOW_PEERS_MAX) {
        return -1;
    }
    size_t pos = 0;
    out[0] = '\0';
    for (size_t i = 0; i < cfg->peer_count; i++) {
        char mac[ESPNOW_MAC_STR_LEN];
        espnow_mac_format(cfg->peers[i], mac);
        int n = snprintf(out + pos, len - pos, "%s%s", i > 0 ? ", " : "", mac);
        if (n < 0 || (size_t)n >= len - pos) {
            out[0] = '\0';
            return -1;
        }
        pos += (size_t)n;
    }
    return (int)pos;
}

espnow_config_result_t espnow_config_validate(const espnow_config_t *cfg,
                                              const uint8_t own_mac[ESPNOW_MAC_LEN])
{
    if (cfg == NULL || (cfg->role != ESPNOW_ROLE_GATEWAY && cfg->role != ESPNOW_ROLE_RECEIVER)) {
        return ESPNOW_CONFIG_ERR_ROLE;
    }
    if (cfg->peer_count > ESPNOW_PEERS_MAX ||
        (own_mac != NULL && espnow_config_has_peer(cfg, own_mac))) {
        return ESPNOW_CONFIG_ERR_PEERS;
    }
    if (cfg->role == ESPNOW_ROLE_RECEIVER) {
        if (!espnow_mac_is_set(cfg->gateway) || (cfg->gateway[0] & 0x01) != 0) {
            return ESPNOW_CONFIG_ERR_GATEWAY;
        }
        if (own_mac != NULL && memcmp(cfg->gateway, own_mac, ESPNOW_MAC_LEN) == 0) {
            return ESPNOW_CONFIG_ERR_GATEWAY;
        }
    }
    return ESPNOW_CONFIG_OK;
}

int espnow_config_to_bytes(const espnow_config_t *cfg, uint8_t *buf, size_t len)
{
    if (cfg == NULL || buf == NULL || cfg->peer_count > ESPNOW_PEERS_MAX) {
        return -1;
    }
    size_t need = 3 + ESPNOW_MAC_LEN * (1 + cfg->peer_count);
    if (len < need) {
        return -1;
    }
    buf[0] = ESPNOW_CONFIG_FORMAT;
    buf[1] = (uint8_t)cfg->role;
    memcpy(buf + 2, cfg->gateway, ESPNOW_MAC_LEN);
    buf[2 + ESPNOW_MAC_LEN] = (uint8_t)cfg->peer_count;
    memcpy(buf + 3 + ESPNOW_MAC_LEN, cfg->peers, ESPNOW_MAC_LEN * cfg->peer_count);
    return (int)need;
}

bool espnow_config_from_bytes(const uint8_t *data, size_t len, espnow_config_t *out)
{
    if (data == NULL || out == NULL || len < 3 + ESPNOW_MAC_LEN ||
        data[0] != ESPNOW_CONFIG_FORMAT) {
        return false;
    }
    size_t count = data[2 + ESPNOW_MAC_LEN];
    if (count > ESPNOW_PEERS_MAX || len != 3 + ESPNOW_MAC_LEN * (1 + count) ||
        (data[1] != ESPNOW_ROLE_GATEWAY && data[1] != ESPNOW_ROLE_RECEIVER)) {
        return false;
    }
    espnow_config_default(out);
    out->role = (espnow_role_t)data[1];
    memcpy(out->gateway, data + 2, ESPNOW_MAC_LEN);
    out->peer_count = count;
    memcpy(out->peers, data + 3 + ESPNOW_MAC_LEN, ESPNOW_MAC_LEN * count);
    return true;
}
