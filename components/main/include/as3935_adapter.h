/**
 * @file as3935_adapter.h
 * @brief Adapter layer for esp_as3935 library to maintain existing API compatibility
 * 
 * This header provides wrapper functions around the esp_as3935 library
 * to maintain backward compatibility with the existing custom driver API.
 */

#ifndef AS3935_ADAPTER_H
#define AS3935_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <esp_err.h>

// Forward-declare http req type
typedef struct httpd_req httpd_req_t;

/**
 * @brief I2C bus configuration structure for AS3935 adapter
 * Note: Separate from library's as3935_config_t which configures the sensor device
 */
typedef struct {
    int i2c_port;    // I2C port number (0 or 1)
    int sda_pin;     // SDA (data) pin
    int scl_pin;     // SCL (clock) pin
    int irq_pin;     // IRQ pin for lightning detection
    int i2c_addr;    // I2C slave address (0x00-0xFF, typically 0x03 for AS3935)
} as3935_adapter_config_t;

/**
 * @brief Event callback type for lightning events
 */
typedef void (*as3935_event_cb_t)(int distance_km, int energy, uint32_t timestamp);

/**
 * @brief Initialize AS3935 adapter with I2C bus (backward compatible wrapper)
 * Note: This is different from library's as3935_init which takes 3 parameters
 */
bool as3935_adapter_bus_init(const as3935_adapter_config_t *cfg);

/**
 * @brief Initialize the AS3935 sensor device library handle
 * Must be called after as3935_init to set up the sensor on the I2C bus
 */
esp_err_t as3935_init_sensor_handle(int i2c_addr, int irq_pin);

/**
 * @brief Configure with default settings
 */
bool as3935_configure_default(void);

/**
 * @brief Set event callback for lightning detection
 */
void as3935_set_event_callback(as3935_event_cb_t cb);

/**
 * @brief Setup IRQ GPIO monitoring
 */
bool as3935_setup_irq(int irq_pin);

/**
 * @brief Apply register configuration from JSON string
 */
bool as3935_apply_config_json(const char *json);

/**
 * @brief Save configuration to NVS
 */
esp_err_t as3935_save_config_nvs(const char *json);

/**
 * @brief Load configuration from NVS
 */
esp_err_t as3935_load_config_nvs(char *out, size_t len);

/**
 * @brief Initialize from NVS stored pins
 */
bool as3935_init_from_nvs(void);

/**
 * @brief Save pin configuration to NVS
 */
esp_err_t as3935_save_pins_nvs(int i2c_port, int sda, int scl, int irq);

/**
 * @brief Load pin configuration from NVS
 */
esp_err_t as3935_load_pins_nvs(int *i2c_port, int *sda, int *scl, int *irq);

/**
 * @brief Save I2C address to NVS
 */
esp_err_t as3935_save_addr_nvs(int i2c_addr);

/**
 * @brief Load I2C address from NVS
 */
esp_err_t as3935_load_addr_nvs(int *i2c_addr);

/**
 * @brief HTTP Handlers
 */
esp_err_t as3935_save_handler(httpd_req_t *req);
esp_err_t as3935_status_handler(httpd_req_t *req);
esp_err_t as3935_pins_save_handler(httpd_req_t *req);
esp_err_t as3935_pins_status_handler(httpd_req_t *req);
esp_err_t as3935_addr_save_handler(httpd_req_t *req);
esp_err_t as3935_addr_status_handler(httpd_req_t *req);
esp_err_t as3935_params_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_start_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_status_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_cancel_handler(httpd_req_t *req);
esp_err_t as3935_calibrate_apply_handler(httpd_req_t *req);
esp_err_t as3935_register_read_handler(httpd_req_t *req);
esp_err_t as3935_register_write_handler(httpd_req_t *req);
esp_err_t as3935_registers_all_handler(httpd_req_t *req);
esp_err_t as3935_post_handler(httpd_req_t *req);
esp_err_t as3935_reg_read_handler(httpd_req_t *req);

/**
 * @brief Advanced Settings HTTP Handlers
 */
esp_err_t as3935_afe_handler(httpd_req_t *req);
esp_err_t as3935_noise_level_handler(httpd_req_t *req);
esp_err_t as3935_spike_rejection_handler(httpd_req_t *req);
esp_err_t as3935_min_strikes_handler(httpd_req_t *req);
esp_err_t as3935_disturber_handler(httpd_req_t *req);
esp_err_t as3935_watchdog_handler(httpd_req_t *req);
esp_err_t as3935_reboot_handler(httpd_req_t *req);

/**
 * @brief Sensor data getters (for MQTT publishing and other uses)
 */
esp_err_t as3935_get_sensor_energy(uint32_t *energy);
esp_err_t as3935_get_sensor_distance(uint8_t *distance_km);

/**
 * @brief Test hooks
 */
bool as3935_validate_and_maybe_restore(int baseline_sp, int baseline_li, int duration_s);
void as3935_test_set_counters(int sp, int li);
void as3935_test_get_counters(int *sp, int *li);
void as3935_set_i2c_write_fn(esp_err_t (*fn)(uint8_t reg, uint8_t val));

#ifdef __cplusplus
}
#endif

#endif // AS3935_ADAPTER_H
