#include "unity.h"
#include "as3935.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>

static uint8_t last_reg = 0xff;
static uint8_t last_val = 0xff;

static esp_err_t fake_spi_write(uint8_t reg, uint8_t val)
{
    last_reg = reg;
    last_val = val;
    return ESP_OK;
}

void setUp(void)
{
    // initialize nvs in RAM for tests
    nvs_flash_init();
    as3935_set_spi_write_fn(fake_spi_write);
}

void tearDown(void)
{
}

void test_apply_config_json_writes_registers(void)
{
    const char *json = "{\"0x03\": 42, \"0x04\": 7}";
    bool ok = as3935_apply_config_json(json);
    TEST_ASSERT_TRUE(ok);
    // last write should be to 0x04 with value 7
    TEST_ASSERT_EQUAL_UINT8(0x04, last_reg);
    TEST_ASSERT_EQUAL_UINT8(7, last_val);
}

void test_nvs_save_and_load_config(void)
{
    const char *cfg = "{\"0x00\":1}";
    esp_err_t err = as3935_save_config_nvs(cfg);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    char buf[128];
    err = as3935_load_config_nvs(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(cfg, buf);
}

// Move app_main() logic into a separate test function
void as3935_run_tests(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_apply_config_json_writes_registers);
    RUN_TEST(test_nvs_save_and_load_config);
    UNITY_END();
}
