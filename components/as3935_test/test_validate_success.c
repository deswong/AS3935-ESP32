#include "unity.h"
#include "as3935.h"
#include "nvs.h"
#include "nvs_flash.h"

void setUp(void) { nvs_flash_init(); }
void tearDown(void) {}

void test_validate_passes_when_spurious_low(void)
{
    const char *dummy = "{\"0x00\":10}";
    nvs_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open("as3935", NVS_READWRITE, &h));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_set_str(h, "regs_backup", dummy));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(h));
    nvs_close(h);

    // baseline 5 spurious over 5s, duration 1s => scaled baseline ~1
    as3935_test_set_counters(0,0);
    bool passed = as3935_validate_and_maybe_restore(5,0,1);
    TEST_ASSERT_TRUE(passed);
}

int app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validate_passes_when_spurious_low);
    UNITY_END();
    return 0;
}
