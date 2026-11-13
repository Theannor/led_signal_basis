# CMock-Mock für die NVS-API (nvs.h) in den Host-Tests.
# Ersetzt im Testprojekt die Komponente nvs_flash von ESP-IDF.
set(nvs_include "$ENV{IDF_PATH}/components/nvs_flash/include")

idf_component_mock(INCLUDE_DIRS "${nvs_include}"
                   MOCK_HEADER_FILES "${nvs_include}/nvs.h")
