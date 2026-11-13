# led_signal_basis (ESP32-C3 / ESP32)

Firmware for a WS2812B LED signal lamp, built with ESP-IDF.

## Hardware
- Board: ESP32 with built-in USB (port `/dev/ttyACM0`), 4 MB flash;
  here an ESP32-C3
- WS2812B strip, 16 LEDs, data on a configurable pin (e.g. **GPIO10**),
  color format GRB
- The strip is powered over USB → keep brightness at max. ~25/255,
  otherwise the current is not sufficient

## Environment & Commands
- ESP-IDF v6.1 in `~/esp/esp-idf`, target `esp32c3`
- Load the environment before every `idf.py` command. Claude uses bash/zsh:
  `. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py build`
  (in my fish terminal: `. ~/esp/esp-idf/export.fish`)
- Build: `idf.py build`
- Flash: `idf.py -p /dev/ttyACM0 flash`
- **Do not start `idf.py monitor`**: it is interactive and blocks.
  To read output, at most use `timeout 15 idf.py -p /dev/ttyACM0 monitor`.
- Exit the monitor (for me): `Ctrl+T`, then `X`
- Host tests (Linux target, no board needed), in `test/host`:
  `idf.py set-target linux` once, then
  `idf.py build && ./build/led_signal_host_test.elf` (exit code 0 = all pass)
- CMock needs `ruby` (installed). The `led_strip` and `mdns` mocks in `test/host/components/`
  take their headers from `managed_components/` → run `idf.py reconfigure` in the root first
- Own mocks live in `test/host/components/` (led_strip, nvs_flash, esp_wifi, mdns,
  esp_http_server). Do not use
  `$IDF_PATH/tools/mocks/esp_wifi` or `lwip`: lwip's mock does not build on this host
- Known: the root build shows 5 CMake warnings from ESP-IDF's esp_wifi/wpa_supplicant; not ours

## Rules for Claude
- Ask for the target before the build
- Run `idf.py build` after every code change and fix any warnings.
- **Do not flash without being asked**: the board is connected to real hardware.
- Do not edit `sdkconfig` by hand. Persistent settings belong in
  `sdkconfig.defaults`; `sdkconfig` is generated from it.
- Add external libraries via `main/idf_component.yml`
  (`idf.py add-dependency ...`). Never edit `managed_components/` or
  `build/`, and do not commit them to Git.
- Pin numbers and configuration values as `#define` at the top of the file,
  not scattered around.
- No `git push`; at most `git commit`, and only if all existing tests pass.
- Do not change tests without asking.
- Always create new functions together with tests.
- An English description for humans is kept in README.md and kept up to date.

## Code Style
- C, ESP-IDF style: 4 spaces, `snake_case`, comments in German
- Logging with `ESP_LOGI/W/E(TAG, ...)` instead of `printf`; one
  `static const char *TAG` per file
- Check return values of ESP-IDF functions: `ESP_ERROR_CHECK()` at startup,
  during operation log errors instead of crashing
- Wait only with `vTaskDelay(pdMS_TO_TICKS(ms))`, never busy loops
- Move longer-running work into separate FreeRTOS tasks, keep `app_main` lean
- Use Unity and CMock for the tests
