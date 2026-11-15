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
    ".brand{font-weight:700;display:flex;align-items:center;gap:.5em}"
    ".brand:before{content:'';width:1.1em;height:1.1em;border-radius:50%;"
    "background:conic-gradient(#f43,#fd3,#3d6,#3cf,#76f,#f4c,#f43)}"
    "nav{display:flex;gap:.2em;background:var(--card);border:1px solid var(--line);"
    "border-radius:99px;padding:.25em}"
    "nav a{padding:.35em .9em;border-radius:99px;color:var(--mut);text-decoration:none;"
    "font-weight:600;font-size:.9em}"
    "nav a.cur{background:var(--acc);color:#fff}"
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
} page_t;

// Gemeinsamer Kopf aller Seiten: Stylesheet, Kopfleiste mit Menü (aktuelle
// Seite markiert, Farben nur mit show_colors), Beginn des Inhalts
static bool append_head(char *buf, size_t len, size_t *pos, page_t page, bool show_colors)
{
    return append(buf, len, pos,
        "<!DOCTYPE html><html lang=\"de\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>" WEB_TITLE "</title><link rel=\"stylesheet\" href=\"/style.css\">"
        "</head><body><header><span class=\"brand\">" WEB_TITLE "</span>"
        "<nav>%s%s%s<a href=\"/\"%s>Einstellungen</a></nav>"
        "</header><main>",
        show_colors ? "<a href=\"/colors\"" : "",
        show_colors ? (page == PAGE_COLORS ? " class=\"cur\">" : ">") : "",
        show_colors ? "Farben</a>" : "",
        page == PAGE_SETTINGS ? " class=\"cur\"" : "");
}

int web_page_render(char *buf, size_t len, const web_page_info_t *info)
{
    if (buf == NULL || len == 0 || info == NULL) {
        return -1;
    }

    // Alle Texte vor dem Einsetzen maskieren (SSID darf z. B. < oder " enthalten)
    char ip[64], hostname[200], ssid[200], ble_name[128], message[400], version[96];
    // MAC-Texte enthalten nur Hex-Ziffern, Doppelpunkte und Kommas
    char own_mac[ESPNOW_MAC_STR_LEN], gateway[ESPNOW_MAC_STR_LEN], peers[ESPNOW_PEER_LIST_STR_MAX];
    if (!web_html_escape(info->ip ? info->ip : "-", ip, sizeof(ip)) ||
        !web_html_escape(info->own_mac, own_mac, sizeof(own_mac)) ||
        !web_html_escape(info->espnow_gateway, gateway, sizeof(gateway)) ||
        !web_html_escape(info->espnow_peers, peers, sizeof(peers)) ||
        !web_html_escape(info->ble_name, ble_name, sizeof(ble_name)) ||
        !web_html_escape(info->hostname, hostname, sizeof(hostname)) ||
        !web_html_escape(info->ssid, ssid, sizeof(ssid)) ||
        !web_html_escape(info->message, message, sizeof(message)) ||
        !web_html_escape(info->version, version, sizeof(version))) {
        return -1;
    }

    size_t pos = 0;
    bool ok = append_head(buf, len, &pos, PAGE_SETTINGS, !info->receiver_mode) &&
              append(buf, len, &pos, "<h1>Einstellungen</h1>");
    if (ok && message[0] != '\0') {
        ok = append(buf, len, &pos, "<p class=\"msg%s\">%s</p>",
                    info->message_is_error ? " err" : "", message);
    }
    ok = ok && append(buf, len, &pos,
        "<div class=\"card\"><h2>Status</h2><dl>"
        "<dt>Modus</dt><dd>%s</dd><dt>IP</dt><dd>%s</dd><dt>Hostname</dt><dd>%s</dd>",
        info->client_mode ? "WLAN-Client"
                          : (info->receiver_mode ? "Access Point, danach ESP-NOW-Empfänger"
                                                 : "Access Point"),
        ip, hostname);
    if (ok && info->own_mac != NULL) {
        ok = append(buf, len, &pos, "<dt>ESP-NOW-MAC</dt><dd>%s</dd>", own_mac);
    }
    if (ok && info->version != NULL) {
        ok = append(buf, len, &pos, "<dt>Version</dt><dd>%s</dd>", version);
    }
    ok = ok && append(buf, len, &pos, "</dl></div>");
    ok = ok && append(buf, len, &pos,
        "<form class=\"card\" method=\"post\" action=\"/config\"><h2>WLAN</h2>"
        "<label>WLAN (SSID)<input type=\"text\" name=\"ssid\" maxlength=\"32\" value=\"%s\"></label>"
        "<label>WLAN-Passwort<input type=\"password\" name=\"password\" maxlength=\"63\" "
        "placeholder=\"leer lassen = unverändert\"></label>"
        "<label class=\"chk\"><input type=\"checkbox\" name=\"open\" value=\"1\"> offenes WLAN (kein Passwort)</label>"
        "<label>Hostname<input type=\"text\" name=\"hostname\" maxlength=\"32\" value=\"%s\"></label>"
        "<button type=\"submit\">Speichern und neu starten</button>"
        "<small>SSID leer lassen = nur Access Point.</small></form>",
        ssid, hostname);
    if (ok && info->ble_name != NULL) {
        ok = append(buf, len, &pos,
            "<form class=\"card\" method=\"post\" action=\"/ble\"><h2>Bluetooth</h2>"
            "<label>Gerätename<input type=\"text\" name=\"ble_name\" maxlength=\"%d\" value=\"%s\"></label>"
            "<button type=\"submit\">Speichern und neu starten</button>"
            "<small>Unter diesem Namen lassen sich die Farben per Bluetooth setzen. "
            "1 bis %d Zeichen, keine Umlaute.</small></form>",
            NET_CONFIG_BLE_NAME_MAX, ble_name, NET_CONFIG_BLE_NAME_MAX);
    }
    if (ok && info->own_mac != NULL) {
        ok = append(buf, len, &pos,
            "<form class=\"card\" method=\"post\" action=\"/espnow\"><h2>ESP-NOW</h2>"
            "<label class=\"chk\"><input type=\"radio\" name=\"espnow_role\" value=\"gateway\"%s>"
            " Gateway: Farben per WLAN/Bluetooth annehmen und weiterleiten</label>"
            "<label>Empfänger (MAC-Adressen, mit Komma getrennt)<input type=\"text\" name=\"peers\" "
            "maxlength=\"%d\" value=\"%s\" placeholder=\"aa:bb:cc:dd:ee:ff, ...\"></label>"
            "<label class=\"chk\"><input type=\"radio\" name=\"espnow_role\" value=\"receiver\"%s>"
            " Empfänger: Farben nur per ESP-NOW vom Gateway</label>"
            "<label>Gateway (MAC-Adresse)<input type=\"text\" name=\"gateway\" maxlength=\"%d\" "
            "value=\"%s\" placeholder=\"aa:bb:cc:dd:ee:ff\"></label>"
            "<button type=\"submit\">Speichern und neu starten</button>"
            "<small>Eigene MAC: %s. Höchstens %d Empfänger. Ein Empfänger öffnet nach dem "
            "Start %u s lang seinen Access Point mit dieser Seite; solange ein Gerät "
            "verbunden ist, bleibt er offen. Danach sucht er den Kanal des Gateways.</small></form>",
            info->espnow_receiver ? "" : " checked",
            ESPNOW_PEER_LIST_STR_MAX, peers,
            info->espnow_receiver ? " checked" : "",
            ESPNOW_MAC_STR_LEN - 1, gateway,
            own_mac, ESPNOW_PEERS_MAX, info->espnow_window_s);
    }
    if (ok && info->led_gpio_mask != 0) {
        ok = append(buf, len, &pos,
            "<form class=\"card\" method=\"post\" action=\"/led\"><h2>LED-Strip</h2>"
            "<label>Datenleitung (GPIO)<select name=\"led_gpio\">");
        for (unsigned gpio = 0; ok && gpio < 64; gpio++) {
            if (led_config_gpio_allowed((int)gpio, info->led_gpio_mask)) {
                ok = append(buf, len, &pos, "<option value=\"%u\"%s>GPIO %u</option>",
                            gpio, gpio == info->led_gpio ? " selected" : "", gpio);
            }
        }
        ok = ok && append(buf, len, &pos,
            "</select></label>"
            "<label>Anzahl LEDs<input type=\"number\" name=\"led_count\" min=\"%d\" max=\"%d\" "
            "value=\"%u\"></label>"
            "<button type=\"submit\">Speichern und neu starten</button>"
            "<small>%d bis %d LEDs. Mehr LEDs brauchen mehr Strom; die Helligkeit ist "
            "wegen der USB-Versorgung begrenzt.</small></form>",
            LED_CONFIG_COUNT_MIN, LED_CONFIG_COUNT_MAX, info->led_count,
            LED_CONFIG_COUNT_MIN, LED_CONFIG_COUNT_MAX);
    }
    ok = ok && append(buf, len, &pos, "</main></body></html>");
    return ok ? (int)pos : -1;
}

int web_colors_page_render(char *buf, size_t len)
{
    if (buf == NULL || len == 0) {
        return -1;
    }
    size_t pos = 0;
    bool ok = append_head(buf, len, &pos, PAGE_COLORS, true);
    // Die Seite lädt und speichert die Palette über GET/PUT /api/colors
    ok = ok && append(buf, len, &pos,
        "<h1>Farben</h1><p id=\"msg\" class=\"msg\"></p>"
        "<div class=\"card\"><p>Jede LED wählt zufällig eine dieser Farben, "
        "blendet sie ein und wieder aus.</p>"
        "<div id=\"list\" class=\"sw\"><button type=\"button\" id=\"add\" class=\"add\" "
        "title=\"Farbe hinzufügen\">+</button></div>"
        "<button type=\"button\" id=\"save\">Speichern</button>"
        "<small>Höchstens %d Farben. Keine Farbe = zufällige Farben. "
        "Die Helligkeit wird begrenzt, dunkle Farben sind daher kaum sichtbar.</small></div>"
        "<script>"
        "var MAX=%d,L=document.getElementById('list'),A=document.getElementById('add'),"
        "M=document.getElementById('msg');"
        "function msg(t,e){M.textContent=t;M.className='msg'+(e?' err':'')}"
        "function row(c){var d=document.createElement('div'),i=document.createElement('input'),"
        "x=document.createElement('button');i.type='color';i.value=c;"
        "x.type='button';x.className='x';x.textContent='×';x.title='Entfernen';"
        "x.onclick=function(){d.remove()};d.append(i,x);L.insertBefore(d,A)}"
        "fetch('/api/colors').then(function(r){return r.json()})"
        ".then(function(j){j.colors.forEach(row)})"
        ".catch(function(){msg('Laden fehlgeschlagen.',1)});"
        "A.onclick=function(){if(L.querySelectorAll('input').length<MAX)"
        "row('#'+('00000'+(Math.random()*16777216|0).toString(16)).slice(-6));"
        "else msg('Höchstens '+MAX+' Farben.',1)};"
        "document.getElementById('save').onclick=function(){"
        "var c=[].map.call(L.querySelectorAll('input'),function(i){return i.value});"
        "fetch('/api/colors',{method:'PUT',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({colors:c})}).then(function(r){return r.json().then(function(j){"
        "if(r.ok)msg('Gespeichert.');else msg(j.error||'Fehler.',1)})})"
        ".catch(function(){msg('Speichern fehlgeschlagen.',1)})};"
        "</script></main></body></html>",
        LED_PALETTE_MAX, LED_PALETTE_MAX);
    return ok ? (int)pos : -1;
}
