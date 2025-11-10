#include "unity.h"
#include "as3935.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

void setUp(void)
{
    nvs_flash_init();
}

void tearDown(void)
{
}

void test_validate_and_maybe_restore_detects_worse_spurious(void)
{
    // Save a dummy backup into NVS so restore has something to apply
    const char *dummy = "{\"0x00\":10}";
    nvs_handle_t h;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open("as3935", NVS_READWRITE, &h));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_set_str(h, "regs_backup", dummy));
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(h));
    nvs_close(h);

    // baseline: 1 spurious in 5s; duration 1s -> scaled baseline will be small
    int baseline_sp = 1;
    int baseline_li = 0;
    int duration_s = 1;

    // Simulate the device generating a high spurious count during validation
    as3935_test_set_counters(10, 0);

    bool passed = as3935_validate_and_maybe_restore(baseline_sp, baseline_li, duration_s);
    // Expect validation to fail and return false (indicating rollback)
    TEST_ASSERT_FALSE(passed);
}

int app_main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validate_and_maybe_restore_detects_worse_spurious);
    UNITY_END();
    return 0;
}
