#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

// forward-declare http req type to avoid heavy header dependencies in other
// translation units which may include this header before esp_http_server.h
typedef struct httpd_req httpd_req_t;

typedef struct {
    int spi_host; // SPI host (e.g. 1)
    int sclk_pin;
    int mosi_pin;
    int miso_pin;
    int cs_pin;
    int irq_pin;
} as3935_config_t;

bool as3935_init(const as3935_config_t *cfg);
bool as3935_configure_default(void);
// Register callback for lightning events (distance, energy, timestamp)
typedef void (*as3935_event_cb_t)(int distance_km, int energy, uint32_t timestamp);
void as3935_set_event_callback(as3935_event_cb_t cb);

// Convenience to publish an event to MQTT (topic/payload form)
void as3935_publish_event_json(const char *topic, const char *json_payload);

// configure IRQ GPIO and start monitoring
bool as3935_setup_irq(int irq_pin);

// Apply register map provided as JSON object (e.g. {"0x03": 150, "0x04": 5})
bool as3935_apply_config_json(const char *json);

// Save/Load config in NVS
esp_err_t as3935_save_config_nvs(const char *json);
esp_err_t as3935_load_config_nvs(char *out, size_t len);

// Test hook: allow tests to override the SPI write function
void as3935_set_spi_write_fn(esp_err_t (*fn)(uint8_t reg, uint8_t val));

// HTTP handlers
esp_err_t as3935_save_handler(httpd_req_t *req);
esp_err_t as3935_status_handler(httpd_req_t *req);

// Pin config save/load
esp_err_t as3935_save_pins_nvs(int i2c_port, int sda, int scl, int irq);
esp_err_t as3935_load_pins_nvs(int *i2c_port, int *sda, int *scl, int *irq);
esp_err_t as3935_pins_save_handler(httpd_req_t *req);
esp_err_t as3935_pins_status_handler(httpd_req_t *req);

// Apply ESPhome-like parameters via HTTP POST (JSON body)
esp_err_t as3935_params_handler(httpd_req_t *req);

// Auto-calibration (sweep) API: start/cancel/status
esp_err_t as3935_calibrate_start_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_status_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_cancel_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_apply_handler(httpd_req_t *req);

// Initialize AS3935 using saved pins (load from NVS)
bool as3935_init_from_nvs(void);

// Debug: read a single register via HTTP GET ?addr=0x03
esp_err_t as3935_reg_read_handler(httpd_req_t *req);

// Test hooks (used by unit tests)
// Synchronously validate baseline vs current counters and restore backup if needed.
// Returns true if validation passed (no restore), false if rollback occurred.
bool as3935_validate_and_maybe_restore(int baseline_sp, int baseline_li, int duration_s);

// Test helpers to set/read IRQ-driven counters (for unit tests only)
void as3935_test_set_counters(int sp, int li);
void as3935_test_get_counters(int *sp, int *li);
