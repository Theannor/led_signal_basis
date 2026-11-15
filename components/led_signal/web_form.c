// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "web_form.h"
#include "led_config.h"
#include "led_palette.h"
#include "espnow_config.h"
#include "net_config.h"

#define WEB_TITLE "LED-Signal"  // Tab-Titel und Name in der Kopfleiste
#define WEB_LANG_COOKIE_AGE "31536000"  // Sprachwahl ein Jahr im Browser merken

// Wert einer Hex-Ziffer, -1 bei ungültigem Zeichen
static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// Dekodiert value[0..len) nach out; false bei Überlauf oder falschem %XX
static bool url_decode(const char *value, size_t len, char *out, size_t out_len)
{
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char c = value[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            // Es müssen noch zwei Hex-Ziffern folgen
            if (i + 2 >= len) {
                return false;
            }
            int hi = hex_value(value[i + 1]);
            int lo = hex_value(value[i + 2]);
            if (hi < 0 || lo < 0) {
                return false;
            }
            c = (char)(hi * 16 + lo);
            i += 2;
        }
        if (o + 1 >= out_len) {
            return false;
        }
        out[o++] = c;
    }
    out[o] = '\0';
    return true;
}

bool web_form_get_field(const char *body, const char *key, char *out, size_t out_len)
{
    if (body == NULL || key == NULL || out == NULL || out_len == 0) {
        return false;
    }
    size_t key_len = strlen(key);
    const char *p = body;
    while (*p != '\0') {
        const char *end = strchr(p, '&');
        size_t pair_len = end ? (size_t)(end - p) : strlen(p);
        if (pair_len > key_len && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            return url_decode(p + key_len + 1, pair_len - key_len - 1, out, out_len);
        }
        if (end == NULL) {
            break;
        }
        p = end + 1;
    }
    return false;
}

void web_form_trim(char *s)
{
    if (s == NULL) {
        return;
    }
    size_t start = 0;
    while (s[start] != '\0' && isspace((unsigned char)s[start])) {
        start++;
    }
    size_t len = strlen(s + start);
    while (len > 0 && isspace((unsigned char)s[start + len - 1])) {
        len--;
    }
    memmove(s, s + start, len);
    s[len] = '\0';
}

bool web_html_escape(const char *in, char *out, size_t out_len)
{
    if (out == NULL || out_len == 0) {
        return false;
    }
    size_t o = 0;
    out[0] = '\0';
    if (in == NULL) {
        return true;
    }
    for (; *in != '\0'; in++) {
        const char *rep = NULL;
        switch (*in) {
        case '&':  rep = "&amp;";  break;
        case '<':  rep = "&lt;";   break;
        case '>':  rep = "&gt;";   break;
        case '"':  rep = "&quot;"; break;
        case '\'': rep = "&#39;";  break;
        default:   break;
        }
        size_t add = rep ? strlen(rep) : 1;
        if (o + add >= out_len) {
            out[o] = '\0';
            return false;
        }
        if (rep) {
            memcpy(out + o, rep, add);
        } else {
            out[o] = *in;
        }
        o += add;
    }
    out[o] = '\0';
    return true;
}

// Hängt formatierten Text an; false, wenn buf voll ist
static bool append(char *buf, size_t len, size_t *pos, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *pos, len - *pos, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= len - *pos) {
        return false;
    }
    *pos += (size_t)n;
    return true;
}

int web_version_json(const char *version, char *buf, size_t len)
{
    if (version == NULL || buf == NULL || len == 0) {
        return -1;
    }
    // PEP-440-Versionen brauchen kein Escaping; alles Unerwartete ablehnen
    for (const char *c = version; *c != '\0'; c++) {
        if (*c == '"' || *c == '\\' || (unsigned char)*c < 0x20) {
            return -1;
        }
    }
    size_t pos = 0;
    return append(buf, len, &pos, "{\"version\":\"%s\"}", version) ? (int)pos : -1;
}

int web_error_json(const char *message, char *buf, size_t len)
{
    if (message == NULL || buf == NULL || len == 0) {
        return -1;
    }
    size_t pos = 0;
    if (!append(buf, len, &pos, "{\"error\":\"")) {
        return -1;
    }
    for (const char *c = message; *c != '\0'; c++) {
        if ((unsigned char)*c < 0x20) {
            return -1;
        }
        bool escape = *c == '"' || *c == '\\';
        if (!append(buf, len, &pos, escape ? "\\%c" : "%c", *c)) {
            return -1;
        }
    }
    return append(buf, len, &pos, "\"}") ? (int)pos : -1;
}

// Gemeinsames Stylesheet (im Flash, wird vom Browser gecacht).
// Hell/Dunkel automatisch über prefers-color-scheme.
static const char STYLE_CSS[] =
    ":root{--bg:#f4f5fb;--card:#fff;--fg:#1d2033;--mut:#6b7090;--line:#e3e5f0;"
    "--acc:#6150e8;--ok:#1f9d55;--bad:#d64545;color-scheme:light dark}"
    "@media(prefers-color-scheme:dark){:root{--bg:#12131c;--card:#1c1e2b;--fg:#e8e9f3;"
    "--mut:#9095b5;--line:#2c2f42;--acc:#8b7dff}}"
    "*{box-sizing:border-box}"
    "body{margin:0;background:var(--bg);color:var(--fg);"
    "font:16px/1.5 system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}"
    "header,main{max-width:34em;margin:0 auto;padding:1em}"
    "header{display:flex;flex-wrap:wrap;align-items:center;justify-content:space-between;gap:.6em}"
    // Titel in eigener Zeile mittig, darunter Menü links und Sprachwahl rechts
    ".brand{font-weight:700;display:flex;align-items:center;justify-content:center;gap:.5em;"
    "flex-basis:100%}"
    ".brand:before{content:'';width:1.1em;height:1.1em;border-radius:50%;"
    "background:conic-gradient(#f43,#fd3,#3d6,#3cf,#76f,#f4c,#f43)}"
    "nav{display:flex;gap:.2em;background:var(--card);border:1px solid var(--line);"
    "border-radius:99px;padding:.25em}"
    "nav a{padding:.35em .9em;border-radius:99px;color:var(--mut);text-decoration:none;"
    "font-weight:600;font-size:.9em}"
    "nav a.cur{background:var(--acc);color:#fff}"
    "nav.lang a{padding:.35em .6em}"
    "nav.sub{flex-wrap:wrap;width:fit-content;border-radius:14px;margin:0 0 1em}"
    "main{padding-top:0}"
    "h1{font-size:1.5em;margin:.2em 0 .6em}"
    "h2{font-size:.8em;margin:0 0 .6em;color:var(--mut);text-transform:uppercase;letter-spacing:.06em}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;"
    "padding:1.2em;margin-bottom:1em;box-shadow:0 2px 8px #0000000d}"
    "label{display:block;margin-top:.9em;font-weight:600;font-size:.9em}"
    "input[type=text],input[type=password],input[type=number],select{display:block;width:100%;margin-top:.3em;"
    "padding:.65em .8em;font:inherit;font-weight:400;color:inherit;background:var(--bg);"
    "border:1px solid var(--line);border-radius:10px}"
    "input:focus{outline:2px solid var(--acc);outline-offset:1px}"
    "label.chk{display:flex;align-items:center;gap:.5em;font-weight:400}"
    "input[type=checkbox],input[type=radio]{accent-color:var(--acc);width:1.1em;height:1.1em;margin:0}"
    "button{font:inherit;font-weight:600;cursor:pointer;border:0;border-radius:10px;"
    "padding:.7em 1.4em;margin-top:1.2em;width:100%;background:var(--acc);color:#fff}"
    ".msg{padding:.7em 1em;border-radius:10px;border-left:4px solid var(--ok);"
    "background:#1f9d551f;margin:0 0 1em}"
    ".err{border-color:var(--bad);background:#d645451f}"
    ".msg:empty{display:none}"
    "dl{display:grid;grid-template-columns:auto 1fr;gap:.3em 1em;margin:0}"
    "dt{color:var(--mut)}dd{margin:0;font-weight:600;word-break:break-all}"
    "p{margin:0}small{display:block;color:var(--mut);font-size:.85em;margin-top:.8em}"
    ".sw{display:flex;flex-wrap:wrap;gap:1em;margin:1.2em 0 0}"
    ".sw div{position:relative}"
    ".sw input,.sw .add{display:block;width:3.2em;height:3.2em;padding:0;margin:0;"
    "border-radius:50%;cursor:pointer}"
    ".sw input{border:2px solid var(--line);background:none}"
    ".sw input::-webkit-color-swatch-wrapper{padding:0}"
    ".sw input::-webkit-color-swatch{border:0;border-radius:50%}"
    ".sw input::-moz-color-swatch{border:0;border-radius:50%}"
    ".sw .x{position:absolute;top:-.35em;right:-.35em;width:1.6em;height:1.6em;padding:0;"
    "margin:0;border-radius:50%;background:var(--card);color:var(--fg);"
    "border:1px solid var(--line);font-size:.75em;line-height:1}"
    ".sw .add{border:2px dashed var(--mut);background:none;color:var(--mut);"
    "font-size:1.4em;line-height:1}";

const char *web_style_css(void)
{
    return STYLE_CSS;
}

// Seiten im Menü
typedef enum {
    PAGE_SETTINGS,
    PAGE_COLORS,
    PAGE_STATUS,
} page_t;

// Texte der Seiten je Sprache. *_fmt sind printf-Formate; beide Sprachen
// müssen dieselben Platzhalter in derselben Reihenfolge haben.
typedef struct {
    const char *settings;
    const char *colors;
    const char *status;
    const char *mode;
    const char *mode_client;
    const char *mode_ap_receiver;
    const char *mode_ap;
    const char *espnow_mac;
    const char *version;
    const char *wifi;
    const char *wifi_ssid;
    const char *wifi_password;
    const char *password_keep;
    const char *open_wifi;
    const char *save_restart;
    const char *ssid_hint;
    const char *ble_name;
    const char *ble_hint_fmt;          // %d (max. Länge)
    const char *espnow_role_gateway;
    const char *espnow_peers;
    const char *espnow_role_receiver;
    const char *espnow_gateway;
    const char *espnow_hint_fmt;       // %s (eigene MAC), %d (max. Empfänger), %u (Sekunden)
    const char *led_strip;
    const char *led_gpio;
    const char *led_count;
    const char *led_hint_fmt;          // %d, %d (min., max. Anzahl)
    const char *colors_intro;
    const char *color_add;
    const char *save;
    const char *colors_hint_fmt;       // %d (max. Farben)
    // Texte im JavaScript der Farben-Seite (ohne ' und \)
    const char *js_remove;
    const char *js_load_failed;
    const char *js_max_before;         // 'Höchstens '+MAX+' Farben.'
    const char *js_max_after;
    const char *js_saved;
    const char *js_error;
    const char *js_save_failed;
} page_text_t;

static const page_text_t TEXT_DE = {
    .settings = "Einstellungen",
    .colors = "Farben",
    .status = "Status",
    .mode = "Modus",
    .mode_client = "WLAN-Client",
    .mode_ap_receiver = "Access Point, danach ESP-NOW-Empfänger",
    .mode_ap = "Access Point",
    .espnow_mac = "ESP-NOW-MAC",
    .version = "Version",
    .wifi = "WLAN",
    .wifi_ssid = "WLAN (SSID)",
    .wifi_password = "WLAN-Passwort",
    .password_keep = "leer lassen = unverändert",
    .open_wifi = "offenes WLAN (kein Passwort)",
    .save_restart = "Speichern und neu starten",
    .ssid_hint = "SSID leer lassen = nur Access Point.",
    .ble_name = "Gerätename",
    .ble_hint_fmt = "Unter diesem Namen lassen sich die Farben per Bluetooth setzen. "
                    "1 bis %d Zeichen, keine Umlaute.",
    .espnow_role_gateway = "Gateway: Farben per WLAN/Bluetooth annehmen und weiterleiten",
    .espnow_peers = "Empfänger (MAC-Adressen, mit Komma getrennt)",
    .espnow_role_receiver = "Empfänger: Farben nur per ESP-NOW vom Gateway",
    .espnow_gateway = "Gateway (MAC-Adresse)",
    .espnow_hint_fmt = "Eigene MAC: %s. Höchstens %d Empfänger. Ein Empfänger öffnet nach dem "
                       "Start %u s lang seinen Access Point mit dieser Seite; solange ein Gerät "
                       "verbunden ist, bleibt er offen. Danach sucht er den Kanal des Gateways.",
    .led_strip = "LED-Strip",
    .led_gpio = "Datenleitung (GPIO)",
    .led_count = "Anzahl LEDs",
    .led_hint_fmt = "%d bis %d LEDs. Mehr LEDs brauchen mehr Strom; die Helligkeit ist "
                    "wegen der USB-Versorgung begrenzt.",
    .colors_intro = "Jede LED wählt zufällig eine dieser Farben, blendet sie ein und wieder aus.",
    .color_add = "Farbe hinzufügen",
    .save = "Speichern",
    .colors_hint_fmt = "Höchstens %d Farben. Keine Farbe = zufällige Farben. "
                       "Die Helligkeit wird begrenzt, dunkle Farben sind daher kaum sichtbar.",
    .js_remove = "Entfernen",
    .js_load_failed = "Laden fehlgeschlagen.",
    .js_max_before = "Höchstens ",
    .js_max_after = " Farben.",
    .js_saved = "Gespeichert.",
    .js_error = "Fehler.",
    .js_save_failed = "Speichern fehlgeschlagen.",
};

static const page_text_t TEXT_EN = {
    .settings = "Settings",
    .colors = "Colors",
    .status = "Status",
    .mode = "Mode",
    .mode_client = "Wi-Fi client",
    .mode_ap_receiver = "Access point, then ESP-NOW receiver",
    .mode_ap = "Access point",
    .espnow_mac = "ESP-NOW MAC",
    .version = "Version",
    .wifi = "Wi-Fi",
    .wifi_ssid = "Wi-Fi (SSID)",
    .wifi_password = "Wi-Fi password",
    .password_keep = "leave empty = unchanged",
    .open_wifi = "open Wi-Fi (no password)",
    .save_restart = "Save and restart",
    .ssid_hint = "Leave SSID empty = access point only.",
    .ble_name = "Device name",
    .ble_hint_fmt = "Colors can be set via Bluetooth under this name. "
                    "1 to %d characters, ASCII only.",
    .espnow_role_gateway = "Gateway: accept colors via Wi-Fi/Bluetooth and forward them",
    .espnow_peers = "Receivers (MAC addresses, separated by commas)",
    .espnow_role_receiver = "Receiver: colors only via ESP-NOW from the gateway",
    .espnow_gateway = "Gateway (MAC address)",
    .espnow_hint_fmt = "Own MAC: %s. At most %d receivers. After starting, a receiver opens its "
                       "access point with this page for %u s; it stays open while a device is "
                       "connected. Then it searches for the gateway's channel.",
    .led_strip = "LED strip",
    .led_gpio = "Data line (GPIO)",
    .led_count = "Number of LEDs",
    .led_hint_fmt = "%d to %d LEDs. More LEDs need more current; brightness is limited "
                    "because of the USB power supply.",
    .colors_intro = "Each LED randomly picks one of these colors, fades it in and out again.",
    .color_add = "Add color",
    .save = "Save",
    .colors_hint_fmt = "At most %d colors. No color = random colors. "
                       "Brightness is limited, so dark colors are barely visible.",
    .js_remove = "Remove",
    .js_load_failed = "Loading failed.",
    .js_max_before = "At most ",
    .js_max_after = " colors.",
    .js_saved = "Saved.",
    .js_error = "Error.",
    .js_save_failed = "Saving failed.",
};

static const page_text_t *page_text(web_lang_t lang)
{
    return lang == WEB_LANG_EN ? &TEXT_EN : &TEXT_DE;
}

// Link des Sprachschalters: aktuelle Sprache markiert, die andere setzt das
// Cookie und lädt die Seite (per GET) neu
static bool append_lang_link(char *buf, size_t len, size_t *pos, web_lang_t link,
                             web_lang_t current, const char *href)
{
    const char *label = link == WEB_LANG_EN ? "EN" : "DE";
    const char *title = link == WEB_LANG_EN ? "English" : "Deutsch";
    if (link == current) {
        return append(buf, len, pos, "<a class=\"cur\" title=\"%s\">%s</a>", title, label);
    }
    return append(buf, len, pos,
        "<a href=\"%s\" title=\"%s\" onclick=\"document.cookie='" WEB_LANG_COOKIE "=%s;path=/;"
        "max-age=" WEB_LANG_COOKIE_AGE ";samesite=lax'\">%s</a>",
        href, title, web_lang_code(link), label);
}

// Gemeinsamer Kopf aller Seiten: Stylesheet, Kopfleiste mit Menü (aktuelle
// Seite markiert, Farben nur mit show_colors) und Sprachschalter (lädt href neu),
// Beginn des Inhalts
static bool append_head(char *buf, size_t len, size_t *pos, page_t page, bool show_colors,
                        web_lang_t lang, const char *href)
{
    const page_text_t *t = page_text(lang);
    bool ok = append(buf, len, pos,
                  "<!DOCTYPE html><html lang=\"%s\"><head><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                  "<title>" WEB_TITLE "</title><link rel=\"stylesheet\" href=\"/style.css\">"
                  "</head><body><header><span class=\"brand\">" WEB_TITLE "</span><nav>",
                  web_lang_code(lang));
    if (ok && show_colors) {
        ok = append(buf, len, pos, "<a href=\"/colors\"%s>%s</a>",
                    page == PAGE_COLORS ? " class=\"cur\"" : "", t->colors);
    }
    return ok &&
           append(buf, len, pos,
                  "<a href=\"/status\"%s>%s</a><a href=\"/wifi\"%s>%s</a></nav><nav class=\"lang\">",
                  page == PAGE_STATUS ? " class=\"cur\"" : "", t->status,
                  page == PAGE_SETTINGS ? " class=\"cur\"" : "", t->settings) &&
           append_lang_link(buf, len, pos, WEB_LANG_DE, lang, href) &&
           append_lang_link(buf, len, pos, WEB_LANG_EN, lang, href) &&
           append(buf, len, pos, "</nav></header><main>");
}

int web_status_page_render(char *buf, size_t len, const web_page_info_t *info)
{
    if (buf == NULL || len == 0 || info == NULL) {
        return -1;
    }
    const page_text_t *t = page_text(info->lang);

    // Texte maskieren (Hostname kommt vom Benutzer)
    char ip[64], hostname[200], version[96];
    char own_mac[ESPNOW_MAC_STR_LEN];
    if (!web_html_escape(info->ip ? info->ip : "-", ip, sizeof(ip)) ||
        !web_html_escape(info->hostname, hostname, sizeof(hostname)) ||
        !web_html_escape(info->own_mac, own_mac, sizeof(own_mac)) ||
        !web_html_escape(info->version, version, sizeof(version))) {
        return -1;
    }

    size_t pos = 0;
    bool ok = append_head(buf, len, &pos, PAGE_STATUS, !info->receiver_mode, info->lang, "/status") &&
              append(buf, len, &pos,
                     "<h1>%s</h1><div class=\"card\"><dl>"
                     "<dt>%s</dt><dd>%s</dd><dt>IP</dt><dd>%s</dd><dt>Hostname</dt><dd>%s</dd>",
                     t->status, t->mode,
                     info->client_mode ? t->mode_client
                                       : (info->receiver_mode ? t->mode_ap_receiver : t->mode_ap),
                     ip, hostname);
    if (ok && info->own_mac != NULL) {
        ok = append(buf, len, &pos, "<dt>%s</dt><dd>%s</dd>", t->espnow_mac, own_mac);
    }
    if (ok && info->version != NULL) {
        ok = append(buf, len, &pos, "<dt>%s</dt><dd>%s</dd>", t->version, version);
    }
    ok = ok && append(buf, len, &pos, "</dl></div></main></body></html>");
    return ok ? (int)pos : -1;
}

// Einträge des Untermenüs der Einstellungen (Reihenfolge = Menü)
typedef struct {
    web_settings_page_t page;
    const char *href;
} settings_link_t;

static const settings_link_t SETTINGS_LINKS[] = {
    {WEB_SETTINGS_WIFI, "/wifi"},
    {WEB_SETTINGS_BLE, "/ble"},
    {WEB_SETTINGS_ESPNOW, "/espnow"},
    {WEB_SETTINGS_LED, "/led"},
};

// Gibt es die Unterseite mit diesen Daten? WLAN immer, sonst nur mit ihren Angaben
static bool settings_available(web_settings_page_t page, const web_page_info_t *info)
{
    switch (page) {
    case WEB_SETTINGS_BLE:
        return info->ble_name != NULL;
    case WEB_SETTINGS_ESPNOW:
        return info->own_mac != NULL;
    case WEB_SETTINGS_LED:
        return info->led_gpio_mask != 0;
    default:
        return true;
    }
}

static const char *settings_label(web_settings_page_t page, const page_text_t *t)
{
    switch (page) {
    case WEB_SETTINGS_BLE:
        return "Bluetooth";
    case WEB_SETTINGS_ESPNOW:
        return "ESP-NOW";
    case WEB_SETTINGS_LED:
        return t->led_strip;
    default:
        return t->wifi;
    }
}

int web_page_render(char *buf, size_t len, const web_page_info_t *info)
{
    if (buf == NULL || len == 0 || info == NULL) {
        return -1;
    }
    const page_text_t *t = page_text(info->lang);
    // Nicht vorhandene Unterseite -> WLAN
    web_settings_page_t page = info->settings_page;
    if ((unsigned)page >= sizeof(SETTINGS_LINKS) / sizeof(SETTINGS_LINKS[0]) ||
        !settings_available(page, info)) {
        page = WEB_SETTINGS_WIFI;
    }

    // Alle Texte vor dem Einsetzen maskieren (SSID darf z. B. < oder " enthalten)
    char hostname[200], ssid[200], ble_name[128], message[400];
    // MAC-Texte enthalten nur Hex-Ziffern, Doppelpunkte und Kommas
    char own_mac[ESPNOW_MAC_STR_LEN], gateway[ESPNOW_MAC_STR_LEN], peers[ESPNOW_PEER_LIST_STR_MAX];
    if (!web_html_escape(info->own_mac, own_mac, sizeof(own_mac)) ||
        !web_html_escape(info->espnow_gateway, gateway, sizeof(gateway)) ||
        !web_html_escape(info->espnow_peers, peers, sizeof(peers)) ||
        !web_html_escape(info->ble_name, ble_name, sizeof(ble_name)) ||
        !web_html_escape(info->hostname, hostname, sizeof(hostname)) ||
        !web_html_escape(info->ssid, ssid, sizeof(ssid)) ||
        !web_html_escape(info->message, message, sizeof(message))) {
        return -1;
    }

    size_t pos = 0;
    bool ok = append_head(buf, len, &pos, PAGE_SETTINGS, !info->receiver_mode, info->lang,
                          SETTINGS_LINKS[page].href) &&
              append(buf, len, &pos, "<nav class=\"sub\">");
    // Untermenü: nur vorhandene Unterseiten, aktuelle markiert
    for (size_t i = 0; ok && i < sizeof(SETTINGS_LINKS) / sizeof(SETTINGS_LINKS[0]); i++) {
        const settings_link_t *l = &SETTINGS_LINKS[i];
        if (settings_available(l->page, info)) {
            ok = append(buf, len, &pos, "<a href=\"%s\"%s>%s</a>", l->href,
                        l->page == page ? " class=\"cur\"" : "", settings_label(l->page, t));
        }
    }
    ok = ok && append(buf, len, &pos, "</nav><h1>%s</h1>", t->settings);
    if (ok && message[0] != '\0') {
        ok = append(buf, len, &pos, "<p class=\"msg%s\">%s</p>",
                    info->message_is_error ? " err" : "", message);
    }
    switch (page) {
    case WEB_SETTINGS_WIFI:
        ok = ok && append(buf, len, &pos,
            "<form class=\"card\" method=\"post\" action=\"/config\"><h2>%s</h2>"
            "<label>%s<input type=\"text\" name=\"ssid\" maxlength=\"32\" value=\"%s\"></label>"
            "<label>%s<input type=\"password\" name=\"password\" maxlength=\"63\" "
            "placeholder=\"%s\"></label>"
            "<label class=\"chk\"><input type=\"checkbox\" name=\"open\" value=\"1\"> %s</label>"
            "<label>Hostname<input type=\"text\" name=\"hostname\" maxlength=\"32\" value=\"%s\"></label>"
            "<button type=\"submit\">%s</button>"
            "<small>%s</small></form>",
            t->wifi, t->wifi_ssid, ssid, t->wifi_password, t->password_keep, t->open_wifi,
            hostname, t->save_restart, t->ssid_hint);
        break;
    case WEB_SETTINGS_BLE:
        ok = ok && append(buf, len, &pos,
                 "<form class=\"card\" method=\"post\" action=\"/ble\"><h2>Bluetooth</h2>"
                 "<label>%s<input type=\"text\" name=\"ble_name\" maxlength=\"%d\" value=\"%s\"></label>"
                 "<button type=\"submit\">%s</button><small>",
                 t->ble_name, NET_CONFIG_BLE_NAME_MAX, ble_name, t->save_restart) &&
             append(buf, len, &pos, t->ble_hint_fmt, NET_CONFIG_BLE_NAME_MAX) &&
             append(buf, len, &pos, "</small></form>");
        break;
    case WEB_SETTINGS_ESPNOW:
        ok = ok && append(buf, len, &pos,
                 "<form class=\"card\" method=\"post\" action=\"/espnow\"><h2>ESP-NOW</h2>"
                 "<label class=\"chk\"><input type=\"radio\" name=\"espnow_role\" value=\"gateway\"%s>"
                 " %s</label>"
                 "<label>%s<input type=\"text\" name=\"peers\" "
                 "maxlength=\"%d\" value=\"%s\" placeholder=\"aa:bb:cc:dd:ee:ff, ...\"></label>"
                 "<label class=\"chk\"><input type=\"radio\" name=\"espnow_role\" value=\"receiver\"%s>"
                 " %s</label>"
                 "<label>%s<input type=\"text\" name=\"gateway\" maxlength=\"%d\" "
                 "value=\"%s\" placeholder=\"aa:bb:cc:dd:ee:ff\"></label>"
                 "<button type=\"submit\">%s</button><small>",
                 info->espnow_receiver ? "" : " checked", t->espnow_role_gateway,
                 t->espnow_peers, ESPNOW_PEER_LIST_STR_MAX, peers,
                 info->espnow_receiver ? " checked" : "", t->espnow_role_receiver,
                 t->espnow_gateway, ESPNOW_MAC_STR_LEN - 1, gateway, t->save_restart) &&
             append(buf, len, &pos, t->espnow_hint_fmt, own_mac, ESPNOW_PEERS_MAX,
                    info->espnow_window_s) &&
             append(buf, len, &pos, "</small></form>");
        break;
    case WEB_SETTINGS_LED:
        ok = ok && append(buf, len, &pos,
            "<form class=\"card\" method=\"post\" action=\"/led\"><h2>%s</h2>"
            "<label>%s<select name=\"led_gpio\">", t->led_strip, t->led_gpio);
        for (unsigned gpio = 0; ok && gpio < 64; gpio++) {
            if (led_config_gpio_allowed((int)gpio, info->led_gpio_mask)) {
                ok = append(buf, len, &pos, "<option value=\"%u\"%s>GPIO %u</option>",
                            gpio, gpio == info->led_gpio ? " selected" : "", gpio);
            }
        }
        ok = ok && append(buf, len, &pos,
                     "</select></label>"
                     "<label>%s<input type=\"number\" name=\"led_count\" min=\"%d\" max=\"%d\" "
                     "value=\"%u\"></label>"
                     "<button type=\"submit\">%s</button><small>",
                     t->led_count, LED_CONFIG_COUNT_MIN, LED_CONFIG_COUNT_MAX, info->led_count,
                     t->save_restart) &&
             append(buf, len, &pos, t->led_hint_fmt, LED_CONFIG_COUNT_MIN, LED_CONFIG_COUNT_MAX) &&
             append(buf, len, &pos, "</small></form>");
        break;
    }
    ok = ok && append(buf, len, &pos, "</main></body></html>");
    return ok ? (int)pos : -1;
}

int web_colors_page_render_lang(char *buf, size_t len, web_lang_t lang)
{
    if (buf == NULL || len == 0) {
        return -1;
    }
    const page_text_t *t = page_text(lang);
    size_t pos = 0;
    bool ok = append_head(buf, len, &pos, PAGE_COLORS, true, lang, "/colors");
    // Die Seite lädt und speichert die Palette über GET/PUT /api/colors
    ok = ok && append(buf, len, &pos,
        "<h1>%s</h1><p id=\"msg\" class=\"msg\"></p>"
        "<div class=\"card\"><p>%s</p>"
        "<div id=\"list\" class=\"sw\"><button type=\"button\" id=\"add\" class=\"add\" "
        "title=\"%s\">+</button></div>"
        "<button type=\"button\" id=\"save\">%s</button><small>",
        t->colors, t->colors_intro, t->color_add, t->save);
    ok = ok && append(buf, len, &pos, t->colors_hint_fmt, LED_PALETTE_MAX);
    ok = ok && append(buf, len, &pos,
        "</small></div>"
        "<script>"
        "var MAX=%d,L=document.getElementById('list'),A=document.getElementById('add'),"
        "M=document.getElementById('msg');"
        "function msg(t,e){M.textContent=t;M.className='msg'+(e?' err':'')}"
        "function row(c){var d=document.createElement('div'),i=document.createElement('input'),"
        "x=document.createElement('button');i.type='color';i.value=c;"
        "x.type='button';x.className='x';x.textContent='×';x.title='%s';"
        "x.onclick=function(){d.remove()};d.append(i,x);L.insertBefore(d,A)}"
        "fetch('/api/colors').then(function(r){return r.json()})"
        ".then(function(j){j.colors.forEach(row)})"
        ".catch(function(){msg('%s',1)});"
        "A.onclick=function(){if(L.querySelectorAll('input').length<MAX)"
        "row('#'+('00000'+(Math.random()*16777216|0).toString(16)).slice(-6));"
        "else msg('%s'+MAX+'%s',1)};"
        "document.getElementById('save').onclick=function(){"
        "var c=[].map.call(L.querySelectorAll('input'),function(i){return i.value});"
        "fetch('/api/colors',{method:'PUT',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({colors:c})}).then(function(r){return r.json().then(function(j){"
        "if(r.ok)msg('%s');else msg(j.error||'%s',1)})})"
        ".catch(function(){msg('%s',1)})};"
        "</script></main></body></html>",
        LED_PALETTE_MAX, t->js_remove, t->js_load_failed, t->js_max_before, t->js_max_after,
        t->js_saved, t->js_error, t->js_save_failed);
    return ok ? (int)pos : -1;
}

int web_colors_page_render(char *buf, size_t len)
{
    return web_colors_page_render_lang(buf, len, WEB_LANG_DE);
}
