# Projekt-CMakeLists für den LED-Farbwechsel (ESP32-C3)
cmake_minimum_required(VERSION 3.22)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# Nur main und dessen Abhängigkeiten bauen
idf_build_set_property(MINIMAL_BUILD ON)
project(led_signal_basis)
