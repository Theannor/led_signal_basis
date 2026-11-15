// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Sprachen der Weboberfläche
typedef enum {
    WEB_LANG_DE = 0,   // Standard, wenn der Browser nichts angibt
    WEB_LANG_EN,
} web_lang_t;

// Name des Cookies, mit dem der Sprachschalter die Wahl im Browser merkt
#define WEB_LANG_COOKIE  "lang"

// Hinweise und Fehlermeldungen der Weboberfläche
typedef enum {
    WEB_MSG_NONE = 0,         // kein Hinweis (web_msg() liefert NULL)
    WEB_MSG_SAVED_RESTART,
    WEB_MSG_SAVE_FAILED,
    WEB_MSG_BAD_REQUEST,      // Body zu groß oder unvollständig
    WEB_MSG_SSID,
    WEB_MSG_PASSWORD,
    WEB_MSG_HOSTNAME,
    WEB_MSG_INVALID,
    WEB_MSG_BLE_NAME,
    WEB_MSG_ESPNOW_PEERS,
    WEB_MSG_ESPNOW_GATEWAY,
    WEB_MSG_ESPNOW_ROLE,
    WEB_MSG_LED,
    WEB_MSG_RECEIVER,         // Gerät ist ESP-NOW-Empfänger
    WEB_MSG_PALETTE_FORMAT,   // PUT /api/colors mit falschem JSON
    WEB_MSG_INTERNAL,
    WEB_MSG_VERSION_INVALID,
    WEB_MSG_COUNT,
} web_msg_t;

// Sprachcode für <html lang>: "de" oder "en"
const char *web_lang_code(web_lang_t lang);

// "de" bzw. "en" (Groß-/Kleinschreibung egal) nach out. false bei anderem Text oder NULL.
bool web_lang_parse(const char *code, web_lang_t *out);

// Sucht das Sprach-Cookie in einem Cookie-Header ("a=b; lang=en").
// false, wenn es fehlt oder keine bekannte Sprache enthält.
bool web_lang_from_cookie(const char *cookie, web_lang_t *out);

// Wählt die Sprache aus einem Accept-Language-Header (mit q-Werten).
// Kein oder leerer Header -> Deutsch; nennt der Browser weder Deutsch noch
// Englisch -> Englisch.
web_lang_t web_lang_from_accept(const char *accept);

// Sprache einer Anfrage: Cookie (Schalter auf der Seite) vor Accept-Language.
// Beide Header dürfen NULL sein.
web_lang_t web_lang_select(const char *cookie, const char *accept);

// Text einer Meldung in der Sprache; NULL für WEB_MSG_NONE oder ungültige Werte
const char *web_msg(web_lang_t lang, web_msg_t msg);

#ifdef __cplusplus
}
#endif
