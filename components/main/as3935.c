#include "as3935.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "driver/spi_master.h"
#include "esp_log.h"
#include "app_mqtt.h"
#include "settings.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "events.h"
#include "http_helpers.h"
#include "esp_http_server.h"

static QueueHandle_t gpio_evt_queue = NULL;
static const char *TAG = "as3935";
static spi_device_handle_t spi_dev = NULL;

// forward declarations used before definition
static esp_err_t as3935_spi_read_reg(uint8_t reg, uint8_t *val);
static esp_err_t as3935_spi_write_reg(uint8_t reg, uint8_t val);
static void as3935_publish_event(const char *json_payload);

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static as3935_event_cb_t event_cb = NULL;
// Calibration counters (IRQ-driven)
static volatile int calib_spur_counter = 0;
static volatile int calib_lightning_counter = 0;
// -------------------- Auto-calibration (status struct forward) --------------------
typedef struct {
    bool running;
    int initial_noise_level;
    int initial_spike_rejection;
    int final_noise_level;
    int final_spike_rejection;
    int windows_sampled;
    int spur_count;
    int lightning_count;
    char message[128];
} calibrate_status_t;

static calibrate_status_t calib_status = {0};
static TaskHandle_t calib_task = NULL;
// Validation task handle
static TaskHandle_t validate_task = NULL;

// Save a snapshot JSON to NVS under key "regs_backup"
static esp_err_t as3935_save_backup_nvs(const char *json)
{
    if (!json) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "regs_backup", json);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

// Load backup JSON
static esp_err_t as3935_load_backup_nvs(char *out, size_t len)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t required = len;
    err = nvs_get_str(h, "regs_backup", out, &required);
    nvs_close(h);
    return err;
}

// Snapshot current registers into provided buffer as JSON
static void as3935_snapshot_regs_json(char *out, size_t out_len)
{
    uint8_t r0=0,r1=0,r2=0,r3=0,r8=0;
    as3935_spi_read_reg(0x00, &r0);
    as3935_spi_read_reg(0x01, &r1);
    as3935_spi_read_reg(0x02, &r2);
    as3935_spi_read_reg(0x03, &r3);
    as3935_spi_read_reg(0x08, &r8);
    snprintf(out, out_len, "{\"0x00\":%d,\"0x01\":%d,\"0x02\":%d,\"0x03\":%d,\"0x08\":%d}", r0, r1, r2, r3, r8);
}

// Restore backup (blocking, applies config JSON if backup exists)
static esp_err_t as3935_restore_backup(void)
{
    char buf[1024];
    if (as3935_load_backup_nvs(buf, sizeof(buf)) != ESP_OK) return ESP_FAIL;
    // apply the JSON mapping
    if (!as3935_apply_config_json(buf)) return ESP_FAIL;
    // also save as current regs snapshot
    as3935_save_config_nvs(buf);
    return ESP_OK;
}

// Validation task: monitor counters for a short duration and rollback if spurious rate worsens
static void validate_task_fn(void *arg)
{
    typedef struct { int baseline_sp; int baseline_li; int duration_s; } validate_params_t;
    validate_params_t *p = (validate_params_t*)arg;
    if (!p) { vTaskDelete(NULL); return; }
    // clear counters then sample for duration
    __sync_lock_test_and_set(&calib_spur_counter, 0);
    __sync_lock_test_and_set(&calib_lightning_counter, 0);
    vTaskDelay(pdMS_TO_TICKS(p->duration_s * 1000));
    int sp_new = __sync_fetch_and_add(&calib_spur_counter, 0);
    int li_new = __sync_fetch_and_add(&calib_lightning_counter, 0);
    ESP_LOGI(TAG, "validate_task: baseline_sp=%d baseline_li=%d duration=%ds -> sp_new=%d li_new=%d", p->baseline_sp, p->baseline_li, p->duration_s, sp_new, li_new);
    // Scale baseline to duration: baseline was measured over 5s earlier
    float scale = (float)p->duration_s / 5.0f;
    int baseline_sp_scaled = (int)(p->baseline_sp * scale + 0.5f);
    // If spurious increased by >2x compared to baseline scaled, rollback
    if (sp_new > baseline_sp_scaled * 2) {
        // restore backup
        if (as3935_restore_backup() == ESP_OK) {
            snprintf(calib_status.message, sizeof(calib_status.message), "Validation failed - rolled back (sp_new=%d)", sp_new);
            ESP_LOGW(TAG, "Validation failed - rolled back (sp_new=%d baseline_scaled=%d)", sp_new, baseline_sp_scaled);
        } else {
            snprintf(calib_status.message, sizeof(calib_status.message), "Validation failed - rollback failed");
            ESP_LOGW(TAG, "Validation failed - attempted rollback but restore failed");
        }
    } else {
        snprintf(calib_status.message, sizeof(calib_status.message), "Validation passed (sp_new=%d)", sp_new);
        ESP_LOGI(TAG, "Validation passed (sp_new=%d)", sp_new);
    }
    // cleanup
    free(p);
    validate_task = NULL;
    vTaskDelete(NULL);
}

// Synchronous helper for unit tests: same logic as validate_task_fn but callable
// directly without spawning a task. Returns true if validation passed, false if rollback occurred.
bool as3935_validate_and_maybe_restore(int baseline_sp, int baseline_li, int duration_s)
{
    // clear counters then sample for duration
    __sync_lock_test_and_set(&calib_spur_counter, 0);
    __sync_lock_test_and_set(&calib_lightning_counter, 0);
    // In unit tests, duration_s may be 0 or small; use vTaskDelay to simulate waiting
    if (duration_s > 0) vTaskDelay(pdMS_TO_TICKS(duration_s * 1000));
    int sp_new = __sync_fetch_and_add(&calib_spur_counter, 0);
    // Scale baseline to duration: baseline was measured over 5s earlier
    float scale = (float)duration_s / 5.0f;
    int baseline_sp_scaled = (int)(baseline_sp * scale + 0.5f);
    if (sp_new > baseline_sp_scaled * 2) {
        // restore backup
        if (as3935_restore_backup() == ESP_OK) {
            return false; // rolled back
        }
        return false; // attempted rollback but failed
    }
    return true; // passed
}

void as3935_test_set_counters(int sp, int li)
{
    __sync_lock_test_and_set(&calib_spur_counter, sp);
    __sync_lock_test_and_set(&calib_lightning_counter, li);
}

void as3935_test_get_counters(int *sp, int *li)
{
    if (sp) *sp = __sync_fetch_and_add(&calib_spur_counter, 0);
    if (li) *li = __sync_fetch_and_add(&calib_lightning_counter, 0);
}

static void gpio_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // Read a status register from AS3935 (placeholder register 0x03)
            uint8_t status = 0;
            if (as3935_spi_read_reg(0x03, &status) == ESP_OK) {
                int distance = status & 0x0F; // placeholder parsing
                int energy = (status >> 4) & 0x0F;
                // call callback
                if (event_cb) event_cb(distance, energy, (uint32_t)esp_log_timestamp());
                // build json and publish (use configured topic if set)
                char payload[128];
                snprintf(payload, sizeof(payload), "{\"distance_km\":%d, \"energy\":%d}", distance, energy);
                as3935_publish_event(payload);
                // broadcast via SSE
                events_broadcast("lightning", payload);
                // If calibration is running, classify this IRQ and increment counters
                if (calib_status.running) {
                    if (energy < 3) {
                        __sync_add_and_fetch(&calib_spur_counter, 1);
                    } else {
                        __sync_add_and_fetch(&calib_lightning_counter, 1);
                    }
                }
            }
        }
    }
}

static as3935_config_t current_cfg;

// SPI register access. AS3935 expects an 8-bit register address, MSB=R/W bit
// We'll assume the adapter expects a standard 1-byte command followed by 1 byte data.
// Allow tests to override the SPI write function
typedef esp_err_t (*as3935_spi_write_fn_t)(uint8_t reg, uint8_t val);
static as3935_spi_write_fn_t as3935_spi_write_fn = NULL;

static esp_err_t as3935_spi_write_reg(uint8_t reg, uint8_t val)
{
    if (!spi_dev) return ESP_ERR_INVALID_STATE;
    // write transaction: send reg (with write bit = 0) then data
    uint8_t tx[2] = { reg & 0x7F, val };
    spi_transaction_t t = {0};
    t.length = 8 * 2;
    t.tx_buffer = tx;
    esp_err_t err = spi_device_transmit(spi_dev, &t);
    if (err != ESP_OK) ESP_LOGW(TAG, "SPI write reg 0x%02x failed: %d", reg, err);
    return err;
}

// Setter exposed to tests (also declared in header)
void as3935_set_spi_write_fn(esp_err_t (*fn)(uint8_t reg, uint8_t val))
{
    if (fn) as3935_spi_write_fn = fn;
    else as3935_spi_write_fn = as3935_spi_write_reg;
}

static esp_err_t as3935_spi_read_reg(uint8_t reg, uint8_t *val)
{
    if (!spi_dev || !val) return ESP_ERR_INVALID_STATE;
    // read transaction: send reg with read bit set (assume MSB=1 indicates read), then read one byte
    uint8_t tx[2] = { (uint8_t)(reg | 0x80), 0x00 };
    uint8_t rx[2] = {0};
    spi_transaction_t t = {0};
    t.length = 8 * 2;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t err = spi_device_transmit(spi_dev, &t);
    if (err == ESP_OK) *val = rx[1];
    else ESP_LOGW(TAG, "SPI read reg 0x%02x failed: %d", reg, err);
    return err;
}

bool as3935_init(const as3935_config_t *cfg)
{
    if (!cfg) return false;
    memcpy(&current_cfg, cfg, sizeof(current_cfg));

    // configure SPI bus and device
    spi_bus_config_t buscfg = {
        .miso_io_num = cfg->miso_pin,
        .mosi_io_num = cfg->mosi_pin,
        .sclk_io_num = cfg->sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 16,
    };
    esp_err_t err = spi_bus_initialize(cfg->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %d", err);
        return false;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000, // 1 MHz (adjust as needed)
        .mode = 0,
        .spics_io_num = cfg->cs_pin,
        .queue_size = 3,
    };
    err = spi_bus_add_device(cfg->spi_host, &devcfg, &spi_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_add_device failed: %d", err);
        return false;
    }

    ESP_LOGI(TAG, "AS3935 SPI initialized on host %d SCLK=%d MOSI=%d MISO=%d CS=%d", cfg->spi_host, cfg->sclk_pin, cfg->mosi_pin, cfg->miso_pin, cfg->cs_pin);

    // prepare GPIO queue and task for IRQ handling
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);
    return true;
}

bool as3935_configure_default(void)
{
    // Configure default registers. Use OUTDOOR AFE_GAIN by default (0x0E)
    // SparkFun/ESPhome use OUTDOOR value 0x0E and INDOOR value 0x12 for reg 0x00
    esp_err_t err = as3935_spi_write_reg(0x00, 0x0E);
    if (err != ESP_OK) return false;
    ESP_LOGI(TAG, "AS3935 default configuration written");
    return true;
}

bool as3935_setup_irq(int irq_pin)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << irq_pin);
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(irq_pin, gpio_isr_handler, (void*) irq_pin);
    ESP_LOGI(TAG, "AS3935 IRQ configured on pin %d", irq_pin);
    return true;
}

void as3935_set_event_callback(as3935_event_cb_t cb)
{
    event_cb = cb;
}

// This would be called from ISR when IRQ fires
void as3935_invoke_event(int distance_km, int energy)
{
    if (event_cb) {
        event_cb(distance_km, energy, 0);
    }
}

void as3935_publish_event_json(const char *topic, const char *json_payload)
{
    if (!topic || !json_payload) return;
    esp_err_t err = mqtt_publish(topic, json_payload);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "MQTT publish failed: %d", err);
    }
}

// publish using saved topic if topic==NULL
void as3935_publish_event(const char *json_payload)
{
    char topic[256] = {0};
    if (settings_load_str("mqtt", "topic", topic, sizeof(topic)) == ESP_OK && topic[0]) {
        as3935_publish_event_json(topic, json_payload);
    } else {
        // fallback to default
        as3935_publish_event_json("as3935/lightning", json_payload);
    }
}

// Apply register map provided as JSON object (e.g. {"0x03": 150, "0x04": 5})
bool as3935_apply_config_json(const char *json)
{
    if (!json) return false;
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        const char *name = item->string;
        if (!name) continue;
        int reg = (int)strtol(name, NULL, 0);
        if (cJSON_IsNumber(item)) {
            int val = item->valueint;
            if (as3935_spi_write_fn) as3935_spi_write_fn((uint8_t)reg, (uint8_t)val);
            else as3935_spi_write_reg((uint8_t)reg, (uint8_t)val);
        }
    }
    cJSON_Delete(root);
    return true;
}

esp_err_t as3935_save_config_nvs(const char *json)
{
    if (!json) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, "regs", json);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t as3935_load_config_nvs(char *out, size_t len)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t required = len;
    err = nvs_get_str(h, "regs", out, &required);
    nvs_close(h);
    return err;
}

// HTTP handlers for AS3935 config
esp_err_t as3935_save_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
        if (content_len <= 0 || content_len > 4096) { http_helpers_send_400(req); return ESP_FAIL; }
    char *buf = malloc(content_len + 1);
    if (!buf) { httpd_resp_send_500(req); return ESP_ERR_NO_MEM; }
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
    buf[ret] = '\0';

    // parse JSON body
    cJSON *root = cJSON_Parse(buf);
    if (!root) { free(buf); http_helpers_send_400(req); return ESP_FAIL; }

    // If the body contains a simple sensor_id field, save it to NVS and return.
    const cJSON *sensor_id = cJSON_GetObjectItemCaseSensitive(root, "sensor_id");
    if (cJSON_IsNumber(sensor_id)) {
        nvs_handle_t h;
        esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
        if (err == ESP_OK) {
            err = nvs_set_i32(h, "sensor_id", sensor_id->valueint);
            if (err == ESP_OK) err = nvs_commit(h);
            nvs_close(h);
        }
        cJSON_Delete(root);
        free(buf);
        if (err != ESP_OK) { httpd_resp_send_500(req); return err; }
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":true}\n");
        return ESP_OK;
    }

    // Otherwise treat body as register JSON map and store/apply it (backward compatible)
    cJSON_Delete(root);
    esp_err_t err = as3935_save_config_nvs(buf);
    if (err != ESP_OK) { free(buf); httpd_resp_send_500(req); return err; }
    // apply immediately
    as3935_apply_config_json(buf);
    free(buf);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_status_handler(httpd_req_t *req)
{
    char buf[2048] = {0};
    // Load saved register JSON if present
    bool have_cfg = (as3935_load_config_nvs(buf, sizeof(buf)) == ESP_OK);

    // Read sensor_id from NVS (default to 1)
    int32_t sid = 1;
    nvs_handle_t h;
    if (nvs_open("as3935", NVS_READONLY, &h) == ESP_OK) {
        int32_t tmp = 0;
        if (nvs_get_i32(h, "sensor_id", &tmp) == ESP_OK) sid = tmp;
        nvs_close(h);
    }

    httpd_resp_set_type(req, "application/json");
    if (have_cfg) {
        // augment existing JSON with sensor_id by returning an object that merges them
        // simplistic approach: return { "sensor_id":N, "config": <existing_json> }
        char out[2300];
        snprintf(out, sizeof(out), "{\"sensor_id\":%" PRId32 ",\"config\":%s}\n", sid, buf);
        httpd_resp_sendstr(req, out);
    } else {
        char out[128];
        snprintf(out, sizeof(out), "{\"sensor_id\":%" PRId32 "}\n", sid);
        httpd_resp_sendstr(req, out);
    }
    return ESP_OK;
}

// Pin save/load
esp_err_t as3935_save_pins_nvs(int i2c_port, int sda, int scl, int irq)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    // For backward compatibility, store under the old keys as well, but prefer SPI-specific keys
    err = nvs_set_i32(h, "i2c_port", i2c_port);
    if (err == ESP_OK) err = nvs_set_i32(h, "sda", sda);
    if (err == ESP_OK) err = nvs_set_i32(h, "scl", scl);
    if (err == ESP_OK) err = nvs_set_i32(h, "irq", irq);
    // Also store SPI-specific keys if the values look like SPI mapping
    if (err == ESP_OK) err = nvs_set_i32(h, "spi_host", i2c_port);
    if (err == ESP_OK) err = nvs_set_i32(h, "sclk", sda);
    if (err == ESP_OK) err = nvs_set_i32(h, "mosi", scl);
    // miso and cs may be absent in old mapping; leave as-is
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t as3935_load_pins_nvs(int *i2c_port, int *sda, int *scl, int *irq)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    int32_t tmp;
    // Prefer SPI-specific keys if present (backwards compatible with older i2c keys)
    if (i2c_port) {
        // try spi_host first
        err = nvs_get_i32(h, "spi_host", &tmp);
        if (err == ESP_OK) *i2c_port = (int)tmp;
        else {
            err = nvs_get_i32(h, "i2c_port", &tmp);
            if (err == ESP_OK) *i2c_port = (int)tmp;
        }
    }
    if (err == ESP_OK && sda) {
        // try sclk
        err = nvs_get_i32(h, "sclk", &tmp);
        if (err == ESP_OK) *sda = (int)tmp;
        else {
            err = nvs_get_i32(h, "sda", &tmp);
            if (err == ESP_OK) *sda = (int)tmp;
        }
    }
    if (err == ESP_OK && scl) {
        // try mosi
        err = nvs_get_i32(h, "mosi", &tmp);
        if (err == ESP_OK) *scl = (int)tmp;
        else {
            err = nvs_get_i32(h, "scl", &tmp);
            if (err == ESP_OK) *scl = (int)tmp;
        }
    }
    if (err == ESP_OK && irq) {
        err = nvs_get_i32(h, "irq", &tmp);
        if (err == ESP_OK) *irq = (int)tmp;
    }
    nvs_close(h);
    return err;
}

esp_err_t as3935_pins_save_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
        if (content_len <= 0 || content_len > 512) { http_helpers_send_400(req); return ESP_FAIL; }
    char buf[512];
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
        if (!root) { http_helpers_send_400(req); return ESP_FAIL; }
    // Accept either legacy I2C-style fields or SPI-specific fields
    const cJSON *spi_host = cJSON_GetObjectItemCaseSensitive(root, "spi_host");
    const cJSON *sclk = cJSON_GetObjectItemCaseSensitive(root, "sclk");
    const cJSON *mosi = cJSON_GetObjectItemCaseSensitive(root, "mosi");
    const cJSON *miso = cJSON_GetObjectItemCaseSensitive(root, "miso");
    const cJSON *cs = cJSON_GetObjectItemCaseSensitive(root, "cs");
    const cJSON *irq = cJSON_GetObjectItemCaseSensitive(root, "irq");

    if (cJSON_IsNumber(spi_host) && cJSON_IsNumber(sclk) && cJSON_IsNumber(mosi) && cJSON_IsNumber(irq)) {
        // SPI-style save
        as3935_save_pins_nvs(spi_host->valueint, sclk->valueint, mosi->valueint, irq->valueint);
        cJSON_Delete(root);
        as3935_config_t cfg = { .spi_host = spi_host->valueint, .sclk_pin = sclk->valueint, .mosi_pin = mosi->valueint, .miso_pin = miso ? miso->valueint : -1, .cs_pin = cs ? cs->valueint : -1, .irq_pin = irq->valueint };
        as3935_init(&cfg);
        as3935_setup_irq(irq->valueint);
    } else {
        // fallback: accept legacy i2c mapping keys
        const cJSON *i2c = cJSON_GetObjectItemCaseSensitive(root, "i2c_port");
        const cJSON *sda = cJSON_GetObjectItemCaseSensitive(root, "sda");
        const cJSON *scl = cJSON_GetObjectItemCaseSensitive(root, "scl");
        if (!cJSON_IsNumber(i2c) || !cJSON_IsNumber(sda) || !cJSON_IsNumber(scl) || !cJSON_IsNumber(irq)) { cJSON_Delete(root); http_helpers_send_400(req); return ESP_FAIL; }
        as3935_save_pins_nvs(i2c->valueint, sda->valueint, scl->valueint, irq->valueint);
        cJSON_Delete(root);
        // Map legacy values into SPI struct (best-effort)
        as3935_config_t cfg = { .spi_host = i2c->valueint, .sclk_pin = sda->valueint, .mosi_pin = scl->valueint, .miso_pin = -1, .cs_pin = -1, .irq_pin = irq->valueint };
        as3935_init(&cfg);
        as3935_setup_irq(irq->valueint);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_pins_status_handler(httpd_req_t *req)
{
    int i2c_port=0,sda=0,scl=0,irq=0;
    if (as3935_load_pins_nvs(&i2c_port,&sda,&scl,&irq) == ESP_OK) {
        // Backward-compatible fields may be present in NVS; format as old i2c style for the API
        char buf[128];
        snprintf(buf,sizeof(buf),"{\"i2c_port\":%d,\"sda\":%d,\"scl\":%d,\"irq\":%d}",i2c_port,sda,scl,irq);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, buf);
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{}\n");
    }
    return ESP_OK;
}


bool as3935_init_from_nvs(void)
{
    int i2c_port=0,sda=0,scl=0,irq=0;
    // For backward compatibility we load the old keys; if you have migrated to SPI you should
    // store SPI pins in the same NVS namespace but under different keys (see docs).
    if (as3935_load_pins_nvs(&i2c_port,&sda,&scl,&irq) != ESP_OK) return false;
    // Interpret saved values: treat them as (spi_host, sclk, mosi, miso/cs) depending on prior UI.
    as3935_config_t cfg = { .spi_host = i2c_port, .sclk_pin = sda, .mosi_pin = scl, .miso_pin = 0, .cs_pin = 0, .irq_pin = irq };
    if (!as3935_init(&cfg)) return false;
    as3935_setup_irq(irq);
    return true;
}

// helper: read-modify-write a register using mask and shift
static esp_err_t as3935_write_masked(uint8_t reg, uint8_t mask, uint8_t value, uint8_t shift)
{
    uint8_t cur = 0;
    if (as3935_spi_read_reg(reg, &cur) != ESP_OK) return ESP_FAIL;
    uint8_t cleared = cur & ~mask;
    uint8_t to_write = cleared | ((value << shift) & mask);
    return as3935_spi_write_reg(reg, to_write);
}

// Apply ESPhome-like parameters
esp_err_t as3935_params_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
        if (content_len <= 0 || content_len > 1024) { http_helpers_send_400(req); return ESP_FAIL; }
    char *buf = malloc(content_len + 1);
    if (!buf) { httpd_resp_send_500(req); return ESP_ERR_NO_MEM; }
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) { free(buf); httpd_resp_send_500(req); return ESP_FAIL; }
    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
    free(buf);
        if (!root) { http_helpers_send_400(req); return ESP_FAIL; }

    // Map fields to register masks/positions
    // reg 0x00 AFE_GAIN, mask 0x3E, indoor flag uses value INDOOR(0x12) or OUTDOOR(0x0E) shifted appropriately
    const cJSON *indoor = cJSON_GetObjectItemCaseSensitive(root, "indoor");
    if (cJSON_IsBool(indoor)) {
        // write full AFE_GAIN with ESPhome INDOOR/OUTDOOR values
    uint8_t val = cJSON_IsTrue(indoor) ? 0x12 : 0x0E;
    as3935_spi_write_reg(0x00, val);
    }

    const cJSON *noise_level = cJSON_GetObjectItemCaseSensitive(root, "noise_level");
    if (cJSON_IsNumber(noise_level)) {
        uint8_t nl = (uint8_t)noise_level->valueint;
        if (nl >=1 && nl <=7) {
            // THRESHOLD reg 0x01 bits[6:4] = noise_level
            as3935_write_masked(0x01, 0x70, nl, 4);
        }
    }

    const cJSON *watchdog_threshold = cJSON_GetObjectItemCaseSensitive(root, "watchdog_threshold");
    if (cJSON_IsNumber(watchdog_threshold)) {
        uint8_t wt = (uint8_t)watchdog_threshold->valueint;
        if (wt >=1 && wt <=10) {
            // THRESHOLD reg lower bits [3:0]
            as3935_write_masked(0x01, 0x0F, wt, 0);
        }
    }

    const cJSON *spike_rejection = cJSON_GetObjectItemCaseSensitive(root, "spike_rejection");
    if (cJSON_IsNumber(spike_rejection)) {
        uint8_t sr = (uint8_t)spike_rejection->valueint;
        if (sr >=1 && sr <=11) {
            as3935_write_masked(0x02, 0x0F, sr, 0);
        }
    }

    const cJSON *lightning_threshold = cJSON_GetObjectItemCaseSensitive(root, "lightning_threshold");
    if (cJSON_IsNumber(lightning_threshold)) {
        uint8_t lt = (uint8_t)lightning_threshold->valueint;
        // SparkFun mapping: 1 -> 0, 5 -> 1, 9 -> 2, 16 -> 3 written into bits [5:4]
        uint8_t bits;
        if (lt == 1) bits = 0;
        else if (lt == 5) bits = 1;
        else if (lt == 9) bits = 2;
        else if (lt == 16) bits = 3;
        else bits = 0; // default to single-strike if unknown
        as3935_write_masked(0x02, 0x30, bits, 4);
    }

    const cJSON *mask_disturber = cJSON_GetObjectItemCaseSensitive(root, "mask_disturber");
    if (cJSON_IsBool(mask_disturber)) {
        uint8_t v = cJSON_IsTrue(mask_disturber) ? 1 : 0;
        as3935_write_masked(0x03, 0x20, v, 5); // INT_MASK_ANT bit 5
    }

    const cJSON *div_ratio = cJSON_GetObjectItemCaseSensitive(root, "div_ratio");
    if (cJSON_IsNumber(div_ratio)) {
        uint8_t dr = (uint8_t)div_ratio->valueint;
        // Map common values: 16->0,22->1,64->2,128->3 in bits[7:6]
        uint8_t mapped = 0;
        if (dr==16) mapped = 0;
        else if (dr==22) mapped = 1;
        else if (dr==64) mapped = 2;
        else if (dr==128) mapped = 3;
        as3935_write_masked(0x03, 0xC0, mapped, 6);
    }

    const cJSON *capacitance = cJSON_GetObjectItemCaseSensitive(root, "capacitance");
    if (cJSON_IsNumber(capacitance)) {
        uint8_t cap = (uint8_t)capacitance->valueint;
        // value stored as steps of 8pF, mask 0x0F at reg 0x08? Use mask 0x0F at shift 0
        as3935_write_masked(0x08, 0x0F, cap, 0);
    }

    const cJSON *tune_antenna = cJSON_GetObjectItemCaseSensitive(root, "tune_antenna");
    if (cJSON_IsBool(tune_antenna) && cJSON_IsTrue(tune_antenna)) {
        // trigger antenna tune: write direct command 0x96 to CALIB_RCO maybe
        as3935_spi_write_reg(0x3D, 0x00);
    }

    const cJSON *calibration = cJSON_GetObjectItemCaseSensitive(root, "calibration");
    if (cJSON_IsBool(calibration)) {
        // if false, may disable calibration—skip here
    }

    const cJSON *clear_statistics = cJSON_GetObjectItemCaseSensitive(root, "clear_statistics");
    if (cJSON_IsBool(clear_statistics) && cJSON_IsTrue(clear_statistics)) {
        // The AS3935 requires a 1->0->1 toggle on STAT bit (reg 0x02 bit6) to clear statistics
        as3935_write_masked(0x02, 0x40, 1, 6);
        as3935_write_masked(0x02, 0x40, 0, 6);
        as3935_write_masked(0x02, 0x40, 1, 6);
    }

    const cJSON *reset_settings = cJSON_GetObjectItemCaseSensitive(root, "reset_settings");
    if (cJSON_IsBool(reset_settings) && cJSON_IsTrue(reset_settings)) {
        // Trigger a chip reset to default settings (direct command/register as per SparkFun)
        as3935_spi_write_reg(0x3C, 0x00);
    }

    // After applying, snapshot some registers and save as JSON to NVS
    char out[256];
    uint8_t r0=0,r1=0,r2=0,r3=0,r8=0;
    as3935_spi_read_reg(0x00, &r0);
    as3935_spi_read_reg(0x01, &r1);
    as3935_spi_read_reg(0x02, &r2);
    as3935_spi_read_reg(0x03, &r3);
    as3935_spi_read_reg(0x08, &r8);
    snprintf(out, sizeof(out), "{\"0x00\":%d,\"0x01\":%d,\"0x02\":%d,\"0x03\":%d,\"0x08\":%d}", r0, r1, r2, r3, r8);
    as3935_save_config_nvs(out);

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

// -------------------- Auto-calibration (active sweep) --------------------
// Simple prototype: sweep noise_level 1..7 and spike_rejection 1..11 and pick
// the highest sensitivity (lowest noise_level) that keeps spur rate below
// target_spur_per_min during a short measurement window. This is synchronous
// but runs in a background FreeRTOS task so HTTP handler returns quickly.

// Helper: set noise_level and spike_rejection immediately (writes registers)
static void as3935_apply_levels(uint8_t noise_level, uint8_t spike_rej)
{
    if (noise_level >= 1 && noise_level <= 7) {
        as3935_write_masked(0x01, 0x70, noise_level, 4);
    }
    if (spike_rej >= 1 && spike_rej <= 15) {
        as3935_write_masked(0x02, 0x0F, spike_rej, 0);
    }
}

// Task: perform sweep
static void calib_task_fn(void *arg)
{
    // params via arg (not used now) — use defaults
    const int window_seconds = 10; // short windows in prototype
    const int target_spur_per_min = 6; // allow up to 6 spurious/min

    calib_status.running = true;
    calib_status.spur_count = 0;
    calib_status.lightning_count = 0;
    calib_status.windows_sampled = 0;
    // reset IRQ counters
    __sync_lock_test_and_set(&calib_spur_counter, 0);
    __sync_lock_test_and_set(&calib_lightning_counter, 0);

    // read current saved registers (we assume previous snapshot in NVS or read regs)
    uint8_t r1=0,r2=0;
    as3935_spi_read_reg(0x01, &r1);
    as3935_spi_read_reg(0x02, &r2);
    // extract current noise_level and spike
    int cur_noise = (r1 & 0x70) >> 4;
    int cur_spike = (r2 & 0x0F) >> 0;
    if (cur_noise < 1) cur_noise = 1;
    if (cur_spike < 1) cur_spike = 1;
    calib_status.initial_noise_level = cur_noise;
    calib_status.initial_spike_rejection = cur_spike;

    int best_noise = cur_noise;
    int best_spike = cur_spike;

    // We'll try noise levels from low (1) up to 7, preferring lower numbers (more sensitive)
    for (int nl = 1; nl <= 7; ++nl) {
        // try a few spike values for each noise level (start at current)
        for (int sr = 1; sr <= 11; ++sr) {
            // apply
            as3935_apply_levels(nl, sr);
            // sample windows using IRQ-driven counters
            // clear counters for this candidate
            __sync_lock_test_and_set(&calib_spur_counter, 0);
            __sync_lock_test_and_set(&calib_lightning_counter, 0);
            int total_spur = 0;
            int total_light = 0;
            for (int w = 0; w < 3; ++w) {
                if (!calib_status.running) goto cancel_calib;
                vTaskDelay(pdMS_TO_TICKS(window_seconds * 1000));
                // sample counters atomically
                int sp = __sync_fetch_and_add(&calib_spur_counter, 0);
                int li = __sync_fetch_and_add(&calib_lightning_counter, 0);
                total_spur += sp;
                total_light += li;
                // reset for next window
                __sync_lock_test_and_set(&calib_spur_counter, 0);
                __sync_lock_test_and_set(&calib_lightning_counter, 0);
            }
            calib_status.windows_sampled += 3;
            calib_status.spur_count += total_spur;
            calib_status.lightning_count += total_light;

            float spur_per_min = (float)total_spur / (3.0f * window_seconds) * 60.0f;
            ESP_LOGI(TAG, "calib candidate nl=%d sr=%d spur_per_min=%.2f total_spur=%d total_light=%d", nl, sr, spur_per_min, total_spur, total_light);
            if (spur_per_min <= target_spur_per_min) {
                best_noise = nl;
                best_spike = sr;
                ESP_LOGI(TAG, "calib found candidate nl=%d sr=%d", nl, sr);
                goto found_good;
            }
        }
    }

found_good:
    // apply best found
    as3935_apply_levels(best_noise, best_spike);
    calib_status.final_noise_level = best_noise;
    calib_status.final_spike_rejection = best_spike;
    snprintf(calib_status.message, sizeof(calib_status.message), "Completed: noise=%d spike=%d", best_noise, best_spike);
    calib_status.running = false;
    calib_task = NULL;
    vTaskDelete(NULL);

cancel_calib:
    snprintf(calib_status.message, sizeof(calib_status.message), "Cancelled");
    calib_status.running = false;
    calib_task = NULL;
    vTaskDelete(NULL);
}

// Start calibration HTTP handler
esp_err_t as3935_calibrate_start_handler(httpd_req_t *req)
{
    if (calib_status.running) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"calibration already running\"}\n");
        return ESP_OK;
    }
    // reset status
    memset(&calib_status, 0, sizeof(calib_status));
    // create task
    if (xTaskCreate(calib_task_fn, "calib_task", 4096, NULL, 5, &calib_task) != pdPASS) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"failed to start task\"}\n");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"calibration started\"}\n");
    return ESP_OK;
}

// Status handler
esp_err_t as3935_calibrate_status_handler(httpd_req_t *req)
{
    char out[512];
    snprintf(out, sizeof(out), "{\"running\":%s,\"initial_noise\":%d,\"initial_spike\":%d,\"final_noise\":%d,\"final_spike\":%d,\"spur_count\":%d,\"lightning_count\":%d,\"msg\":\"%s\"}\n",
             calib_status.running ? "true" : "false",
             calib_status.initial_noise_level,
             calib_status.initial_spike_rejection,
             calib_status.final_noise_level,
             calib_status.final_spike_rejection,
             calib_status.spur_count,
             calib_status.lightning_count,
             calib_status.message);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    return ESP_OK;
}

// Cancel calibration (if running)
esp_err_t as3935_calibrate_cancel_handler(httpd_req_t *req)
{
    if (!calib_status.running) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"not running\"}\n");
        return ESP_OK;
    }
    // Signal running task to stop by clearing running; task polls this indirectly
    calib_status.running = false;
    // task will self-terminate shortly; clear counters
    calib_spur_counter = 0;
    calib_lightning_counter = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"cancel requested\"}\n");
    return ESP_OK;
}

// Apply current calibration counters as a new setting (simple snapshot)
esp_err_t as3935_calibrate_apply_handler(httpd_req_t *req)
{
    // Convert counters into a simple recommended setting: if many spurs, increase noise
    int sp = __sync_fetch_and_add(&calib_spur_counter, 0);
    int li = __sync_fetch_and_add(&calib_lightning_counter, 0);
    // read current reg values
    uint8_t r1=0,r2=0;
    as3935_spi_read_reg(0x01, &r1);
    as3935_spi_read_reg(0x02, &r2);
    int cur_noise = (r1 & 0x70) >> 4;
    int cur_spike = (r2 & 0x0F) >> 0;
    if (cur_noise < 1) cur_noise = 1;
    if (cur_spike < 1) cur_spike = 1;
    int new_noise = cur_noise;
    int new_spike = cur_spike;
    if (sp > li * 2) new_noise = cur_noise + 1;
    if (new_noise > 7) new_noise = 7;

    // Snapshot current regs to backup in NVS before applying
    char backup[512];
    as3935_snapshot_regs_json(backup, sizeof(backup));
    as3935_save_backup_nvs(backup);

    // Capture baseline counters (zero then sample for short baseline window)
    __sync_lock_test_and_set(&calib_spur_counter, 0);
    __sync_lock_test_and_set(&calib_lightning_counter, 0);
    // small baseline sample
    vTaskDelay(pdMS_TO_TICKS(5000));
    int baseline_sp = __sync_fetch_and_add(&calib_spur_counter, 0);
    int baseline_li = __sync_fetch_and_add(&calib_lightning_counter, 0);

    // apply new levels
    as3935_apply_levels((uint8_t)new_noise, (uint8_t)new_spike);

    // Save applied setting snapshot as current regs
    char applied[256];
    as3935_snapshot_regs_json(applied, sizeof(applied));
    as3935_save_config_nvs(applied);

    // Start validation task to auto-rollback if metrics worsen
    typedef struct { int baseline_sp; int baseline_li; int duration_s; } validate_params_t;
    validate_params_t *p = malloc(sizeof(*p));
    if (p) {
        p->baseline_sp = baseline_sp;
        p->baseline_li = baseline_li;
        p->duration_s = 20; // validate for 20s
        // Create validation task to monitor spurious/lightning counts and rollback if needed
        if (xTaskCreate(validate_task_fn, "validate_task", 4096, p, 5, &validate_task) != pdPASS) {
            ESP_LOGW(TAG, "Failed to create validate_task");
            free(p);
        }
    }

    // Note: we intentionally return quickly; validation/rollback runs in background
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"applied and validating\"}\n");
    return ESP_OK;
}


// Debug HTTP handler: GET /api/as3935/reg?addr=0x03
esp_err_t as3935_reg_read_handler(httpd_req_t *req)
{
    // parse query string for addr parameter
    char buf[64];
    int qlen = httpd_req_get_url_query_len(req);
    if (qlen <= 0 || qlen >= (int)sizeof(buf)) { http_helpers_send_400(req); return ESP_FAIL; }
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK) {
        http_helpers_send_400(req);
        return ESP_FAIL;
    }
    char addr_str[32] = {0};
    if (httpd_query_key_value(buf, "addr", addr_str, sizeof(addr_str)) != ESP_OK) {
        http_helpers_send_400(req);
        return ESP_FAIL;
    }
    // parse hex or decimal
    int addr = (int)strtol(addr_str, NULL, 0);
    if (addr < 0 || addr > 0xFF) { http_helpers_send_400(req); return ESP_FAIL; }
    uint8_t val = 0;
    if (as3935_spi_read_reg((uint8_t)addr, &val) != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    char out[128];
    snprintf(out, sizeof(out), "{\"addr\":\"0x%02X\",\"value\":%d}\n", addr, val);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, out);
    return ESP_OK;
}

