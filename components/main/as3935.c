#include "as3935.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include "as3935_adapter.h"  // For sensor data getters

// I2C header
#if defined(__has_include)
  #if __has_include(<driver/i2c_master.h>)
    #include <driver/i2c_master.h>
    #define AS3935_HAVE_I2C 1
  #elif __has_include(<esp_driver_i2c/i2c_master.h>)
    #include <esp_driver_i2c/i2c_master.h>
    #define AS3935_HAVE_I2C 1
  #elif __has_include("driver/i2c_master.h")
    #include "driver/i2c_master.h"
    #define AS3935_HAVE_I2C 1
  #else
    #define AS3935_HAVE_I2C 0
  #endif

  // GPIO header
  #if __has_include(<driver/gpio.h>)
    #include <driver/gpio.h>
    #define AS3935_HAVE_GPIO 1
  #elif __has_include(<esp_driver_gpio/gpio.h>)
    #include <esp_driver_gpio/gpio.h>
    #define AS3935_HAVE_GPIO 1
  #elif __has_include("driver/gpio.h")
    #include "driver/gpio.h"
    #define AS3935_HAVE_GPIO 1
  #else
    #define AS3935_HAVE_GPIO 0
  #endif
#else
  #include <driver/i2c_master.h>
  #include <driver/gpio.h>
  #define AS3935_HAVE_I2C 1
  #define AS3935_HAVE_GPIO 1
#endif

#if !AS3935_HAVE_I2C || !AS3935_HAVE_GPIO
  #error "Missing I2C/GPIO driver headers. Ensure 'driver' components are in REQUIRES."
#endif

#include "esp_http_server.h"
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

static QueueHandle_t gpio_evt_queue = NULL;
static const char *TAG = "as3935";
static i2c_master_dev_handle_t i2c_dev = NULL;
static i2c_master_bus_handle_t i2c_bus = NULL;

// AS3935 I2C slave address (typically 0x03)
#define AS3935_I2C_ADDR 0x03

// Forward declarations
static esp_err_t as3935_i2c_read_reg(uint8_t reg, uint8_t *val);
static esp_err_t as3935_i2c_write_reg(uint8_t reg, uint8_t val);
static void as3935_publish_event(const char *json_payload);

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // Log that interrupt was triggered
    ESP_LOGI(TAG, "[IRQ] GPIO interrupt triggered on pin %d at %u ms", gpio_num, esp_log_timestamp());
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static as3935_event_cb_t event_cb = NULL;
static volatile int calib_spur_counter = 0;
static volatile int calib_lightning_counter = 0;

typedef struct {
    bool running;
    int initial_noise_level;
    int initial_spike_rejection;
    int final_noise_level;
    int final_spike_rejection;
    char status[128];
} calib_status_t;

static calib_status_t calib_status = {0};

// Auto-calibration backup state
typedef struct {
    uint8_t reg01;
    uint8_t reg02;
    uint8_t reg07;
} as3935_backup_t;

static as3935_backup_t backup_state = {0};

__attribute__((unused))
static esp_err_t as3935_save_backup(void)
{
    esp_err_t err1 = as3935_i2c_read_reg(0x01, &backup_state.reg01);
    esp_err_t err2 = as3935_i2c_read_reg(0x02, &backup_state.reg02);
    esp_err_t err3 = as3935_i2c_read_reg(0x07, &backup_state.reg07);
    return (err1 == ESP_OK && err2 == ESP_OK && err3 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

static esp_err_t as3935_restore_backup(void)
{
    esp_err_t err1 = as3935_i2c_write_reg(0x01, backup_state.reg01);
    esp_err_t err2 = as3935_i2c_write_reg(0x02, backup_state.reg02);
    esp_err_t err3 = as3935_i2c_write_reg(0x07, backup_state.reg07);
    return (err1 == ESP_OK && err2 == ESP_OK && err3 == ESP_OK) ? ESP_OK : ESP_FAIL;
}

bool as3935_validate_and_maybe_restore(int baseline_sp, int baseline_li, int duration_s)
{
    __sync_lock_test_and_set(&calib_spur_counter, 0);
    __sync_lock_test_and_set(&calib_lightning_counter, 0);
    if (duration_s > 0) vTaskDelay(pdMS_TO_TICKS(duration_s * 1000));
    int sp_new = __sync_fetch_and_add(&calib_spur_counter, 0);
    float scale = (float)duration_s / 5.0f;
    int baseline_sp_scaled = (int)(baseline_sp * scale + 0.5f);
    if (sp_new > baseline_sp_scaled * 2) {
        if (as3935_restore_backup() == ESP_OK) {
            return false;
        }
        return false;
    }
    return true;
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
            ESP_LOGI(TAG, "[TASK] gpio_task woken up by interrupt on pin %d at %u ms", io_num, esp_log_timestamp());
            
            // Get sensor data - these now use non-blocking I2C wrapped with mutex
            uint32_t energy = 0;
            uint8_t distance_km = 0;
            bool has_data = false;
            
            if (as3935_get_sensor_energy(&energy) == ESP_OK && 
                as3935_get_sensor_distance(&distance_km) == ESP_OK) {
                has_data = true;
            }
            
            ESP_LOGI(TAG, "[TASK] Sensor data: energy=%u, distance=%d km, has_data=%d", energy, distance_km, has_data);
            
            // Publish event with full sensor data
            if (event_cb) event_cb(distance_km, energy, (uint32_t)esp_log_timestamp());
            
            char payload[256];
            snprintf(payload, sizeof(payload), 
                "{\"distance_km\":%d,\"energy\":%u,\"timestamp\":%u}", 
                distance_km, energy, (unsigned int)esp_log_timestamp());
            
            as3935_publish_event(payload);
            events_broadcast("lightning", payload);
            
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

static as3935_config_t current_cfg;

// I2C register access
typedef esp_err_t (*as3935_i2c_write_fn_t)(uint8_t reg, uint8_t val);
static as3935_i2c_write_fn_t as3935_i2c_write_fn = NULL;

static esp_err_t as3935_i2c_write_reg(uint8_t reg, uint8_t val)
{
    if (!i2c_dev) {
        ESP_LOGW(TAG, "I2C write reg 0x%02x=0x%02x: I2C device not initialized", reg, val);
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t write_buf[2] = {reg, val};
    esp_err_t err = i2c_master_transmit(i2c_dev, write_buf, sizeof(write_buf), -1);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C write reg 0x%02x=0x%02x failed: error %d", reg, val, err);
    }
    return err;
}

void as3935_set_i2c_write_fn(esp_err_t (*fn)(uint8_t reg, uint8_t val))
{
    if (fn) as3935_i2c_write_fn = fn;
    else as3935_i2c_write_fn = as3935_i2c_write_reg;
}

static esp_err_t as3935_i2c_read_reg(uint8_t reg, uint8_t *val)
{
    if (!i2c_dev) {
        ESP_LOGW(TAG, "I2C read reg 0x%02x: I2C device not initialized", reg);
        return ESP_ERR_INVALID_STATE;
    }
    if (!val) {
        ESP_LOGW(TAG, "I2C read reg 0x%02x: val pointer is NULL", reg);
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = i2c_master_transmit_receive(i2c_dev, &reg, 1, val, 1, -1);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C read reg 0x%02x failed: error %d", reg, err);
    }
    return err;
}

bool as3935_init(const as3935_config_t *cfg)
{
    if (!cfg) return false;
    memcpy(&current_cfg, cfg, sizeof(current_cfg));

    // Clean up any existing I2C device/bus
    if (i2c_dev) {
        i2c_master_bus_rm_device(i2c_dev);
        i2c_dev = NULL;
        ESP_LOGI(TAG, "Previous I2C device removed");
    }
    
    if (i2c_bus) {
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
        ESP_LOGI(TAG, "I2C bus deinitialized");
    }

    ESP_LOGI(TAG, "Configuring I2C port %d: SDA=%d, SCL=%d", cfg->i2c_port, cfg->sda_pin, cfg->scl_pin);

    // Initialize I2C master bus
    i2c_master_bus_config_t bus_config = {
        .i2c_port = cfg->i2c_port,
        .sda_io_num = cfg->sda_pin,
        .scl_io_num = cfg->scl_pin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ i2c_new_master_bus FAILED: error code %d", err);
        ESP_LOGE(TAG, "  Possible causes: GPIO in use, invalid GPIO, bus already initialized");
        return false;
    }
    ESP_LOGI(TAG, "✓ I2C bus initialized successfully");

    // Add AS3935 device to I2C bus
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cfg->i2c_addr > 0 ? cfg->i2c_addr : 0x03,  // Use configured address or default 0x03
        .scl_speed_hz = 100000,  // 100 kHz standard I2C speed
    };

    err = i2c_master_bus_add_device(i2c_bus, &dev_config, &i2c_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "✗ i2c_master_bus_add_device FAILED: error code %d", err);
        ESP_LOGE(TAG, "  Possible causes: device already added, invalid address, bus not initialized");
        i2c_del_master_bus(i2c_bus);
        i2c_bus = NULL;
        return false;
    }
    ESP_LOGI(TAG, "✓ I2C device added successfully (AS3935 @ 0x%02X, 100kHz)", dev_config.device_address);

    ESP_LOGI(TAG, "✓ AS3935 I2C interface initialized on port %d", cfg->i2c_port);

    // Setup GPIO queue and task
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (!gpio_evt_queue) {
        ESP_LOGE(TAG, "✗ Failed to create GPIO event queue");
        return false;
    }
    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);
    ESP_LOGI(TAG, "✓ GPIO task and event queue initialized");

    ESP_LOGI(TAG, "AS3935 I2C initialization complete - sensor diagnostics available via HTTP API");

    return true;
}

bool as3935_configure_default(void)
{
    return true;
}

void as3935_set_event_callback(as3935_event_cb_t cb)
{
    event_cb = cb;
}

void as3935_publish_event_json(const char *topic, const char *json_payload)
{
    if (!topic || !json_payload) {
        ESP_LOGW(TAG, "[MQTT] publish_event_json: invalid arguments (topic=%p, payload=%p)", topic, json_payload);
        return;
    }
    ESP_LOGI(TAG, "[MQTT] Publishing to topic '%s': %s", topic, json_payload);
    mqtt_publish(topic, json_payload);
    ESP_LOGI(TAG, "[MQTT] Publish call completed");
}

bool as3935_setup_irq(int irq_pin)
{
    if (irq_pin < 0 || irq_pin > 46) {
        ESP_LOGW(TAG, "Invalid IRQ pin: %d", irq_pin);
        return false;
    }

    // Install GPIO ISR service if not already installed
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service failed: %d", err);
        return false;
    }

    err = gpio_set_direction(irq_pin, GPIO_MODE_INPUT);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_set_direction failed for pin %d", irq_pin);
        return false;
    }

    gpio_set_intr_type(irq_pin, GPIO_INTR_POSEDGE);

    err = gpio_isr_handler_add(irq_pin, gpio_isr_handler, (void*)(intptr_t)irq_pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_isr_handler_add failed for pin %d", irq_pin);
        return false;
    }

    ESP_LOGI(TAG, "AS3935 IRQ configured on pin %d", irq_pin);
    return true;
}

bool as3935_apply_config_json(const char *json)
{
    if (!json) return false;
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;

    bool modified = false;
    cJSON *item = NULL;

    cJSON_ArrayForEach(item, root) {
        const char *key = item->string;
        if (cJSON_IsNumber(item)) {
            uint8_t reg = (uint8_t)strtol(key, NULL, 0);
            uint8_t val = (uint8_t)(item->valueint & 0xFF);
            if (as3935_i2c_write_reg(reg, val) == ESP_OK) {
                modified = true;
            }
        }
    }

    cJSON_Delete(root);
    return modified;
}

esp_err_t as3935_save_config_nvs(const char *json)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, "config", json);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

esp_err_t as3935_load_config_nvs(char *out, size_t len)
{
    if (!out || len == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    err = nvs_get_str(h, "config", out, &len);
    nvs_close(h);
    return err;
}

esp_err_t as3935_save_pins_nvs(int i2c_port, int sda, int scl, int irq)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    nvs_set_i32(h, "i2c_port", i2c_port);
    nvs_set_i32(h, "sda", sda);
    nvs_set_i32(h, "scl", scl);
    nvs_set_i32(h, "irq", irq);

    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t as3935_load_pins_nvs(int *i2c_port, int *sda, int *scl, int *irq)
{
    if (!i2c_port || !sda || !scl || !irq) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    int32_t i2c_port_tmp, sda_tmp, scl_tmp, irq_tmp;
    
    // Only update if the key exists and is valid
    if (nvs_get_i32(h, "i2c_port", &i2c_port_tmp) == ESP_OK && i2c_port_tmp >= 0 && i2c_port_tmp <= 1) {
        *i2c_port = (int)i2c_port_tmp;
    }
    if (nvs_get_i32(h, "sda", &sda_tmp) == ESP_OK && sda_tmp >= 0 && sda_tmp <= 46) {
        *sda = (int)sda_tmp;
    }
    if (nvs_get_i32(h, "scl", &scl_tmp) == ESP_OK && scl_tmp >= 0 && scl_tmp <= 46) {
        *scl = (int)scl_tmp;
    }
    if (nvs_get_i32(h, "irq", &irq_tmp) == ESP_OK && (irq_tmp == -1 || (irq_tmp >= 0 && irq_tmp <= 46))) {
        *irq = (int)irq_tmp;
    }

    nvs_close(h);
    return ESP_OK;
}

// I2C address NVS functions
esp_err_t as3935_save_addr_nvs(int i2c_addr)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    nvs_set_i32(h, "i2c_addr", i2c_addr);

    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t as3935_load_addr_nvs(int *i2c_addr)
{
    if (!i2c_addr) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    int32_t addr_tmp;
    
    // Only update if the key exists and is valid (0x00-0xFF)
    if (nvs_get_i32(h, "i2c_addr", &addr_tmp) == ESP_OK && addr_tmp >= 0 && addr_tmp <= 255) {
        *i2c_addr = (int)addr_tmp;
    }

    nvs_close(h);
    return ESP_OK;
}

// Placeholder for removed SPI-specific function (for compatibility)
esp_err_t as3935_save_spi_pins_nvs(int spi_host, int sclk, int mosi, int miso, int cs, int irq)
{
    // Map SPI pins to I2C pins for backward compatibility
    return as3935_save_pins_nvs(spi_host, sclk, mosi, irq);
}

esp_err_t as3935_load_spi_pins_nvs(int *spi_host, int *sclk, int *mosi, int *miso, int *cs, int *irq)
{
    // Map I2C pins back to SPI parameter names
    int i2c_port, sda, scl;
    esp_err_t err = as3935_load_pins_nvs(&i2c_port, &sda, &scl, irq);
    if (err == ESP_OK) {
        *spi_host = i2c_port;
        *sclk = sda;
        *mosi = scl;
        if (miso) *miso = -1;
        if (cs) *cs = -1;
    }
    return err;
}

// HTTP Handlers
esp_err_t as3935_save_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;

    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        http_helpers_send_400(req);
        return ESP_FAIL;
    }

    const cJSON *config = cJSON_GetObjectItemCaseSensitive(root, "config");
    if (config && config->valuestring) {
        if (as3935_apply_config_json(config->valuestring) != ESP_OK) {
            cJSON_Delete(root);
            http_helpers_send_400(req);
            return ESP_FAIL;
        }
        as3935_save_config_nvs(config->valuestring);
    }

    cJSON_Delete(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_status_handler(httpd_req_t *req)
{
    uint8_t r0, r1, r3, r8;
    r0 = r1 = r3 = r8 = 0;

    as3935_i2c_read_reg(0x00, &r0);
    as3935_i2c_read_reg(0x01, &r1);
    as3935_i2c_read_reg(0x03, &r3);
    as3935_i2c_read_reg(0x08, &r8);

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"initialized\":%s,\"r0\":\"0x%02x\",\"r1\":\"0x%02x\",\"r3\":\"0x%02x\",\"r8\":\"0x%02x\"}\n",
             (i2c_dev != NULL) ? "true" : "false", r0, r1, r3, r8);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t as3935_pins_status_handler(httpd_req_t *req)
{
    int i2c_port = 0, sda = 0, scl = 0, irq = 0;
    if (as3935_load_pins_nvs(&i2c_port, &sda, &scl, &irq) == ESP_OK) {
        char buf[256];
        snprintf(buf, sizeof(buf), "{\"i2c_port\":%d,\"sda\":%d,\"scl\":%d,\"irq\":%d}\n",
                 i2c_port, sda, scl, irq);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, buf);
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{}\n");
    }
    return ESP_OK;
}

esp_err_t as3935_pins_save_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;

    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        http_helpers_send_400(req);
        return ESP_FAIL;
    }

    const cJSON *i2c_port = cJSON_GetObjectItemCaseSensitive(root, "i2c_port");
    const cJSON *sda = cJSON_GetObjectItemCaseSensitive(root, "sda");
    const cJSON *scl = cJSON_GetObjectItemCaseSensitive(root, "scl");
    const cJSON *irq = cJSON_GetObjectItemCaseSensitive(root, "irq");

    if (!cJSON_IsNumber(i2c_port) || !cJSON_IsNumber(sda) || !cJSON_IsNumber(scl) || !cJSON_IsNumber(irq)) {
        cJSON_Delete(root);
        http_helpers_send_400(req);
        return ESP_FAIL;
    }

    as3935_save_pins_nvs(i2c_port->valueint, sda->valueint, scl->valueint, irq->valueint);
    cJSON_Delete(root);

    // Load current I2C address from NVS
    int i2c_addr = 0x03;
    as3935_load_addr_nvs(&i2c_addr);

    as3935_config_t cfg = {
        .i2c_port = i2c_port->valueint,
        .sda_pin = sda->valueint,
        .scl_pin = scl->valueint,
        .irq_pin = irq->valueint,
        .i2c_addr = i2c_addr
    };

    if (!as3935_init(&cfg)) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"I2C init failed\"}\n");
        return ESP_FAIL;
    }
    as3935_setup_irq(irq->valueint);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_addr_status_handler(httpd_req_t *req)
{
    int i2c_addr = 0x03;  // Default address
    if (as3935_load_addr_nvs(&i2c_addr) == ESP_OK || i2c_addr == 0x03) {
        char buf[128];
        snprintf(buf, sizeof(buf), "{\"i2c_addr\":\"0x%02X\",\"i2c_addr_dec\":%d}\n", i2c_addr, i2c_addr);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, buf);
    } else {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"i2c_addr\":\"0x03\",\"i2c_addr_dec\":3}\n");
    }
    return ESP_OK;
}

esp_err_t as3935_addr_save_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) return ESP_FAIL;

    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        http_helpers_send_400(req);
        return ESP_FAIL;
    }

    const cJSON *addr_field = cJSON_GetObjectItemCaseSensitive(root, "i2c_addr");
    if (!addr_field) {
        cJSON_Delete(root);
        http_helpers_send_400(req);
        return ESP_FAIL;
    }

    int i2c_addr = addr_field->valueint;
    
    // Validate address (0x00-0xFF)
    if (i2c_addr < 0 || i2c_addr > 255) {
        cJSON_Delete(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"Address must be 0x00-0xFF (0-255)\"}\n");
        return ESP_FAIL;
    }

    as3935_save_addr_nvs(i2c_addr);
    cJSON_Delete(root);

    // Re-initialize I2C with new address
    int i2c_port = 0, sda = 21, scl = 22, irq = 0;
    as3935_load_pins_nvs(&i2c_port, &sda, &scl, &irq);

    as3935_config_t cfg = {
        .i2c_port = i2c_port,
        .sda_pin = sda,
        .scl_pin = scl,
        .irq_pin = irq,
        .i2c_addr = i2c_addr
    };

    if (!as3935_init(&cfg)) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"I2C init failed with new address\"}\n");
        return ESP_FAIL;
    }

    char response[128];
    snprintf(response, sizeof(response), "{\"ok\":true,\"i2c_addr\":\"0x%02X\"}\n", i2c_addr);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

// Stub implementations for compatibility
esp_err_t as3935_params_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_calibrate_start_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_calibrate_status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"running\":false}\n");
    return ESP_OK;
}

esp_err_t as3935_calibrate_cancel_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t as3935_calibrate_apply_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

bool as3935_init_from_nvs(void)
{
    int i2c_port = 0, sda = 21, scl = 22, irq = 0, i2c_addr = 0x03;  // Default pins and address

    esp_err_t err = as3935_load_pins_nvs(&i2c_port, &sda, &scl, &irq);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded I2C pins from NVS: port=%d sda=%d scl=%d irq=%d",
                 i2c_port, sda, scl, irq);
    } else {
        ESP_LOGI(TAG, "No I2C pins in NVS, using defaults");
    }

    err = as3935_load_addr_nvs(&i2c_addr);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded I2C address from NVS: 0x%02X", i2c_addr);
    } else {
        ESP_LOGI(TAG, "No I2C address in NVS, using default: 0x03");
        i2c_addr = 0x03;
    }

    as3935_config_t cfg = {
        .i2c_port = i2c_port,
        .sda_pin = sda,
        .scl_pin = scl,
        .irq_pin = irq,
        .i2c_addr = i2c_addr
    };

    if (!as3935_init(&cfg)) {
        ESP_LOGE(TAG, "Failed to initialize AS3935 with config: port=%d sda=%d scl=%d irq=%d addr=0x%02X",
                 i2c_port, sda, scl, irq, i2c_addr);
        return false;
    }
    as3935_setup_irq(irq);
    return true;
}

esp_err_t as3935_reg_read_handler(httpd_req_t *req)
{
    uint8_t reg = 0x00;
    
    // Get query string length first
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                char param[4] = {0};
                if (httpd_query_key_value(buf, "addr", param, sizeof(param)) == ESP_OK) {
                    reg = (uint8_t)strtol(param, NULL, 0);
                }
            }
            free(buf);
        }
    }

    uint8_t val = 0;
    esp_err_t err = as3935_i2c_read_reg(reg, &val);

    char response[128];
    snprintf(response, sizeof(response),
             "{\"reg\":\"0x%02x\",\"value\":\"0x%02x\",\"error\":%d}\n",
             reg, val, (err != ESP_OK));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, response);
    return ESP_OK;
}

esp_err_t as3935_post_handler(httpd_req_t *req)
{
    if (!i2c_dev) {
        char json[256];
        snprintf(json, sizeof(json), "{\"status\":\"FAILED\",\"error\":\"I2C device not initialized\",\"step\":0}\n");
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    uint8_t test_val = 0xFF;
    char json[512] = {0};

    ESP_LOGI(TAG, "POST via HTTP: Starting I2C test...");
    esp_err_t err = as3935_i2c_read_reg(0x00, &test_val);
    if (err != ESP_OK) {
        snprintf(json, sizeof(json), "{\"status\":\"FAILED\",\"error\":\"Read failed\",\"step\":1,\"error_code\":%d}\n", err);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    uint8_t initial_val = test_val;

    // Check if sensor is responding (all zeros = not present)
    if (initial_val == 0x00) {
        snprintf(json, sizeof(json), "{\"status\":\"FAILED\",\"error\":\"Sensor not responding - all registers read 0x00\",\"step\":1,\"reg0\":\"0x%02x\"}\n", initial_val);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    // Step 2: Write test
    esp_err_t err_write = as3935_i2c_write_reg(0x00, 0x0E);
    if (err_write != ESP_OK) {
        snprintf(json, sizeof(json), "{\"status\":\"FAILED\",\"error\":\"Write failed\",\"step\":2,\"error_code\":%d}\n", err_write);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    // Step 3: Readback
    uint8_t readback = 0;
    err = as3935_i2c_read_reg(0x00, &readback);
    if (err != ESP_OK) {
        snprintf(json, sizeof(json), "{\"status\":\"FAILED\",\"error\":\"Readback failed\",\"step\":3,\"error_code\":%d}\n", err);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    // Step 4: Read other registers
    uint8_t r1, r3, r8;
    r1 = r3 = r8 = 0xFF;
    as3935_i2c_read_reg(0x01, &r1);
    as3935_i2c_read_reg(0x03, &r3);
    as3935_i2c_read_reg(0x08, &r8);

    // Check if all registers still zero (sensor not present)
    if (readback == 0x00 && r1 == 0x00 && r3 == 0x00 && r8 == 0x00) {
        snprintf(json, sizeof(json),
            "{\"status\":\"FAILED\",\"error\":\"Sensor not present - all registers return 0x00\",\"step\":4,\"reg0\":\"0x%02x\",\"reg1\":\"0x%02x\",\"reg3\":\"0x%02x\",\"reg8\":\"0x%02x\"}\n",
            readback, r1, r3, r8);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }

    // Return success with register values
    snprintf(json, sizeof(json),
        "{\"status\":\"PASSED\",\"initial_reg0\":\"0x%02x\",\"readback_reg0\":\"0x%02x\",\"reg1\":\"0x%02x\",\"reg3\":\"0x%02x\",\"reg8\":\"0x%02x\",\"notes\":\"Sensor is responsive\"}\n",
        initial_val, readback, r1, r3, r8);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    return ESP_OK;
}

// HTTP handler to read a register value
// GET /api/as3935/register/read?reg=0x01
esp_err_t as3935_register_read_handler(httpd_req_t *req)
{
    char buf[128] = {0};
    char reg_str[16] = {0};
    
    // Get reg parameter
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1 && buf_len <= sizeof(buf)) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "reg", reg_str, sizeof(reg_str)) == ESP_OK) {
                uint8_t reg = (uint8_t)strtol(reg_str, NULL, 0);
                uint8_t val = 0;
                
                if (as3935_i2c_read_reg(reg, &val) == ESP_OK) {
                    char json[256] = {0};
                    snprintf(json, sizeof(json), "{\"reg\":\"0x%02x\",\"value\":\"0x%02x\",\"value_dec\":%d}\n", reg, val, val);
                    httpd_resp_set_type(req, "application/json");
                    httpd_resp_sendstr(req, json);
                    return ESP_OK;
                }
            }
        }
    }
    
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "{\"error\":\"Failed to read register\"}\n");
    return ESP_OK;
}

// HTTP handler to write a register value
// POST /api/as3935/register/write with JSON body: {"reg":0x01, "value":0x20}
esp_err_t as3935_register_write_handler(httpd_req_t *req)
{
    char buf[512] = {0};
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    
    if (ret <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Empty body\"}\n");
        return ESP_OK;
    }
    
    buf[ret] = '\0';
    cJSON *root = cJSON_Parse(buf);
    
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"error\":\"Invalid JSON\"}\n");
        return ESP_OK;
    }
    
    uint8_t reg = 0, val = 0;
    bool valid = false;
    
    // Handle "reg" as either number or hex string
    const cJSON *reg_item = cJSON_GetObjectItemCaseSensitive(root, "reg");
    if (cJSON_IsNumber(reg_item)) {
        reg = (uint8_t)(reg_item->valueint & 0xFF);
        valid = true;
    } else if (cJSON_IsString(reg_item)) {
        reg = (uint8_t)strtol(reg_item->valuestring, NULL, 0);
        valid = true;
    }
    
    // Handle "value" as either number or hex string
    const cJSON *val_item = cJSON_GetObjectItemCaseSensitive(root, "value");
    if (cJSON_IsNumber(val_item)) {
        val = (uint8_t)(val_item->valueint & 0xFF);
        valid = valid && true;
    } else if (cJSON_IsString(val_item)) {
        val = (uint8_t)strtol(val_item->valuestring, NULL, 0);
        valid = valid && true;
    }
    
    cJSON_Delete(root);
    
    if (valid) {
        if (as3935_i2c_write_reg(reg, val) == ESP_OK) {
            char json[256] = {0};
            snprintf(json, sizeof(json), "{\"ok\":true,\"reg\":\"0x%02x\",\"value\":\"0x%02x\",\"value_dec\":%d}\n", reg, val, val);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, json);
            return ESP_OK;
        }
    }
    
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_sendstr(req, "{\"error\":\"Failed to write register\"}\n");
    return ESP_OK;
}

// HTTP handler to read all AS3935 registers
// GET /api/as3935/registers/all
esp_err_t as3935_registers_all_handler(httpd_req_t *req)
{
    char json[1024] = {0};
    uint8_t vals[16] = {0};
    bool all_ok = true;
    
    // Try to read first 16 registers (0x00-0x0F, adjust as needed)
    for (int i = 0; i < 9; i++) {  // AS3935 has registers 0x00-0x08
        if (as3935_i2c_read_reg(i, &vals[i]) != ESP_OK) {
            all_ok = false;
            break;
        }
    }
    
    if (all_ok) {
        snprintf(json, sizeof(json),
            "{\"status\":\"ok\",\"registers\":{"
            "\"0x00\":\"0x%02x\",\"0x01\":\"0x%02x\",\"0x02\":\"0x%02x\",\"0x03\":\"0x%02x\","
            "\"0x04\":\"0x%02x\",\"0x05\":\"0x%02x\",\"0x06\":\"0x%02x\",\"0x07\":\"0x%02x\","
            "\"0x08\":\"0x%02x\"}}\n",
            vals[0], vals[1], vals[2], vals[3], vals[4], vals[5], vals[6], vals[7], vals[8]);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, json);
        return ESP_OK;
    }
    
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_sendstr(req, "{\"error\":\"Failed to read registers\"}\n");
    return ESP_OK;
}

// Stub for publish event
static void as3935_publish_event(const char *json_payload)
{
    if (!json_payload) {
        ESP_LOGW(TAG, "[MQTT] as3935_publish_event: null payload");
        return;
    }
    ESP_LOGI(TAG, "[MQTT] as3935_publish_event: publishing event");
    as3935_publish_event_json("as3935/event", json_payload);
}
