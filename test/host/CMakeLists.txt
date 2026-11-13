# Host-Testprojekt: läuft auf dem Linux-Rechner (Ziel "linux"), kein Board nötig
cmake_minimum_required(VERSION 3.22)

set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/../../components"
    # Eigene CMock-Mocks (led_strip, nvs_flash, esp_wifi, mdns, esp_http_server)
    "${CMAKE_CURRENT_LIST_DIR}/components"
    # CMock-Mocks von ESP-IDF für Events und Netzwerk-Interface
    "$ENV{IDF_PATH}/tools/mocks/esp_event"
    "$ENV{IDF_PATH}/tools/mocks/esp_netif"
    # Mock für http_parser (wird von esp_http_server.h eingebunden)
    "$ENV{IDF_PATH}/tools/mocks/http_parser")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(COMPONENTS main)
project(led_signal_host_test)
