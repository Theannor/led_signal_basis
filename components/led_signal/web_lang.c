// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <ctype.h>
#include <string.h>
#include "web_lang.h"
#include "espnow_config.h"
#include "led_config.h"
#include "led_palette.h"
#include "net_config.h"

#define Q_MAX  1000   // q-Werte in Tausendsteln (q=1 -> 1000)

// Zahl als Text in String-Literalen
#define STR_(x) #x
#define STR(x)  STR_(x)

// Meldungen je Sprache, Index = web_msg_t
static const char *const MSG_DE[WEB_MSG_COUNT] = {
    [WEB_MSG_SAVED_RESTART]  = "Gespeichert. Das Gerät startet neu …",
    [WEB_MSG_SAVE_FAILED]    = "Speichern fehlgeschlagen.",
    [WEB_MSG_BAD_REQUEST]    = "Anfrage zu groß oder unvollständig.",
    [WEB_MSG_SSID]           = "SSID ist zu lang (max. 32 Zeichen).",
    [WEB_MSG_PASSWORD]       = "Passwort muss 8 bis 63 Zeichen haben.",
    [WEB_MSG_HOSTNAME]       = "Hostname: 1 bis 32 Zeichen, nur a-z, 0-9 und '-', nicht am Anfang/Ende.",
    [WEB_MSG_INVALID]        = "Ungültige Eingabe.",
    [WEB_MSG_BLE_NAME]       = "Bluetooth-Name: 1 bis " STR(NET_CONFIG_BLE_NAME_MAX) " Zeichen, ohne Umlaute.",
    [WEB_MSG_ESPNOW_PEERS]   = "Empfänger: bis zu " STR(ESPNOW_PEERS_MAX) " MAC-Adressen wie aa:bb:cc:dd:ee:ff, "
                               "mit Komma getrennt, nicht die eigene.",
    [WEB_MSG_ESPNOW_GATEWAY] = "Gateway: eine MAC-Adresse wie aa:bb:cc:dd:ee:ff (nicht die eigene) angeben.",
    [WEB_MSG_ESPNOW_ROLE]    = "Bitte Gateway oder Empfänger wählen.",
    [WEB_MSG_LED]            = "LED-Strip: GPIO aus der Liste wählen, " STR(LED_CONFIG_COUNT_MIN) " bis "
                               STR(LED_CONFIG_COUNT_MAX) " LEDs.",
    [WEB_MSG_RECEIVER]       = "Dieses Gerät ist ESP-NOW-Empfänger und übernimmt die Farben vom Gateway.",
    [WEB_MSG_PALETTE_FORMAT] = "Erwartet {\"colors\":[\"#rrggbb\", ...]} mit höchstens "
                               STR(LED_PALETTE_MAX) " Farben.",
    [WEB_MSG_INTERNAL]       = "Interner Fehler.",
    [WEB_MSG_VERSION_INVALID] = "Version ungültig",
};

static const char *const MSG_EN[WEB_MSG_COUNT] = {
    [WEB_MSG_SAVED_RESTART]  = "Saved. The device is restarting …",
    [WEB_MSG_SAVE_FAILED]    = "Saving failed.",
    [WEB_MSG_BAD_REQUEST]    = "Request too large or incomplete.",
    [WEB_MSG_SSID]           = "SSID is too long (max. 32 characters).",
    [WEB_MSG_PASSWORD]       = "Password must have 8 to 63 characters.",
    [WEB_MSG_HOSTNAME]       = "Hostname: 1 to 32 characters, only a-z, 0-9 and '-', not at the start/end.",
    [WEB_MSG_INVALID]        = "Invalid input.",
    [WEB_MSG_BLE_NAME]       = "Bluetooth name: 1 to " STR(NET_CONFIG_BLE_NAME_MAX) " characters, ASCII only.",
    [WEB_MSG_ESPNOW_PEERS]   = "Receivers: up to " STR(ESPNOW_PEERS_MAX) " MAC addresses like aa:bb:cc:dd:ee:ff, "
                               "separated by commas, not this device's own.",
    [WEB_MSG_ESPNOW_GATEWAY] = "Gateway: enter a MAC address like aa:bb:cc:dd:ee:ff (not this device's own).",
    [WEB_MSG_ESPNOW_ROLE]    = "Please choose gateway or receiver.",
    [WEB_MSG_LED]            = "LED strip: choose a GPIO from the list, " STR(LED_CONFIG_COUNT_MIN) " to "
                               STR(LED_CONFIG_COUNT_MAX) " LEDs.",
    [WEB_MSG_RECEIVER]       = "This device is an ESP-NOW receiver and takes its colors from the gateway.",
    [WEB_MSG_PALETTE_FORMAT] = "Expected {\"colors\":[\"#rrggbb\", ...]} with at most "
                               STR(LED_PALETTE_MAX) " colors.",
    [WEB_MSG_INTERNAL]       = "Internal error.",
    [WEB_MSG_VERSION_INVALID] = "Invalid version",
};

const char *web_lang_code(web_lang_t lang)
{
    return lang == WEB_LANG_EN ? "en" : "de";
}

// Sprache aus einem Text der Länge len ("de", "EN", ...)
static bool lang_from_text(const char *s, size_t len, web_lang_t *out)
{
    if (len != 2) {
        return false;
    }
    char a = (char)tolower((unsigned char)s[0]);
    char b = (char)tolower((unsigned char)s[1]);
    if (a == 'd' && b == 'e') {
        *out = WEB_LANG_DE;
        return true;
    }
    if (a == 'e' && b == 'n') {
        *out = WEB_LANG_EN;
        return true;
    }
    return false;
}

bool web_lang_parse(const char *code, web_lang_t *out)
{
    if (code == NULL || out == NULL) {
        return false;
    }
    return lang_from_text(code, strlen(code), out);
}

static bool is_space(char c)
{
    return c == ' ' || c == '\t';
}

bool web_lang_from_cookie(const char *cookie, web_lang_t *out)
{
    if (cookie == NULL || out == NULL) {
        return false;
    }
    const size_t name_len = strlen(WEB_LANG_COOKIE);
    const char *p = cookie;
    while (*p != '\0') {
        while (is_space(*p) || *p == ';') {
            p++;
        }
        size_t pair_len = strcspn(p, ";");
        if (pair_len > name_len && strncmp(p, WEB_LANG_COOKIE, name_len) == 0 && p[name_len] == '=') {
            const char *value = p + name_len + 1;
            size_t value_len = pair_len - name_len - 1;
            while (value_len > 0 && is_space(value[value_len - 1])) {
                value_len--;
            }
            return lang_from_text(value, value_len, out);
        }
        p += pair_len;
    }
    return false;
}

// q-Wert wie "0.8" oder "1" in Tausendsteln; unlesbar -> 0
static int parse_q(const char *p)
{
    if (*p < '0' || *p > '9') {
        return 0;
    }
    int q = (*p - '0') * Q_MAX;
    p++;
    if (*p == '.') {
        p++;
        for (int scale = Q_MAX / 10; scale > 0 && *p >= '0' && *p <= '9'; scale /= 10, p++) {
            q += (*p - '0') * scale;
        }
    }
    return q > Q_MAX ? Q_MAX : q;
}

web_lang_t web_lang_from_accept(const char *accept)
{
    if (accept == NULL) {
        return WEB_LANG_DE;
    }
    bool any = false;
    int best_q = 0;
    web_lang_t best = WEB_LANG_EN;
    const char *p = accept;
    while (*p != '\0') {
        while (is_space(*p) || *p == ',') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        any = true;
        // Eintrag "de-DE;q=0.8": Hauptsprache bis '-', dann Parameter bis ','
        const char *tag = p;
        size_t tag_len = strcspn(p, "-_;, \t");
        p += strcspn(p, ";,");
        int q = Q_MAX;
        while (*p == ';') {
            p++;
            while (is_space(*p)) {
                p++;
            }
            if ((p[0] == 'q' || p[0] == 'Q') && p[1] == '=') {
                q = parse_q(p + 2);
            }
            p += strcspn(p, ";,");
        }
        web_lang_t lang;
        if (q > best_q && lang_from_text(tag, tag_len, &lang)) {
            best_q = q;
            best = lang;
        }
    }
    return any ? best : WEB_LANG_DE;
}

web_lang_t web_lang_select(const char *cookie, const char *accept)
{
    web_lang_t lang;
    if (web_lang_from_cookie(cookie, &lang)) {
        return lang;
    }
    return web_lang_from_accept(accept);
}

const char *web_msg(web_lang_t lang, web_msg_t msg)
{
    if ((int)msg <= WEB_MSG_NONE || msg >= WEB_MSG_COUNT) {
        return NULL;
    }
    return lang == WEB_LANG_EN ? MSG_EN[msg] : MSG_DE[msg];
}
