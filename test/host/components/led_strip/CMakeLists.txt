# CMock-Mock des led_strip-Treibers für die Host-Tests.
# Die Header kommen aus dem Hauptprojekt, damit Mock und echter Build
# dieselbe Version nutzen.
set(led_strip_include "${CMAKE_CURRENT_LIST_DIR}/../../../../managed_components/espressif__led_strip/include")
get_filename_component(led_strip_include "${led_strip_include}" ABSOLUTE)

if(NOT EXISTS "${led_strip_include}/led_strip.h")
    message(FATAL_ERROR "led_strip-Header fehlen: ${led_strip_include}\n"
                        "Zuerst im Projekt-Hauptordner 'idf.py reconfigure' ausführen.")
endif()

# stub_include ersetzt die Treiber-Header (rmt_types.h, spi_master.h,
# gpio_types.h), die es für das Ziel linux nicht gibt.
idf_component_mock(INCLUDE_DIRS "${led_strip_include}" "stub_include"
                   MOCK_HEADER_FILES
                   "${led_strip_include}/led_strip.h"
                   "${led_strip_include}/led_strip_rmt.h")
