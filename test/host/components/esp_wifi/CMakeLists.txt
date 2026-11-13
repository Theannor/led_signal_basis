# CMock-Mock für die WLAN-API in den Host-Tests. Ersetzt im Testprojekt die
# Komponente esp_wifi. Anders als der Mock in $IDF_PATH/tools/mocks/esp_wifi
# werden nur esp_wifi.h, esp_wifi_default.h und esp_now.h gemockt: esp_mesh.h braucht
# lwip, und dessen Mock baut auf dem Linux-Host nicht. stub_include liefert
# einen leeren lwip/ip_addr.h, damit esp_mesh.h trotzdem übersetzt.
set(original_esp_wifi_dir "$ENV{IDF_PATH}/components/esp_wifi")

idf_component_mock(INCLUDE_DIRS "${original_esp_wifi_dir}/include"
                                "${original_esp_wifi_dir}/include/local"
                                "stub_include"
                   REQUIRES esp_event esp_netif
                   MOCK_HEADER_FILES
                   "${original_esp_wifi_dir}/include/esp_wifi.h"
                   "${original_esp_wifi_dir}/include/esp_wifi_default.h"
                   "${original_esp_wifi_dir}/include/esp_now.h")

target_sources(${COMPONENT_LIB} PRIVATE "global_symbols.c")

# Die generierten Mocks deklarieren Array-Parameter als Zeiger (wie im ESP-IDF-Mock)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-array-parameter)
