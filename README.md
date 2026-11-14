# led_signal_basis

Drives a WS2812B LED strip with an ESP32 (tested with an ESP32-C3 SuperMini and an ESP32 D1 mini), built with ESP-IDF v6.1.

Part of the LED signal project:

- **led_signal_basis** (this repository) – the firmware
- [led_signal_crystals](https://codeberg.org/Theannor/led_signal_crystals) – 3D-printed crystal case for a 4 × 4 LED matrix
- [led_signal_desktop_ble_controller](https://codeberg.org/Theannor/led_signal_desktop_ble_controller) – desktop app that sends your PC's status
  (muted, VPN, screen unlocked) to the lamp over Bluetooth or the REST API

At boot the ESP tries to join your Wi-Fi as a client. If that works, the strip
flashes green and then **twinkles** (see below). If not, the ESP opens its
own access point and shows the password on the strip. In both modes a small
web site lets you set the Wi-Fi to join and the twinkle colors. The colors can
also be set over **Bluetooth LE** (see below). Several strips can be linked
with **ESP-NOW**: one gateway forwards its colors to receivers (see below).
Brightness is capped at 25/255.

## Network modes

At every boot (devices set up as ESP-NOW receiver work differently, see
[ESP-NOW](#esp-now)):

1. No client Wi-Fi saved → **access point mode**.
2. Client Wi-Fi saved → try to connect for up to 15 s.
   - Connected → **client mode**: strip is green for 2 s, then twinkling.
   - Not connected → **access point mode**.

In client mode the ESP reconnects by itself after a drop-out. If it stays
offline for 2 minutes, it restarts and decides again (so it falls back to the
access point if your Wi-Fi is gone).

## Twinkling

Every LED runs on its own: it picks a random color, fades in from black to
that color, fades back out to black, then picks a new color.
Each fade takes a random time between 1 and 4 s (fade-in and fade-out of one
cycle take the same time). Because every LED has its own timing, the strip
quickly gets out of sync and looks like random twinkling.

The colors come from the **color palette** (up to 16 colors), set on the
"Farben" page, through the REST API or over Bluetooth (see below). With an empty palette
every LED picks any fully saturated hue. The palette is stored in flash (NVS)
and survives restarts. After a change each LED switches to the new palette
at the start of its next cycle; no restart needed. Palette colors are scaled
to the brightness cap, so dark colors are hardly visible.

**Switching the LEDs off:** a palette containing only black (`#000000`, e.g.
`{"colors":["#000000"]}`) turns all LEDs off at once. The strip is written
once; after that nothing is computed or sent, and the display only checks its
state every 500 ms (a new palette wakes it immediately). Setting colors again
starts the twinkling right away. Status signals (password, green flash, blue
search pulse) still show while off.

## Web site

| Mode | Address |
|------|---------|
| Client | `http://<hostname>.local` (default `http://led-signal-basis.local`) or the IP from your router |
| Access point | `http://192.168.4.1` or `http://<hostname>.local` |

A menu at the top switches between two pages:

- **Farben** (`/colors`): the color palette as round color swatches. Click a
  swatch to pick its color, "×" removes it, the dashed "+" adds a random
  color, then "Speichern". The page itself uses the REST API below.
- **Einstellungen** (`/`): status and Wi-Fi settings, see below.

Both pages share one stylesheet, `/style.css`, stored in flash and cached by
the browser for an hour. It follows the light/dark setting of the device and
needs no internet (no external fonts or libraries).

The Einstellungen page shows the mode, IP address and hostname, and has a form for:

- **Wi-Fi (SSID)**: the network to join. Empty = always access point.
- **Wi-Fi password**: empty = keep the saved one. Tick "offenes WLAN" for a
  network without password. The saved password is never shown.
- **Hostname**: 1–32 characters, letters, digits and `-` (used for DHCP and mDNS).

A second card, **Bluetooth**, sets the Bluetooth device name (1–20 characters,
printable ASCII only, so no umlauts; default `LED Signal`). It has its own
"Speichern und neu starten" button, and the new name is used after the restart.

A third card, **ESP-NOW**, sets the role of the device (see [ESP-NOW](#esp-now))
and shows its own ESP-NOW MAC address, which is also listed in the status card.

A fourth card, **LED-Strip**, sets the data pin and the number of LEDs. It is
shown in every mode, also on ESP-NOW receivers. The pin is picked from a list
of pins that are safe to use: ESP32-C3 GPIO 0, 1, 3–7, 10 (no strapping, flash,
USB or UART pins) and classic ESP32 GPIO 4, 13, 14, 16–19, 21–23, 25–27, 32, 33.
The number of LEDs is 16–25: the password needs one LED per letter, and more
LEDs need more current than USB can supply. Both values take effect after the
restart. The password is shown on the first 16 LEDs; the rest stay dark.

Spaces at the start or end of SSID and hostname are removed when saving
(phone keyboards often add one). The password is saved exactly as typed.

If the ESP cannot join the Wi-Fi, the serial log shows the reason, e.g.
`wifi_sta: Getrennt, Grund 201: WLAN nicht gefunden`. The ESP32-C3 only
supports 2.4 GHz networks.

"Speichern und neu starten" saves the settings in flash (NVS) and restarts the
ESP. The web site has no login yet: anyone on the same network can change it.

### REST API: color palette

| Method | Path | Body / response |
|--------|------|-----------------|
| `GET` | `/api/colors` | `{"colors":["#ff0000","#00ff00"]}` |
| `PUT` | `/api/colors` | Same format; saves the palette and returns it |

Colors are `#rrggbb` (the `#` is optional, upper or lower case). At most 16
colors; `{"colors":[]}` means random hues, `{"colors":["#000000"]}` turns
the LEDs off. A bare array `["#ff0000"]` is also
accepted. Errors return status 400 (or 500 if saving fails) with
`{"error":"..."}`. Examples:

```sh
curl http://led-signal-basis.local/api/colors
curl -X PUT -H 'Content-Type: application/json' \
     -d '{"colors":["#ff0000","#ff8000","#ffffff"]}' \
     http://led-signal-basis.local/api/colors
```

## Bluetooth

The ESP advertises as a Bluetooth LE device (default name `LED Signal`, change
it on the Einstellungen page). Bluetooth runs alongside Wi-Fi in both modes.
There is no pairing: like the web site, anyone in range can change the colors.

| | UUID |
|---|---|
| Service "LED Signal Palette" | `a4ace06f-4b89-4738-86cf-f71f6f6d39e7` |
| Characteristic "Palette" (read, write with response) | `e5e16445-2813-4387-904d-6624f8aa0f03` |

The value of the characteristic is the palette in **binary**: 3 bytes per color
(red, green, blue), at most 16 colors = 48 bytes. 0 bytes means random hues.
Example: red and green = `FF 00 00 00 FF 00` (6 bytes).

- **Write**: sets the palette, saves it in flash and applies it at once (same
  as `PUT /api/colors`; the web site shows it too). A length that is not a
  multiple of 3 or more than 48 bytes is rejected with the ATT error
  "Invalid Attribute Value Length", and nothing changes. Values longer than
  the MTU (20 bytes by default) are sent as a long write; BLE libraries do this
  by themselves.
- **Read**: returns the current palette in the same format, including changes
  made on the web site.

Example script with Python and [bleak](https://github.com/hbldh/bleak)
(`pip install bleak`):

```sh
python tools/ble_set_colors.py "LED Signal" ff0000 00ff00   # set red and green
python tools/ble_set_colors.py "LED Signal"                 # read
python tools/ble_set_colors.py "LED Signal" --clear         # random hues
```

## ESP-NOW

ESP-NOW lets ESPs send short messages to each other directly, without a router.
Every device has one of two roles (card "ESP-NOW" on the Einstellungen page,
saved in flash, applied after the restart):

- **Gateway** (default): works as described above (Wi-Fi, web site, Bluetooth).
  Enter the **receivers** as a comma-separated list of MAC addresses
  (`aa:bb:cc:dd:ee:ff, 11:22:33:44:55:66`, `-` also works, at most 10).
  Every new palette (web site, REST API or Bluetooth) is sent to all of them.
  An empty list means no forwarding.
- **Receiver**: takes colors **only** over ESP-NOW from the one **gateway** MAC
  entered. No Wi-Fi client, no Bluetooth, no "Farben" page (it and
  `PUT /api/colors` answer with 403).

Use the MAC shown as "ESP-NOW-MAC" on the other device's page (it is also in the
serial log at boot: `main: ESP-NOW: MAC …`). It is the MAC of the Wi-Fi station
interface; the access point uses a different one, so don't take it from a
Wi-Fi scan.

**Receiver boot:** the receiver opens its access point for **60 seconds** with
the normal Einstellungen page (password on the strip as usual). While a device
is connected, the access point stays open; 60 s after the last device has
left, the receiver switches the access point and web site off and starts
receiving. To change its settings later, switch it off and on again.

**Finding the channel:** ESP-NOW only works when both devices are on the same
Wi-Fi channel, and the gateway's channel is set by your router (or channel 1 in
access point mode). The receiver therefore tries channels 1–13 (country code
`DE`) in turn, sending a short "hello" to the gateway; the radio
acknowledgement tells it the right channel. While searching, the strip
**pulses blue**. Once found, it sends a hello every 30 s; if 3 in a row are not
acknowledged (router changed channel, gateway off) it searches again.

**Keeping in sync:** each hello carries a checksum of the receiver's palette.
If it differs from the gateway's, the gateway answers with its palette, so a
receiver that was off or missed a message catches up within 30 s. Received
palettes are saved in flash on the receiver, too.

Notes:

- No encryption: a receiver only accepts messages from its gateway MAC, and a
  gateway only answers hellos from MACs in its list.
- With Bluetooth on, ESP-IDF requires Wi-Fi power saving on the gateway, so it
  can miss single hellos; the receiver repeats each hello 5 times per channel.
- All ESP-NOW timings are `#define`s at the top of `main/main.c`.

## Access point

- SSID: `led_signal_basis-XXXX` (WPA2), where `XXXX` are the last 4 hex digits
  of the ESP-NOW MAC, so several devices can be told apart
- Password: 16 letters, each `r`, `g` or `b`. It is created randomly on the
  first boot and stored in flash (NVS), so it stays the same after restarts.

**Reading the password from the strip** (while no device is connected):

- Every LED stands for one letter: red = `r`, green = `g`, blue = `b`.
- To show the order, one LED at a time turns white for 1 second, starting at
  the first LED of the strip. The white LED walks to the end, then starts over.
- So the first LED that turns white is the first letter.

Example: red, red, blue, green, … → password `rrbg…`

As soon as a device has joined, the strip twinkles. When the last device
leaves, the password is shown again.

**Reset everything** (new AP password, no client Wi-Fi, empty palette, default Bluetooth name, ESP-NOW gateway without receivers, default LED pin and count):
`idf.py -p /dev/ttyACM0 erase-flash`, then flash again.

## Hardware

- ESP32-C3 (e.g. SuperMini) with built-in USB (`/dev/ttyACM0`), or a classic ESP32
  (e.g. D1 mini, USB-serial chip, usually `/dev/ttyUSB0`); 4 MB flash, 1.5 MB app partition
  (`partitions_singleapp_large.csv`; NVS keeps its place, so settings survive the switch)
- WS2812B strip with 16–25 LEDs, GRB color order. By default 16 LEDs with data on
  GPIO10 (classic ESP32: GPIO21); pin and count can be changed on the web site
- A classic ESP32 works too (separate build, see "Build and flash")
- The strip is powered over USB, so brightness is capped at 25/255

All fixed settings (default pin and LED count, selectable pins, brightness, timeouts, AP channel, animation
speed, default hostname, default Bluetooth name) are `#define`s at the top of `main/main.c`.

## Project layout

| Path | Content |
|------|---------|
| `main/` | Application: settings, boot flow (client or AP), `display` FreeRTOS task |
| `components/led_signal/` | Hardware-independent logic: colors, rainbow and pulsing (`led_color.h`), twinkling (`led_twinkle.h`), color palette, its JSON and binary formats and checksum (`led_palette.h`), AP password (`led_password.h`), LED pin and count validation (`led_config.h`), Wi-Fi settings, AP SSID and Bluetooth name validation (`net_config.h`), form parsing and HTML pages (`web_form.h`), ESP-NOW settings and MAC lists (`espnow_config.h`), ESP-NOW messages (`espnow_proto.h`), channel search (`espnow_scan.h`) |
| `components/led_output/` | Thin wrapper around the `espressif/led_strip` driver (SPI backend with DMA, so radio load does not cause flicker): `led_output_init()`, `led_output_show()` |
| `components/password_store/` | Loads the AP password from NVS, or creates and saves a new one |
| `components/config_store/` | Loads and saves the client Wi-Fi settings in NVS |
| `components/palette_store/` | Loads and saves the color palette in NVS |
| `components/ble_name_store/` | Loads and saves the Bluetooth device name in NVS |
| `components/led_store/` | Loads and saves the LED pin and count in NVS |
| `components/espnow_store/` | Loads and saves the ESP-NOW role and MAC addresses in NVS |
| `components/espnow_link/` | ESP-NOW radio: gateway (forward, answer hellos) and receiver (channel search, hellos, receive) tasks |
| `components/ble_api/` | Bluetooth palette read/write logic without the BLE stack (host-tested) |
| `components/ble_gatt/` | NimBLE glue: GATT service, advertising; forwards to `ble_api` (board only) |
| `components/wifi_ap/` | Starts the Wi-Fi access point, counts connected devices, remembers when the last one left |
| `components/wifi_sta/` | Joins a Wi-Fi as client with timeout, reconnects after drop-outs |
| `components/net_name/` | mDNS: `<hostname>.local` and the HTTP service; stop again |
| `components/web_server/` | HTTP server: `GET /` settings page, `POST /config` save + restart, `GET /colors` palette page, `GET /style.css` stylesheet, `GET`/`PUT /api/colors` REST API, `POST /ble` Bluetooth name + restart, `POST /espnow` ESP-NOW role + restart, `POST /led` LED pin and count + restart |
| `tools/` | `ble_set_colors.py`: example Bluetooth client (Python, bleak) |
| `test/host/` | Unity + CMock tests that run on the Linux host, no board needed |

## Build and flash

```sh
. ~/esp/esp-idf/export.sh          # fish: . ~/esp/esp-idf/export.fish
idf.py build
idf.py -p /dev/ttyACM0 flash
```

**Classic ESP32** (needs the Xtensa toolchain: `~/esp/esp-idf/install.sh esp32`).
It has its own build folder and sdkconfig, so the ESP32-C3 build stays as it
is. Extra settings are in `sdkconfig.defaults.esp32`. Don't use
`idf.py set-target`: it deletes the C3's `sdkconfig`.

```sh
idf.py -B build_esp32 -D SDKCONFIG=sdkconfig.esp32 -D IDF_TARGET=esp32 build
idf.py -B build_esp32 -D SDKCONFIG=sdkconfig.esp32 -p /dev/ttyACM0 flash
```

## Run the tests

Requirements: Ruby (used by CMock to generate the mocks).

The tests use the ESP-IDF `linux` target and run on the PC. Some mocks read
their headers from the main project's `managed_components/`, so the main
project must have been configured once (`idf.py build` or
`idf.py reconfigure` in the project root).

```sh
cd test/host
idf.py set-target linux            # only once
idf.py build && ./build/led_signal_host_test.elf
```

The exit code is 0 when all tests pass.

Mocks in `test/host/components/`: `led_strip`, `nvs_flash`, `mdns`,
`esp_http_server` and `esp_wifi` (only `esp_wifi.h`, `esp_wifi_default.h` and `esp_now.h`;
ESP-IDF's own Wi-Fi mock needs lwip, which does not build on the Linux host).
`esp_event`, `esp_netif` and `http_parser` use ESP-IDF's mocks from
`$IDF_PATH/tools/mocks`. The `led_strip` and `mdns` mocks read their headers
from the main project's `managed_components/`.

## Known build messages

`idf.py build` in the root prints 5 CMake warnings from `component_validation.cmake`
about private include folders shared between ESP-IDF's own `esp_wifi` and
`wpa_supplicant` components. They come from ESP-IDF v6.1, not from this project.

## License

Copyright (C) 2026 Theannor

This project is licensed under the GNU General Public License v3.0 or later
(`GPL-3.0-or-later`), see [LICENSE](LICENSE).
Dependencies fetched by the ESP-IDF component manager (`managed_components/`) and
ESP-IDF itself keep their own licenses.
