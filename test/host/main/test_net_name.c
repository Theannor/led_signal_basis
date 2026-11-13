#include "unity.h"
#include "net_name.h"
#include "Mockmdns.h"

static void test_start_registers_name_and_service(void)
{
    mdns_init_ExpectAndReturn(ESP_OK);
    mdns_hostname_set_ExpectAndReturn("lampe", ESP_OK);
    mdns_instance_name_set_ExpectAndReturn("lampe", ESP_OK);
    mdns_service_add_ExpectAndReturn(NULL, "_http", "_tcp", 80, NULL, 0, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, net_name_start("lampe", 80));
}

static void test_start_reports_init_error(void)
{
    mdns_init_ExpectAndReturn(ESP_ERR_NO_MEM);
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, net_name_start("lampe", 80));
}

static void test_start_frees_on_setup_error(void)
{
    mdns_init_ExpectAndReturn(ESP_OK);
    mdns_hostname_set_ExpectAndReturn("lampe", ESP_ERR_INVALID_ARG);
    mdns_free_Expect();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, net_name_start("lampe", 80));
}

static void test_start_rejects_empty_name(void)
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, net_name_start("", 80));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, net_name_start(NULL, 80));
}

// Wird von test_main.c aufgerufen
void run_net_name_tests(void)
{
    RUN_TEST(test_start_registers_name_and_service);
    RUN_TEST(test_start_reports_init_error);
    RUN_TEST(test_start_frees_on_setup_error);
    RUN_TEST(test_start_rejects_empty_name);
}
