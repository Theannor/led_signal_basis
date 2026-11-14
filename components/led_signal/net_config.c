// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "net_config.h"

bool net_config_hostname_is_valid(const char *hostname)
{
    if (hostname == NULL) {
        return false;
    }
    size_t len = strnlen(hostname, NET_CONFIG_HOSTNAME_MAX + 1);
    if (len == 0 || len > NET_CONFIG_HOSTNAME_MAX) {
        return false;
    }
    if (hostname[0] == '-' || hostname[len - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)hostname[i];
        if (!isalnum(c) && c != '-') {
            return false;
        }
    }
    return true;
}

bool net_config_ble_name_is_valid(const char *name)
{
    if (name == NULL) {
        return false;
    }
    size_t len = strnlen(name, NET_CONFIG_BLE_NAME_MAX + 1);
    if (len == 0 || len > NET_CONFIG_BLE_NAME_MAX) {
        return false;
    }
    if (name[0] == ' ' || name[len - 1] == ' ') {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c < 0x20 || c > 0x7E) {
            return false;
        }
    }
    return true;
}

net_config_result_t net_config_validate(const net_config_t *cfg)
{
    if (cfg == NULL) {
        return NET_CONFIG_ERR_SSID;
    }
    // Felder müssen innerhalb ihres Puffers enden
    if (strnlen(cfg->ssid, sizeof(cfg->ssid)) > NET_CONFIG_SSID_MAX) {
        return NET_CONFIG_ERR_SSID;
    }
    size_t pw_len = strnlen(cfg->password, sizeof(cfg->password));
    if (pw_len > NET_CONFIG_PASSWORD_MAX || (pw_len > 0 && pw_len < NET_CONFIG_PASSWORD_MIN)) {
        return NET_CONFIG_ERR_PASSWORD;
    }
    if (!net_config_hostname_is_valid(cfg->hostname)) {
        return NET_CONFIG_ERR_HOSTNAME;
    }
    return NET_CONFIG_OK;
}

bool net_should_fall_back(uint32_t now_ms, uint32_t lost_at_ms, bool connected, uint32_t limit_ms)
{
    if (connected) {
        return false;
    }
    // Vorzeichenlose Differenz stimmt auch über einen Überlauf hinweg
    return (uint32_t)(now_ms - lost_at_ms) >= limit_ms;
}

bool net_ap_ssid(const char *prefix, const uint8_t mac[6], char *out, size_t len)
{
    if (prefix == NULL || mac == NULL || out == NULL || len == 0) {
        return false;
    }
    int n = snprintf(out, len, "%s-%02X%02X", prefix, mac[4], mac[5]);
    if (n < 0 || (size_t)n >= len || n > NET_CONFIG_SSID_MAX) {
        out[0] = '\0';
        return false;
    }
    return true;
}

const char *net_disconnect_reason_text(uint16_t reason)
{
    switch (reason) {
    case 201:          return "WLAN nicht gefunden (SSID falsch oder nur 5 GHz?)";
    case 15:
    case 204:          return "Passwort falsch";
    case 2:
    case 202:          return "Anmeldung abgelehnt";
    case 200:          return "kein Beacon mehr (Signal zu schwach?)";
    case 203:          return "Zuordnung fehlgeschlagen";
    case 205:          return "Verbindung fehlgeschlagen";
    default:           return NULL;
    }
}
