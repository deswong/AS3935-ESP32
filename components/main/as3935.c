#include "as3935.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
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
            }
        }
    }
}

static const char *TAG = "as3935";
static as3935_config_t current_cfg;

// SPI register access. AS3935 expects an 8-bit register address, MSB=R/W bit
// We'll assume the adapter expects a standard 1-byte command followed by 1 byte data.
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
    // Example: write some placeholder registers; real regs depend on sensor mapping
    esp_err_t err = as3935_spi_write_reg(0x00, 0x96);
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
            as3935_spi_write_reg((uint8_t)reg, (uint8_t)val);
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
        // Map to bits [5:4] and possible extra bit for 9 case per ESPhome mapping
        if (lt==1) {
            as3935_write_masked(0x02, 0x30, 0x00, 4);
        } else if (lt==5) {
            as3935_write_masked(0x02, 0x30, 0x01, 4);
        } else if (lt==9) {
            // ESPhome uses an extra bit; write 1 at pos 5 (value 0x20) and 1 at pos4? We'll write 0x11 shifted
            as3935_write_masked(0x02, 0x30, 0x01, 4);
            // set additional high bit via masked write elsewhere if needed
        } else if (lt==16) {
            as3935_write_masked(0x02, 0x30, 0x03, 4);
        }
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

