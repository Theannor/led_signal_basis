# CMock-Mock für mDNS (mdns.h) in den Host-Tests.
# Der Header kommt aus dem Hauptprojekt (gleiche Version wie im echten Build).
set(mdns_include "${CMAKE_CURRENT_LIST_DIR}/../../../../managed_components/espressif__mdns/include")
get_filename_component(mdns_include "${mdns_include}" ABSOLUTE)

if(NOT EXISTS "${mdns_include}/mdns.h")
    message(FATAL_ERROR "mdns-Header fehlen: ${mdns_include}\n"
                        "Zuerst im Projekt-Hauptordner 'idf.py reconfigure' ausführen.")
endif()

idf_component_mock(INCLUDE_DIRS "${mdns_include}" "stub_include"
                   REQUIRES esp_netif
                   MOCK_HEADER_FILES "${mdns_include}/mdns.h")
