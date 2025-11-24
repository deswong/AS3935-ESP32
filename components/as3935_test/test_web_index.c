/* Basic unity tests for embedded web index header */
#include "unity.h"
#include <string.h>
#include "web_index.h"

void test_index_html_exists(void)
{
    TEST_ASSERT_TRUE_MESSAGE(index_html_len > 0, "index_html_len must be > 0");
}

void test_index_contains_icons(void)
{
    /* look for the inline svg path fragment used for the load/save icons */
    const char *needle = "<svg width=16 height=16";
    TEST_ASSERT_NOT_NULL(strstr(index_html, needle));
}

void test_index_contains_mqtt_password(void)
{
    const char *needle = "mqtt_password";
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(index_html, needle), "index_html must contain 'mqtt_password' marker");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_index_html_exists);
    RUN_TEST(test_index_contains_icons);
    RUN_TEST(test_index_contains_mqtt_password);
    return UNITY_END();
}
