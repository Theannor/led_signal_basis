# CMock-Mock für den HTTP-Server (esp_http_server.h) in den Host-Tests.
# Ersetzt im Testprojekt die Komponente esp_http_server; die Kconfig ist von
# ESP-IDF kopiert, damit die CONFIG_HTTPD_*-Werte für den Header existieren.
set(httpd_include "$ENV{IDF_PATH}/components/esp_http_server/include")

idf_component_mock(INCLUDE_DIRS "${httpd_include}"
                   REQUIRES http_parser esp_event
                   MOCK_HEADER_FILES "${httpd_include}/esp_http_server.h")
