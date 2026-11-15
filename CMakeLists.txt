# Projekt-CMakeLists für die LED-Signallampe (ESP32-C3 und ESP32)
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Versionsnummer (YYYY.N nach PEP 440) aus dem Git-Tag
include(${CMAKE_CURRENT_LIST_DIR}/tools/project_version.cmake)
# Nur main und dessen Abhängigkeiten bauen
idf_build_set_property(MINIMAL_BUILD ON)
project(led_signal_basis)
