/**
 * @file as3935_adapter.c
 * @brief Adapter implementation for esp_as3935 library integration
 * 
 * This file implements the adapter layer that wraps the esp_as3935 library
 * and maintains backward compatibility with the existing custom driver API.
 */

#include "as3935_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include "http_helpers.h"
#include "driver/i2c_master.h"
#include <ctype.h>
#include "cJSON.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "app_mqtt.h"
#include "events.h"
#include "settings.h"

// Include the REAL library header for all types and function declarations
// This comes from components/esp_as3935/include/as3935.h
#include "../esp_as3935/include/as3935.h"

static const char *TAG = "as3935_adapter";

// Forward declarations for NVS and internal functions
static esp_err_t as3935_load_advanced_settings_nvs(int *afe, int *noise_level, int *spike_rejection,
                                                    int *min_strikes, bool *disturber_enabled, int *watchdog);
static esp_err_t as3935_save_advanced_settings_nvs(int afe, int noise_level, int spike_rejection,
                                                    int min_strikes, bool disturber_enabled, int watchdog);
static esp_err_t as3935_apply_advanced_settings(int afe, int noise_level, int spike_rejection,
                                                 int min_strikes, bool disturber_enabled, int watchdog);

// I2C and sensor state
static i2c_master_bus_handle_t g_i2c_bus = NULL;
static i2c_master_dev_handle_t g_i2c_device = NULL;  // Persistent I2C device handle for non-blocking reads
static SemaphoreHandle_t g_i2c_mutex = NULL;  // Mutex for thread-safe I2C access
static as3935_handle_t g_sensor_handle = NULL;  // Library device handle
static as3935_monitor_handle_t g_monitor_handle = NULL;  // Monitor handle for event loop
static bool g_initialized = false;
static as3935_event_cb_t g_event_callback = NULL;

// Configuration
static as3935_adapter_config_t g_config = {
    .i2c_port = 0,
    .sda_pin = 21,
    .scl_pin = 22,
    .irq_pin = 23,
    .i2c_addr = 0x03
};

// Forward declarations
static esp_err_t as3935_i2c_read_byte_nb(uint8_t reg_addr, uint8_t *value);
static esp_err_t as3935_i2c_write_byte_nb(uint8_t reg_addr, uint8_t value);

/**
 * @brief AS3935 event handler - processes lightning, disturber, and noise events
 * 
 * This handler is called by the AS3935 monitor task when events occur.
 * It publishes register status to MQTT and broadcasts via SSE.
 * Device availability is published to MQTT topic 'as3935/availability' as 'online' or 'offline' for OpenHAB/Home Assistant integration.
 */
static void as3935_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGI(TAG, "[EVENT] AS3935 event received: event_id=%d", (int)event_id);
    
    // Cast event data to the monitor base structure
    as3935_monitor_base_t *monitor_data = (as3935_monitor_base_t *)event_data;
    
    // Read current register values for status
    uint8_t r0 = 0, r1 = 0, r3 = 0, r8 = 0;
    as3935_i2c_read_byte_nb(0x00, &r0);
    as3935_i2c_read_byte_nb(0x01, &r1);
    as3935_i2c_read_byte_nb(0x03, &r3);
    as3935_i2c_read_byte_nb(0x08, &r8);
    
    // Build JSON payload with event data and register status
    char payload[768];
    const char *event_type = "unknown";
    const char *event_description = "Unknown event";
    
    switch (event_id) {
        case AS3935_INT_LIGHTNING:
            event_type = "lightning";
            event_description = "Lightning Strike Detected";
            ESP_LOGI(TAG, "[EVENT] Lightning detected! Distance=%d km, Energy=%lu", 
                     monitor_data->lightning_distance, (unsigned long)monitor_data->lightning_energy);
            snprintf(payload, sizeof(payload),
                "{\"event\":\"%s\",\"description\":\"%s\",\"distance_km\":%d,\"distance_description\":\"%s\","
                "\"energy\":%lu,\"energy_description\":\"%s\","
                "\"r0\":\"0x%02x\",\"r1\":\"0x%02x\",\"r3\":\"0x%02x\",\"r8\":\"0x%02x\","
                "\"timestamp\":%u}",
                event_type, event_description,
                monitor_data->lightning_distance,
                monitor_data->lightning_distance > 40 ? "Very Far (>40km)" : 
                monitor_data->lightning_distance > 20 ? "Far (20-40km)" : 
                monitor_data->lightning_distance > 10 ? "Moderate (10-20km)" : 
                monitor_data->lightning_distance > 5 ? "Close (5-10km)" : 
                "Very Close (<5km)",
                (unsigned long)monitor_data->lightning_energy,
                monitor_data->lightning_energy > 1000 ? "Very Strong (>1000)" : 
                monitor_data->lightning_energy > 500 ? "Strong (500-1000)" : 
                monitor_data->lightning_energy > 200 ? "Moderate (200-500)" : "Weak (<200)",
                r0, r1, r3, r8, (unsigned int)esp_log_timestamp());
            break;
            
        case AS3935_INT_DISTURBER:
            event_type = "disturber";
            event_description = "Disturber Detected (non-lightning noise)";
            ESP_LOGI(TAG, "[EVENT] Disturber detected");
            snprintf(payload, sizeof(payload),
                "{\"event\":\"%s\",\"description\":\"%s\","
                "\"r0\":\"0x%02x\",\"r1\":\"0x%02x\",\"r3\":\"0x%02x\",\"r8\":\"0x%02x\","
                "\"timestamp\":%u}",
                event_type, event_description, r0, r1, r3, r8, (unsigned int)esp_log_timestamp());
            break;
            
        case AS3935_INT_NOISE:
            event_type = "noise";
            event_description = "Noise Level Too High";
            ESP_LOGI(TAG, "[EVENT] Noise level too high");
            snprintf(payload, sizeof(payload),
                "{\"event\":\"%s\",\"description\":\"%s\","
                "\"r0\":\"0x%02x\",\"r1\":\"0x%02x\",\"r3\":\"0x%02x\",\"r8\":\"0x%02x\","
                "\"timestamp\":%u}",
                event_type, event_description, r0, r1, r3, r8, (unsigned int)esp_log_timestamp());
            break;
            
        default:
            event_description = "Unknown event type";
            ESP_LOGI(TAG, "[EVENT] Unknown event type: %d", (int)event_id);
            snprintf(payload, sizeof(payload),
                "{\"event\":\"unknown\",\"description\":\"%s\",\"event_id\":%d,"
                "\"r0\":\"0x%02x\",\"r1\":\"0x%02x\",\"r3\":\"0x%02x\",\"r8\":\"0x%02x\","
                "\"timestamp\":%u}",
                event_description, (int)event_id, r0, r1, r3, r8, (unsigned int)esp_log_timestamp());
            break;
    }
    
    // Load MQTT topic from NVS
    char topic[256] = "as3935/lightning";  // Default topic
    settings_load_str("mqtt", "topic", topic, sizeof(topic));
    
    // Publish to MQTT if connected
    if (mqtt_is_connected()) {
        ESP_LOGI(TAG, "[EVENT] Publishing to MQTT topic '%s': %s", topic, payload);
        esp_err_t err = mqtt_publish(topic, payload);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "[EVENT] MQTT publish failed: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGD(TAG, "[EVENT] MQTT not connected, skipping publish");
    }
    
    // Broadcast via SSE for web UI
    events_broadcast(event_type, payload);
    ESP_LOGI(TAG, "[EVENT] SSE broadcast sent");
    
    // Call legacy callback if registered
    if (g_event_callback) {
        ESP_LOGD(TAG, "[EVENT] Calling legacy event callback");
        // Only call for lightning events with valid data
        if (event_id == AS3935_INT_LIGHTNING) {
            g_event_callback(monitor_data->lightning_distance, monitor_data->lightning_energy, esp_log_timestamp());
        }
    }
}

/**
 * @brief Non-blocking I2C register read for HTTP handlers
 * 
 * This function reads a single byte from an AS3935 register WITHOUT vTaskDelay,
 * making it safe to call from HTTP handler context. Unlike the library functions,
 * this does NOT retry on NACK.
 * 
 * @param reg_addr Register address to read
 * @param value Pointer to store the read byte
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t as3935_i2c_read_byte_nb(uint8_t reg_addr, uint8_t *value) {
    if (!g_i2c_device) {
        ESP_LOGE(TAG, "[I2C-NB] ERROR: I2C device not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!value) {
        ESP_LOGE(TAG, "[I2C-NB] ERROR: value pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!g_i2c_mutex) {
        ESP_LOGE(TAG, "[I2C-NB] ERROR: I2C mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Acquire mutex with 5 second timeout to prevent concurrent I2C access
    // Using long timeout to ensure mutex is available even under load
    if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "[I2C-NB] ERROR: Failed to acquire I2C mutex (5s timeout)");
        return ESP_ERR_TIMEOUT;
    }
    
    uint8_t tx_buf = reg_addr;
    uint8_t rx_buf = 0;
    
    // Single I2C transaction - no vTaskDelay, no retry, uses persistent device handle
    esp_err_t ret = i2c_master_transmit_receive(g_i2c_device, &tx_buf, 1, &rx_buf, 1, 500);
    
    if (ret == ESP_OK) {
        *value = rx_buf;
    }
    
    // Release mutex
    xSemaphoreGive(g_i2c_mutex);
    
    // Log errors only
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[I2C-NB] FAILED: reg=0x%02x error=%s", reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Non-blocking I2C register write for HTTP handlers
 * 
 * This function writes a single byte to an AS3935 register WITHOUT vTaskDelay,
 * making it safe to call from HTTP handler context.
 * 
 * @param reg_addr Register address to write
 * @param value Byte value to write
 * @return ESP_OK on success, error code otherwise
 */
static esp_err_t as3935_i2c_write_byte_nb(uint8_t reg_addr, uint8_t value) {
    if (!g_i2c_device) {
        ESP_LOGE(TAG, "[I2C-NB-WRITE] ERROR: I2C device not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!g_i2c_mutex) {
        ESP_LOGE(TAG, "[I2C-NB-WRITE] ERROR: I2C mutex not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Acquire mutex with 5 second timeout to prevent concurrent I2C access
    if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "[I2C-NB-WRITE] ERROR: Failed to acquire I2C mutex (5s timeout)");
        return ESP_ERR_TIMEOUT;
    }
    
    uint8_t tx_buf[2] = {reg_addr, value};
    
    // Single I2C transaction - no vTaskDelay, no retry, uses persistent device handle
    esp_err_t ret = i2c_master_transmit(g_i2c_device, tx_buf, 2, 500);
    
    // Release mutex
    xSemaphoreGive(g_i2c_mutex);
    
    // Log errors only
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "[I2C-NB-WRITE] FAILED: reg=0x%02x error=%s", reg_addr, esp_err_to_name(ret));
    }
    
    return ret;
}

/**
 * @brief Initialize AS3935 adapter with I2C bus configuration
 * Note: The sensor library will be initialized separately via the library's as3935_init function
 */
bool as3935_adapter_bus_init(const as3935_adapter_config_t *cfg) {
    if (!cfg) {
        ESP_LOGE(TAG, "Invalid config");
        return false;
    }
    
    memcpy(&g_config, cfg, sizeof(as3935_adapter_config_t));
    esp_err_t ret;
    
    // Create I2C master bus if not already created
    if (!g_i2c_bus) {
        i2c_master_bus_config_t i2c_mst_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = cfg->i2c_port,
            .scl_io_num = cfg->scl_pin,
            .sda_io_num = cfg->sda_pin,
            .glitch_ignore_cnt = 7,
        };
        
        ret = i2c_new_master_bus(&i2c_mst_config, &g_i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2C bus creation failed: %s", esp_err_to_name(ret));
            return false;
        }
        ESP_LOGI(TAG, "I2C bus created on port %d, sda=%d, scl=%d", cfg->i2c_port, cfg->sda_pin, cfg->scl_pin);
    }
    
    // Create persistent I2C device handle for non-blocking reads
    if (!g_i2c_device && g_i2c_bus) {
        ESP_LOGI(TAG, "[INIT] Creating persistent I2C device handle for addr=0x%02x", cfg->i2c_addr);
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = cfg->i2c_addr,
            .scl_speed_hz = 100000,
        };
        
        ret = i2c_master_bus_add_device(g_i2c_bus, &dev_cfg, &g_i2c_device);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[INIT] FAILED to create persistent I2C device handle: %s", esp_err_to_name(ret));
            // Continue anyway, non-blocking reads will fail gracefully
        } else {
            ESP_LOGI(TAG, "[INIT] SUCCESS: Persistent I2C device handle created, handle=%p", g_i2c_device);
        }
    } else {
        ESP_LOGI(TAG, "[INIT] Device handle already exists or bus not ready: device=%p, bus=%p", g_i2c_device, g_i2c_bus);
    }
    
    // Create I2C mutex for thread-safe access if not already created
    if (!g_i2c_mutex) {
        g_i2c_mutex = xSemaphoreCreateMutex();
        if (!g_i2c_mutex) {
            ESP_LOGE(TAG, "[INIT] FAILED to create I2C mutex");
            return false;
        }
        ESP_LOGI(TAG, "[INIT] I2C mutex created for thread-safe I2C operations");
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "AS3935 adapter initialized: i2c_addr=0x%02x, irq_pin=%d",
             cfg->i2c_addr, cfg->irq_pin);
    
    return true;
}

/**
 * @brief Initialize the AS3935 sensor device library handle with event monitoring
 * Must be called after as3935_adapter_bus_init to set up the sensor on the I2C bus
 * This uses as3935_monitor_init which creates the interrupt monitoring task and event loop
 */
esp_err_t as3935_init_sensor_handle(int i2c_addr, int irq_pin) {
    if (!g_i2c_bus) {
        ESP_LOGE(TAG, "I2C bus not initialized. Call as3935_adapter_bus_init first.");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_monitor_handle != NULL) {
        ESP_LOGW(TAG, "Monitor already initialized");
        return ESP_OK;
    }
    
    // Create library config using the LIBRARY's config structure
    // Note: This is different from our adapter's as3935_adapter_config_t!
    // Library config structure has: i2c_address, i2c_clock_speed, irq_io_enabled, irq_io_num,
    //                               analog_frontend, min_lightning_strikes, calibrate_rco,
    //                               disturber_detection_enabled, noise_level_threshold
    as3935_config_t lib_config = {
        .i2c_address = i2c_addr,
        .i2c_clock_speed = 100000,          // 100 kHz I2C clock
        .irq_io_enabled = true,
        .irq_io_num = (irq_pin >= 0) ? irq_pin : 10,
        .analog_frontend = 0,               // Indoor/outdoor (default: 0 for indoor)
        .min_lightning_strikes = 1,         // Minimum lightning strikes before event
        .calibrate_rco = true,
        .disturber_detection_enabled = true,
        .noise_level_threshold = 2,         // Noise level threshold
    };
    
    // Use as3935_monitor_init instead of as3935_init to enable event monitoring
    // This creates the interrupt monitoring task and event loop that processes GPIO interrupts
    ESP_LOGI(TAG, "Initializing AS3935 monitor with event loop support...");
    esp_err_t ret = as3935_monitor_init(g_i2c_bus, &lib_config, &g_monitor_handle);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AS3935 monitor init failed: %s", esp_err_to_name(ret));
        g_monitor_handle = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "AS3935 monitor initialized successfully");
    
    // Extract the sensor handle from the monitor context for compatibility with existing code
    // The monitor contains the sensor handle internally - we need it for direct sensor access
    as3935_monitor_context_t *monitor_ctx = (as3935_monitor_context_t *)g_monitor_handle;
    g_sensor_handle = monitor_ctx->as3935_handle;
    ESP_LOGI(TAG, "Extracted sensor handle from monitor context: %p", g_sensor_handle);
    
    // Register the event handler to receive lightning, disturber, and noise events
    ESP_LOGI(TAG, "Registering AS3935 event handler...");
    ret = as3935_monitor_add_handler(g_monitor_handle, as3935_event_handler, NULL);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register event handler: %s", esp_err_to_name(ret));
        as3935_monitor_deinit(g_monitor_handle);
        g_monitor_handle = NULL;
        return ret;
    }
    
    ESP_LOGI(TAG, "AS3935 event handler registered - system ready for lightning detection");
    ESP_LOGI(TAG, "AS3935 sensor monitoring active: i2c_addr=0x%02x, irq_pin=%d", i2c_addr, irq_pin);
    
    // Load and apply saved advanced settings from NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    esp_err_t nvs_err = as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                                           &min_strikes, &disturber_enabled, &watchdog);
    
    if (nvs_err == ESP_OK) {
        ESP_LOGI(TAG, "Applying saved advanced settings: AFE=%d, Noise=%d, Spike=%d, MinStrikes=%d, Disturber=%s, Watchdog=%d",
                 afe, noise_level, spike_rejection, min_strikes, 
                 disturber_enabled ? "ON" : "OFF", watchdog);
        
        esp_err_t apply_err = as3935_apply_advanced_settings(afe, noise_level, spike_rejection, 
                                                             min_strikes, disturber_enabled, watchdog);
        if (apply_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to apply some advanced settings: %s", esp_err_to_name(apply_err));
        } else {
            ESP_LOGI(TAG, "Advanced settings applied successfully from NVS");
        }
    } else {
        ESP_LOGI(TAG, "No saved advanced settings found, sensor using defaults from library init");
    }
    
    return ESP_OK;
}

/**
 * @brief Configure with default settings
 */
bool as3935_configure_default(void) {
    as3935_adapter_config_t cfg = {
        .i2c_port = 0,
        .sda_pin = 21,
        .scl_pin = 22,
        .irq_pin = 23,
        .i2c_addr = 0x03
    };
    return as3935_adapter_bus_init(&cfg);
}

/**
 * @brief Set event callback for lightning detection
 */
void as3935_set_event_callback(as3935_event_cb_t cb) {
    g_event_callback = cb;
    ESP_LOGI(TAG, "Event callback set");
}

/**
 * @brief Setup IRQ GPIO monitoring
 */
bool as3935_setup_irq(int irq_pin) {
    if (irq_pin < 0) {
        ESP_LOGE(TAG, "Invalid IRQ pin");
        return false;
    }
    g_config.irq_pin = irq_pin;
    ESP_LOGI(TAG, "IRQ pin configured: %d", irq_pin);
    return true;
}

/**
 * @brief Apply register configuration from JSON string
 */
bool as3935_apply_config_json(const char *json) {
    if (!json) {
        return false;
    }
    
    // Simplified: just log that config was received
    // Full JSON parsing would require cJSON integration
    ESP_LOGI(TAG, "Config JSON received: %s", json);
    return true;
}

/**
 * @brief Save configuration to NVS
 */
esp_err_t as3935_save_config_nvs(const char *json) {
    if (!json) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open("as3935", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_str(handle, "config", json);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

/**
 * @brief Load configuration from NVS
 */
esp_err_t as3935_load_config_nvs(char *out, size_t len) {
    if (!out || len < 10) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open("as3935", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_get_str(handle, "config", out, &len);
    nvs_close(handle);
    return err;
}

/**
 * @brief Initialize from NVS stored pins
 */
bool as3935_init_from_nvs(void) {
    int i2c_port, sda, scl, irq;
    
    esp_err_t err = as3935_load_pins_nvs(&i2c_port, &sda, &scl, &irq);
    if (err == ESP_OK) {
        as3935_adapter_config_t cfg = {
            .i2c_port = i2c_port,
            .sda_pin = sda,
            .scl_pin = scl,
            .irq_pin = irq,
            .i2c_addr = 0x03
        };
        
        // Try to load address if saved
        int addr;
        if (as3935_load_addr_nvs(&addr) == ESP_OK) {
            cfg.i2c_addr = addr;
        }
        
        // Initialize I2C bus via adapter
        if (!as3935_adapter_bus_init(&cfg)) {
            return false;
        }
        
        // Initialize sensor device handle via library
        if (as3935_init_sensor_handle(cfg.i2c_addr, cfg.irq_pin) != ESP_OK) {
            ESP_LOGW(TAG, "Sensor device initialization failed, will retry later");
            // Don't fail - user can still configure via web interface
        }
        
        return true;
    }
    
    return false;
}

/**
 * @brief Save pin configuration to NVS
 */
esp_err_t as3935_save_pins_nvs(int i2c_port, int sda, int scl, int irq) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("as3935_pins", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    int32_t i2c_port_val = i2c_port;
    int32_t sda_val = sda;
    int32_t scl_val = scl;
    int32_t irq_val = irq;
    
    err = nvs_set_i32(handle, "i2c_port", i2c_port_val);
    if (err == ESP_OK) err = nvs_set_i32(handle, "sda_pin", sda_val);
    if (err == ESP_OK) err = nvs_set_i32(handle, "scl_pin", scl_val);
    if (err == ESP_OK) err = nvs_set_i32(handle, "irq_pin", irq_val);
    if (err == ESP_OK) err = nvs_commit(handle);
    
    nvs_close(handle);
    return err;
}

/**
 * @brief Load pin configuration from NVS
 */
esp_err_t as3935_load_pins_nvs(int *i2c_port, int *sda, int *scl, int *irq) {
    if (!i2c_port || !sda || !scl || !irq) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open("as3935_pins", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    int32_t i2c_port_val = 0;
    int32_t sda_val = 0;
    int32_t scl_val = 0;
    int32_t irq_val = 0;
    
    err = nvs_get_i32(handle, "i2c_port", &i2c_port_val);
    if (err == ESP_OK) err = nvs_get_i32(handle, "sda_pin", &sda_val);
    if (err == ESP_OK) err = nvs_get_i32(handle, "scl_pin", &scl_val);
    if (err == ESP_OK) err = nvs_get_i32(handle, "irq_pin", &irq_val);
    
    if (err == ESP_OK) {
        *i2c_port = (int)i2c_port_val;
        *sda = (int)sda_val;
        *scl = (int)scl_val;
        *irq = (int)irq_val;
    }
    
    nvs_close(handle);
    return err;
}

/**
 * @brief Save I2C address to NVS
 */
esp_err_t as3935_save_addr_nvs(int i2c_addr) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("as3935_addr", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_i32(handle, "i2c_addr", (int32_t)i2c_addr);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

/**
 * @brief Load I2C address from NVS
 */
esp_err_t as3935_load_addr_nvs(int *i2c_addr) {
    if (!i2c_addr) {
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open("as3935_addr", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }
    
    int32_t addr_val = 0;
    err = nvs_get_i32(handle, "i2c_addr", &addr_val);
    
    if (err == ESP_OK) {
        *i2c_addr = (int)addr_val;
    }
    
    nvs_close(handle);
    return err;
}

/**
 * @brief Advanced Settings NVS Storage
 * These functions save/load the advanced sensor settings to/from NVS
 * so they persist across reboots and the sensor starts with consistent configuration
 */

#define NVS_NAMESPACE_AS3935_CFG "as3935_cfg"

/**
 * @brief Save advanced settings to NVS
 */
esp_err_t as3935_save_advanced_settings_nvs(
    int afe, int noise_level, int spike_rejection, 
    int min_strikes, bool disturber_enabled, int watchdog) {
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_AS3935_CFG, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for advanced settings: %s", esp_err_to_name(err));
        return err;
    }
    
    err = nvs_set_i32(handle, "afe", (int32_t)afe);
    if (err == ESP_OK) err = nvs_set_i32(handle, "noise_lvl", (int32_t)noise_level);
    if (err == ESP_OK) err = nvs_set_i32(handle, "spike_rej", (int32_t)spike_rejection);
    if (err == ESP_OK) err = nvs_set_i32(handle, "min_strikes", (int32_t)min_strikes);
    if (err == ESP_OK) err = nvs_set_u8(handle, "disturber", disturber_enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i32(handle, "watchdog", (int32_t)watchdog);
    
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    
    nvs_close(handle);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Advanced settings saved to NVS");
    } else {
        ESP_LOGE(TAG, "Failed to save advanced settings: %s", esp_err_to_name(err));
    }
    
    return err;
}

/**
 * @brief Load advanced settings from NVS with defaults
 */
esp_err_t as3935_load_advanced_settings_nvs(
    int *afe, int *noise_level, int *spike_rejection,
    int *min_strikes, bool *disturber_enabled, int *watchdog) {
    
    // Set defaults first
    if (afe) *afe = 18;                         // Indoor (AFE = 0b10010 = 18)
    if (noise_level) *noise_level = 2;          // Medium noise level
    if (spike_rejection) *spike_rejection = 2;  // Low rejection
    if (min_strikes) *min_strikes = 0;          // 1 strike (value 0)
    if (disturber_enabled) *disturber_enabled = true;  // Disturber detection ON
    if (watchdog) *watchdog = 2;                // Medium watchdog threshold
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE_AS3935_CFG, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No saved advanced settings found, using defaults");
        return ESP_ERR_NVS_NOT_FOUND;  // Not an error, just no saved settings
    }
    
    int32_t val;
    uint8_t u8_val;
    
    if (afe && nvs_get_i32(handle, "afe", &val) == ESP_OK) {
        *afe = (int)val;
        // Validate AFE: must be 18 (INDOOR) or 14 (OUTDOOR)
        if (*afe != 18 && *afe != 14) {
            ESP_LOGW(TAG, "Invalid AFE value %d in NVS, using default 18 (INDOOR)", *afe);
            *afe = 18;
        }
    }
    if (noise_level && nvs_get_i32(handle, "noise_lvl", &val) == ESP_OK) {
        *noise_level = (int)val;
        // Validate noise level: 0-7
        if (*noise_level < 0 || *noise_level > 7) {
            ESP_LOGW(TAG, "Invalid noise level %d in NVS, using default 2", *noise_level);
            *noise_level = 2;
        }
    }
    if (spike_rejection && nvs_get_i32(handle, "spike_rej", &val) == ESP_OK) {
        *spike_rejection = (int)val;
        // Validate spike rejection: 0-15
        if (*spike_rejection < 0 || *spike_rejection > 15) {
            ESP_LOGW(TAG, "Invalid spike rejection %d in NVS, using default 2", *spike_rejection);
            *spike_rejection = 2;
        }
    }
    if (min_strikes && nvs_get_i32(handle, "min_strikes", &val) == ESP_OK) {
        *min_strikes = (int)val;
        // Validate min strikes: 0-3
        if (*min_strikes < 0 || *min_strikes > 3) {
            ESP_LOGW(TAG, "Invalid min strikes %d in NVS, using default 0", *min_strikes);
            *min_strikes = 0;
        }
    }
    if (disturber_enabled && nvs_get_u8(handle, "disturber", &u8_val) == ESP_OK) {
        *disturber_enabled = (u8_val != 0);
    }
    if (watchdog && nvs_get_i32(handle, "watchdog", &val) == ESP_OK) {
        *watchdog = (int)val;
        // Validate watchdog: 0-10
        if (*watchdog < 0 || *watchdog > 10) {
            ESP_LOGW(TAG, "Invalid watchdog %d in NVS, using default 2", *watchdog);
            *watchdog = 2;
        }
    }
    
    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded advanced settings from NVS");
    return ESP_OK;
}

/**
 * @brief Apply advanced settings to the sensor
 */
esp_err_t as3935_apply_advanced_settings(
    int afe, int noise_level, int spike_rejection,
    int min_strikes, bool disturber_enabled, int watchdog) {
    
    if (!g_sensor_handle) {
        ESP_LOGW(TAG, "Cannot apply settings - sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t err = ESP_OK;
    
    // Apply AFE (Analog Front End)
    as3935_0x00_register_t reg_0x00;
    err = as3935_get_0x00_register(g_sensor_handle, &reg_0x00);
    if (err == ESP_OK) {
        reg_0x00.bits.analog_frontend = (afe & 0x1F);
        err = as3935_set_0x00_register(g_sensor_handle, reg_0x00);
        ESP_LOGI(TAG, "Applied AFE: %d", afe);
    }
    
    // Apply Noise Level
    as3935_0x01_register_t reg_0x01;
    err = as3935_get_0x01_register(g_sensor_handle, &reg_0x01);
    if (err == ESP_OK) {
        reg_0x01.bits.noise_floor_level = (noise_level & 0x07);
        reg_0x01.bits.watchdog_threshold = (watchdog & 0x0F);
        err = as3935_set_0x01_register(g_sensor_handle, reg_0x01);
        ESP_LOGI(TAG, "Applied Noise:%d, Watchdog:%d", noise_level, watchdog);
    }
    
    // Apply Spike Rejection (Register 0x02)
    as3935_0x02_register_t reg_0x02;
    err = as3935_get_0x02_register(g_sensor_handle, &reg_0x02);
    if (err == ESP_OK) {
        reg_0x02.bits.spike_rejection = (spike_rejection & 0x0F);
        reg_0x02.bits.min_num_lightning = (min_strikes & 0x03);
        err = as3935_set_0x02_register(g_sensor_handle, reg_0x02);
        ESP_LOGI(TAG, "Applied Spike:%d, Min Strikes:%d", spike_rejection, min_strikes);
    }
    
    // Apply Disturber Detection
    as3935_0x03_register_t reg_0x03;
    err = as3935_get_0x03_register(g_sensor_handle, &reg_0x03);
    if (err == ESP_OK) {
        reg_0x03.bits.disturber_detection_state = disturber_enabled ? 
            AS3935_DISTURBER_DETECTION_ENABLED : AS3935_DISTURBER_DETECTION_DISABLED;
        err = as3935_set_0x03_register(g_sensor_handle, reg_0x03);
        ESP_LOGI(TAG, "Applied Disturber Detection: %s", disturber_enabled ? "ENABLED" : "DISABLED");
    }
    
    return err;
}

/**
 * @brief HTTP Handlers - Stub implementations
 */

esp_err_t as3935_save_handler(httpd_req_t *req) {
    // Parse JSON POST body for configuration save
    char buf[256];
    int content_len = req->content_len;
    
    // If no content, just acknowledge
    if (content_len <= 0) {
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"msg\":\"no_changes\"}");
        return http_reply_json(req, buf);
    }
    
    if (content_len > 1024) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"content_too_large\"}");
        return http_reply_json(req, buf);
    }
    
    char *body = malloc(content_len + 1);
    if (!body) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
        free(body);
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"recv_failed\"}");
        return http_reply_json(req, buf);
    }
    
    body[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    free(body);
    
    if (!json) {
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"msg\":\"no_json_body\"}");
        return http_reply_json(req, buf);
    }
    
    // Log received configuration
    ESP_LOGI(TAG, "Configuration save requested");
    
    cJSON_Delete(json);
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"saved\":true}");
    return http_reply_json(req, buf);
}

esp_err_t as3935_status_handler(httpd_req_t *req) {
    char buf[1024];
    bool sensor_ok = g_initialized && (g_i2c_bus != NULL) && (g_sensor_handle != NULL) && (g_monitor_handle != NULL);
    
    const char *sensor_status = sensor_ok ? "connected" : "disconnected";
    
    // Read register values using non-blocking I2C reads
    uint8_t r0 = 0, r1 = 0, r3 = 0, r8 = 0;
    
    // Use non-blocking reads to avoid blocking HTTP handler
    as3935_i2c_read_byte_nb(0x00, &r0);
    as3935_i2c_read_byte_nb(0x01, &r1);
    as3935_i2c_read_byte_nb(0x03, &r3);
    as3935_i2c_read_byte_nb(0x08, &r8);
    
    snprintf(buf, sizeof(buf), 
        "{\"initialized\":%s,"
        "\"sensor_status\":\"%s\","
        "\"sensor_handle_valid\":%s,"
        "\"i2c_port\":%d,"
        "\"sda\":%d,"
        "\"scl\":%d,"
        "\"irq\":%d,"
        "\"addr\":\"0x%02x\","
        "\"verification_register\":\"0x%02x\","
        "\"r0\":\"0x%02x\","
        "\"r1\":\"0x%02x\","
        "\"r3\":\"0x%02x\","
        "\"r8\":\"0x%02x\"}",
        g_initialized ? "true" : "false",
        sensor_status,
        (g_sensor_handle != NULL) ? "true" : "false",
        g_config.i2c_port, g_config.sda_pin, g_config.scl_pin, 
        g_config.irq_pin, g_config.i2c_addr, r0, r0, r1, r3, r8);
    return http_reply_json(req, buf);
}

esp_err_t as3935_pins_save_handler(httpd_req_t *req) {
    // Parse JSON POST body: {"i2c_port":0,"sda":21,"scl":22,"irq":0}
    char buf[512];
    int content_len = req->content_len;
    
    if (content_len <= 0 || content_len > 1024) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_content_length\"}");
        return http_reply_json(req, buf);
    }
    
    char *body = malloc(content_len + 1);
    if (!body) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
        free(body);
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"recv_failed\"}");
        return http_reply_json(req, buf);
    }
    
    body[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    free(body);
    
    if (!json) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");
        return http_reply_json(req, buf);
    }
    
    // Extract values with defaults
    int i2c_port = g_config.i2c_port;
    int sda = g_config.sda_pin;
    int scl = g_config.scl_pin;
    int irq = g_config.irq_pin;
    
    // Store old values to detect changes
    int old_i2c_port = g_config.i2c_port;
    int old_sda = g_config.sda_pin;
    int old_scl = g_config.scl_pin;
    
    const cJSON *item = NULL;
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "i2c_port")) && item->type == cJSON_Number) {
        i2c_port = item->valueint;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "sda")) && item->type == cJSON_Number) {
        sda = item->valueint;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "scl")) && item->type == cJSON_Number) {
        scl = item->valueint;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "irq")) && item->type == cJSON_Number) {
        irq = item->valueint;
    }
    
    cJSON_Delete(json);
    
    // Check if critical pins changed (these require I2C bus recreation)
    bool critical_pins_changed = (i2c_port != old_i2c_port) || 
                                 (sda != old_sda) || 
                                 (scl != old_scl);
    
    // Save to NVS
    esp_err_t err = as3935_save_pins_nvs(i2c_port, sda, scl, irq);
    if (err == ESP_OK) {
        // Update global config
        g_config.i2c_port = i2c_port;
        g_config.sda_pin = sda;
        g_config.scl_pin = scl;
        g_config.irq_pin = irq;
        
        if (critical_pins_changed && g_i2c_bus != NULL) {
            ESP_LOGW(TAG, "I2C pins changed - device restart required to apply changes!");
            snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"saved\":true,\"warning\":\"I2C pins require restart to apply\"}");
        } else {
            snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"saved\":true}");
        }
        
        ESP_LOGI(TAG, "Pins saved to NVS: i2c_port=%d, sda=%d, scl=%d, irq=%d", 
                 i2c_port, sda, scl, irq);
    } else {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"code\":%d}", err);
        ESP_LOGE(TAG, "Failed to save pins: %s", esp_err_to_name(err));
    }
    
    return http_reply_json(req, buf);
}

esp_err_t as3935_pins_status_handler(httpd_req_t *req) {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"i2c_port\":%d,\"sda\":%d,\"scl\":%d,\"irq\":%d}",
        g_config.i2c_port, g_config.sda_pin, g_config.scl_pin, g_config.irq_pin);
    return http_reply_json(req, buf);
}

esp_err_t as3935_addr_save_handler(httpd_req_t *req) {
    // Parse JSON POST body: {"i2c_addr":"0x03"} or {"i2c_addr":3}
    char buf[256];
    int content_len = req->content_len;
    
    if (content_len <= 0 || content_len > 1024) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_content_length\"}");
        return http_reply_json(req, buf);
    }
    
    char *body = malloc(content_len + 1);
    if (!body) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
        free(body);
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"recv_failed\"}");
        return http_reply_json(req, buf);
    }
    
    body[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    free(body);
    
    if (!json) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int i2c_addr = -1;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "i2c_addr");
    
    if (item) {
        if (item->type == cJSON_Number) {
            // Parse as decimal number
            i2c_addr = item->valueint;
        } else if (item->type == cJSON_String && item->valuestring) {
            // Parse as hex string like "0x03"
            i2c_addr = (int)strtol(item->valuestring, NULL, 16);
        }
    }
    
    cJSON_Delete(json);
    
    if (i2c_addr < 0 || i2c_addr > 255) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_address\",\"received\":%d}", i2c_addr);
        ESP_LOGE(TAG, "Invalid I2C address: %d", i2c_addr);
        return http_reply_json(req, buf);
    }
    
    // Save to NVS
    esp_err_t err = as3935_save_addr_nvs(i2c_addr);
    if (err == ESP_OK) {
        g_config.i2c_addr = i2c_addr;
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"saved\":true,\"i2c_addr\":\"0x%02x\"}", i2c_addr);
        ESP_LOGI(TAG, "I2C address saved to NVS: 0x%02x", i2c_addr);
    } else {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"code\":%d}", err);
        ESP_LOGE(TAG, "Failed to save address: %s", esp_err_to_name(err));
    }
    
    return http_reply_json(req, buf);
}

esp_err_t as3935_addr_status_handler(httpd_req_t *req) {
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"i2c_addr\":\"0x%02x\"}", g_config.i2c_addr);
    return http_reply_json(req, buf);
}

esp_err_t as3935_params_handler(httpd_req_t *req) {
    // Use non-blocking I2C reads instead of blocking library functions
    uint8_t reg0 = 0, reg1 = 0, reg2 = 0;
    
    char buf[1024];
    
    // Read the registers using non-blocking I2C
    as3935_i2c_read_byte_nb(0x00, &reg0);
    as3935_i2c_read_byte_nb(0x01, &reg1);
    as3935_i2c_read_byte_nb(0x02, &reg2);
    
    // Get lightning distance and energy (these are safe)
    uint32_t energy = 0;
    uint8_t distance_km = 0;
    if (g_sensor_handle) {
        as3935_get_lightning_energy(g_sensor_handle, &energy);
        as3935_get_lightning_distance_km(g_sensor_handle, &distance_km);
    }
    
    // Extract bit fields from raw register values
    // Reg 0x00 bits: power_state=0, analog_frontend=5:1
    // Reg 0x01 bits: watchdog_threshold=3:0, noise_floor_level=6:4
    // Reg 0x02 bits: spike_rejection=3:0, min_num_lightning=5:4
    
    // Manually build JSON response with sensor parameters
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\","
        "\"params\":{"
            "\"power_state\":%d,"
            "\"afe_mode\":%d,"
            "\"watchdog_threshold\":%d,"
            "\"noise_floor\":%d,"
            "\"spike_rejection\":%d,"
            "\"min_lightning_strikes\":%d"
        "},"
        "\"sensor_readings\":{"
            "\"lightning_energy\":%lu,"
            "\"lightning_distance_km\":%d"
        "}"
        "}",
        (reg0 >> 0) & 0x01,           // power_state
        (reg0 >> 1) & 0x1F,           // afe_mode (bits 5:1)
        (reg1 >> 0) & 0x0F,           // watchdog_threshold (bits 3:0)
        (reg1 >> 4) & 0x07,           // noise_floor_level (bits 6:4)
        (reg2 >> 0) & 0x0F,           // spike_rejection (bits 3:0)
        (reg2 >> 4) & 0x03,           // min_num_lightning (bits 5:4)
        (unsigned long)energy,
        distance_km
    );
    
    return http_reply_json(req, buf);
}

esp_err_t as3935_calibrate_start_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Calibration started");
    return http_reply_json(req, "{\"status\":\"calibration_started\"}");
}

esp_err_t as3935_calibrate_status_handler(httpd_req_t *req) {
    return http_reply_json(req, "{\"status\":\"idle\",\"progress\":0}");
}

esp_err_t as3935_calibrate_cancel_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Calibration cancelled");
    return http_reply_json(req, "{\"status\":\"cancelled\"}");
}

esp_err_t as3935_calibrate_apply_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Calibration applied");
    return http_reply_json(req, "{\"status\":\"applied\"}");
}

esp_err_t as3935_register_read_handler(httpd_req_t *req) {
    // Parse JSON POST body: {"reg":0}
    char buf[256];
    int content_len = req->content_len;
    
    if (content_len <= 0 || content_len > 256) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_content_length\"}");
        return http_reply_json(req, buf);
    }
    
    char *body = malloc(content_len + 1);
    if (!body) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
        free(body);
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"recv_failed\"}");
        return http_reply_json(req, buf);
    }
    
    body[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    free(body);
    
    if (!json) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int reg = -1;
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "reg");
    
    if (item && item->type == cJSON_Number) {
        reg = item->valueint;
    }
    
    cJSON_Delete(json);
    
    if (reg < 0 || reg > 255) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_reg\",\"reg\":%d}", reg);
        return http_reply_json(req, buf);
    }
    
    // Use non-blocking I2C read instead of library functions
    uint8_t value = 0;
    esp_err_t ret_esp = as3935_i2c_read_byte_nb(reg, &value);
    
    if (ret_esp != ESP_OK) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"read_failed\",\"reg\":\"0x%02x\"}", reg);
        return http_reply_json(req, buf);
    }
    
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"reg\":\"0x%02x\",\"value\":%d}", reg, value);
    return http_reply_json(req, buf);
}

esp_err_t as3935_register_write_handler(httpd_req_t *req) {
    // Parse JSON POST body: {"reg":0,"value":128}
    char buf[256];
    int content_len = req->content_len;
    
    if (content_len <= 0 || content_len > 1024) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_content_length\"}");
        return http_reply_json(req, buf);
    }
    
    char *body = malloc(content_len + 1);
    if (!body) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
        free(body);
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"recv_failed\"}");
        return http_reply_json(req, buf);
    }
    
    body[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(body);
    free(body);
    
    if (!json) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");
        return http_reply_json(req, buf);
    }
    
    int reg = -1, value = -1;
    const cJSON *item = NULL;
    
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "reg")) && item->type == cJSON_Number) {
        reg = item->valueint;
    }
    if ((item = cJSON_GetObjectItemCaseSensitive(json, "value")) && item->type == cJSON_Number) {
        value = item->valueint;
    }
    
    cJSON_Delete(json);
    
    if (reg < 0 || reg > 255 || value < 0 || value > 255) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"invalid_params\",\"reg\":%d,\"value\":%d}", reg, value);
        return http_reply_json(req, buf);
    }
    
    if (!g_sensor_handle) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"sensor_not_initialized\",\"reg\":\"0x%02x\"}", reg);
        return http_reply_json(req, buf);
    }
    
    // Write to the requested register using non-blocking I2C (no library functions)
    esp_err_t ret_esp = ESP_OK;
    
    // All writable registers can be written directly via I2C
    switch (reg) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x08:
            // Write directly via I2C helper
            ret_esp = as3935_i2c_write_byte_nb((uint8_t)reg, (uint8_t)value);
            break;
        default:
            snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"register_not_supported\",\"reg\":\"0x%02x\"}", reg);
            return http_reply_json(req, buf);
    }
    
    if (ret_esp != ESP_OK) {
        snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"msg\":\"write_failed\",\"reg\":\"0x%02x\",\"error\":\"%s\"}", reg, esp_err_to_name(ret_esp));
        return http_reply_json(req, buf);
    }
    
    ESP_LOGI(TAG, "Register write success: reg=0x%02x, value=0x%02x", reg, value);
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"reg\":\"0x%02x\",\"value\":%d}", reg, value);
    return http_reply_json(req, buf);
}

esp_err_t as3935_registers_all_handler(httpd_req_t *req) {
    // Use non-blocking I2C reads instead of blocking library functions
    uint8_t reg0 = 0, reg1 = 0, reg2 = 0, reg3 = 0, reg8 = 0;
    
    // Read all registers using non-blocking I2C
    as3935_i2c_read_byte_nb(0x00, &reg0);
    as3935_i2c_read_byte_nb(0x01, &reg1);
    as3935_i2c_read_byte_nb(0x02, &reg2);
    as3935_i2c_read_byte_nb(0x03, &reg3);
    as3935_i2c_read_byte_nb(0x08, &reg8);
    
    // Get lightning readings (these are safe as they likely use internal state, not I2C)
    uint32_t energy = 0;
    uint8_t distance_km = 0;
    if (g_sensor_handle) {
        as3935_get_lightning_energy(g_sensor_handle, &energy);
        as3935_get_lightning_distance_km(g_sensor_handle, &distance_km);
    }
    
    // Build JSON response
    char response[1024];
    snprintf(response, sizeof(response),
        "{\"status\":\"ok\","
        "\"registers\":{"
            "\"0x00\":%d,"
            "\"0x01\":%d,"
            "\"0x02\":%d,"
            "\"0x03\":%d,"
            "\"0x08\":%d"
        "},"
        "\"sensor_data\":{"
            "\"lightning_energy\":%lu,"
            "\"lightning_distance_km\":%d"
        "}"
        "}",
        reg0, reg1, reg2, reg3, reg8,
        (unsigned long)energy, distance_km);
    
    return http_reply_json(req, response);
}

esp_err_t as3935_post_handler(httpd_req_t *req) {
    // Enhanced POST handler - returns detailed sensor status and configuration
    ESP_LOGI(TAG, "POST request received on /api/as3935/post");
    
    char buf[1024];
    
    // Read current register values
    uint8_t r0 = 0, r1 = 0, r3 = 0, r8 = 0;
    as3935_i2c_read_byte_nb(0x00, &r0);
    as3935_i2c_read_byte_nb(0x01, &r1);
    as3935_i2c_read_byte_nb(0x03, &r3);
    as3935_i2c_read_byte_nb(0x08, &r8);
    
    // Load advanced settings from NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                       &min_strikes, &disturber_enabled, &watchdog);
    
    // Extract detailed information from registers
    const char *afe_name = (afe == 18) ? "Indoor (Sensitive)" : "Outdoor (Less Sensitive)";
    const char *noise_names[] = {"390µV", "630µV", "860µV", "1100µV", "1140µV", "1570µV", "1800µV", "2000µV"};
    const char *min_strike_names[] = {"1 Strike", "5 Strikes", "9 Strikes", "16 Strikes"};
    
    snprintf(buf, sizeof(buf),
        "{\"status\":\"ok\","
        "\"sensor_status\":{"
        "\"initialized\":%s,"
        "\"i2c_address\":\"0x%02x\","
        "\"registers\":{\"r0\":\"0x%02x\",\"r1\":\"0x%02x\",\"r3\":\"0x%02x\",\"r8\":\"0x%02x\"}"
        "},"
        "\"configuration\":{"
        "\"afe\":%d,\"afe_description\":\"%s\","
        "\"noise_level\":%d,\"noise_level_description\":\"%s\","
        "\"spike_rejection\":%d,\"spike_rejection_max\":15,"
        "\"min_strikes\":%d,\"min_strikes_description\":\"%s\","
        "\"disturber_enabled\":%s,"
        "\"watchdog\":%d,\"watchdog_max\":10"
        "},"
        "\"timestamp\":%u}",
        g_sensor_handle ? "true" : "false",
        g_config.i2c_addr,
        r0, r1, r3, r8,
        afe, afe_name,
        noise_level, (noise_level < 8) ? noise_names[noise_level] : "Invalid",
        spike_rejection,
        min_strikes, (min_strikes < 4) ? min_strike_names[min_strikes] : "Invalid",
        disturber_enabled ? "true" : "false",
        watchdog,
        (unsigned int)esp_log_timestamp());
    
    return http_reply_json(req, buf);
}

esp_err_t as3935_reg_read_handler(httpd_req_t *req) {
    // Simple GET handler for register read
    // Full register address parsing would require query parameter support
    char buf[256];
    int reg = 0;  // Default to register 0
    
    ESP_LOGI(TAG, "Register read via GET: reg=0x%02x", reg);
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"reg\":\"0x%02x\",\"value\":0}", reg);
    return http_reply_json(req, buf);
}

/**
 * @brief Sensor data getters - allow old code to access new library sensor data
 */
esp_err_t as3935_get_sensor_energy(uint32_t *energy) {
    if (!energy) return ESP_ERR_INVALID_ARG;
    if (!g_sensor_handle) return ESP_ERR_INVALID_STATE;
    
    // Use direct non-blocking I2C read instead of library function to avoid vTaskDelay
    // Library's as3935_get_lightning_energy() calls get_0x04_register() which blocks
    uint8_t reg_val = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x04, &reg_val);
    if (err != ESP_OK) {
        return err;
    }
    
    // Energy is in the upper nibble of register 0x04
    *energy = (reg_val >> 4) & 0x0F;
    return ESP_OK;
}

esp_err_t as3935_get_sensor_distance(uint8_t *distance_km) {
    if (!distance_km) return ESP_ERR_INVALID_ARG;
    if (!g_sensor_handle) return ESP_ERR_INVALID_STATE;
    
    // Use direct non-blocking I2C read instead of library function to avoid vTaskDelay
    // Library's as3935_get_lightning_distance_km() calls get_0x03_register() which blocks
    uint8_t reg_val = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x03, &reg_val);
    if (err != ESP_OK) {
        return err;
    }
    
    // Distance is in the lower nibble of register 0x03
    *distance_km = reg_val & 0x0F;
    return ESP_OK;
}

/**
 * @brief Advanced Settings Handlers
 */

esp_err_t as3935_afe_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "[AFE] ENTER handler");
    char buf[512];
    
    int content_len = req->content_len;
    ESP_LOGI(TAG, "[AFE] content_len=%d", content_len);
    
    if (content_len <= 0) {
        ESP_LOGI(TAG, "[AFE-GET] Starting");
        // GET: return current AFE setting from NVS (or default)
        int afe, noise_level, spike_rejection, min_strikes, watchdog;
        bool disturber_enabled;
        as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                          &min_strikes, &disturber_enabled, &watchdog);
        
        int afe_value = afe;  // AFE value from NVS
        ESP_LOGI(TAG, "[AFE-GET] AFE from NVS: %d", afe_value);
        snprintf(buf, sizeof(buf), 
            "{\"status\":\"ok\",\"afe\":%d,\"afe_name\":\"%s\"}", 
            afe_value,
            afe_value == 18 ? "INDOOR" : "OUTDOOR");
        ESP_LOGI(TAG, "[AFE-GET] Sending response");
        return http_reply_json(req, buf);
    }

    // POST: set AFE
    if (!g_sensor_handle) {
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"sensor_not_initialized\"}");
    }
    
    char *body = malloc(content_len + 1);
    if (!body) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) { free(body); return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"recv_failed\"}"); }
    body[ret] = '\0';

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");

    const cJSON *afe_item = cJSON_GetObjectItemCaseSensitive(root, "afe");
    if (!cJSON_IsNumber(afe_item)) { cJSON_Delete(root); return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"invalid_afe\"}"); }

    int afe_val = afe_item->valueint;
    if (afe_val != 18 && afe_val != 14) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"afe_must_be_18_or_14\"}");
    }

    // Set AFE via direct I2C write (non-blocking, no library function)
    // AFE value goes into bits 5:1 of register 0x00
    // First read current register value to preserve other bits
    uint8_t reg0_current = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x00, &reg0_current);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_register_failed\"}");
    }
    
    // Clear bits 5:1, then set new AFE value
    uint8_t reg0_new = (reg0_current & 0xC1) | ((afe_val & 0x1F) << 1);
    err = as3935_i2c_write_byte_nb(0x00, reg0_new);
    cJSON_Delete(root);
    
    if (err != ESP_OK) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"set_failed\"}");
    
    // Save to NVS - load current settings, update AFE, save all
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                      &min_strikes, &disturber_enabled, &watchdog);
    afe = afe_val;  // Update AFE
    as3935_save_advanced_settings_nvs(afe, noise_level, spike_rejection, 
                                      min_strikes, disturber_enabled, watchdog);
    
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"afe\":%d,\"afe_name\":\"%s\"}", 
        afe_val, afe_val == 18 ? "INDOOR" : "OUTDOOR");
    return http_reply_json(req, buf);
}

esp_err_t as3935_noise_level_handler(httpd_req_t *req) {
    char buf[512];
    int content_len = req->content_len;
    
    if (content_len <= 0) {
        // GET: return current noise level from NVS
        int afe, noise_level, spike_rejection, min_strikes, watchdog;
        bool disturber_enabled;
        as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                          &min_strikes, &disturber_enabled, &watchdog);
        
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"noise_level\":%d}", noise_level);
        return http_reply_json(req, buf);
    }

    // POST: set noise level
    if (!g_sensor_handle) {
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"sensor_not_initialized\"}");
    }
    
    char *body = malloc(content_len + 1);
    if (!body) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) { free(body); return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"recv_failed\"}"); }
    body[ret] = '\0';

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");

    const cJSON *level_item = cJSON_GetObjectItemCaseSensitive(root, "noise_level");
    if (!cJSON_IsNumber(level_item) || level_item->valueint < 0 || level_item->valueint > 7) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"noise_level_must_be_0_to_7\"}");
    }

    // Set noise level via direct I2C write (non-blocking, no library function)
    // Noise level (0-7) goes into bits 6:4 of register 0x01
    // First read current register value to preserve other bits
    uint8_t reg1_current = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x01, &reg1_current);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_register_failed\"}");
    }
    
    // Clear bits 6:4, then set new noise level value
    uint8_t reg1_new = (reg1_current & 0x8F) | ((level_item->valueint & 0x07) << 4);
    err = as3935_i2c_write_byte_nb(0x01, reg1_new);
    int noise_val = level_item->valueint;
    cJSON_Delete(root);
    
    if (err != ESP_OK) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"set_failed\"}");
    
    // Save to NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                      &min_strikes, &disturber_enabled, &watchdog);
    noise_level = noise_val;  // Update noise level
    as3935_save_advanced_settings_nvs(afe, noise_level, spike_rejection, 
                                      min_strikes, disturber_enabled, watchdog);
    
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"noise_level\":%d}", noise_val);
    return http_reply_json(req, buf);
}

esp_err_t as3935_spike_rejection_handler(httpd_req_t *req) {
    char buf[512];
    int content_len = req->content_len;
    
    if (content_len <= 0) {
        // GET: return current spike rejection from NVS
        int afe, noise_level, spike_rejection, min_strikes, watchdog;
        bool disturber_enabled;
        as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                          &min_strikes, &disturber_enabled, &watchdog);
        
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"spike_rejection\":%d}", spike_rejection);
        return http_reply_json(req, buf);
    }

    // POST: set spike rejection
    if (!g_sensor_handle) {
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"sensor_not_initialized\"}");
    }
    
    char *body = malloc(content_len + 1);
    if (!body) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) { free(body); return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"recv_failed\"}"); }
    body[ret] = '\0';

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");

    const cJSON *reject_item = cJSON_GetObjectItemCaseSensitive(root, "spike_rejection");
    if (!cJSON_IsNumber(reject_item) || reject_item->valueint < 0 || reject_item->valueint > 15) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"spike_rejection_must_be_0_to_15\"}");
    }

    // Set spike rejection via direct I2C write (non-blocking, no library function)
    // Spike rejection (0-15) goes into bits 3:0 of register 0x02
    // First read current register value to preserve other bits
    uint8_t reg2_current = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x02, &reg2_current);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_register_failed\"}");
    }
    
    // Clear bits 3:0, then set new spike rejection value
    uint8_t reg2_new = (reg2_current & 0xF0) | (reject_item->valueint & 0x0F);
    err = as3935_i2c_write_byte_nb(0x02, reg2_new);
    int spike_val = reject_item->valueint;
    cJSON_Delete(root);
    
    if (err != ESP_OK) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"set_failed\"}");
    
    // Save to NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                      &min_strikes, &disturber_enabled, &watchdog);
    spike_rejection = spike_val;  // Update spike rejection
    as3935_save_advanced_settings_nvs(afe, noise_level, spike_rejection, 
                                      min_strikes, disturber_enabled, watchdog);
    
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"spike_rejection\":%d}", spike_val);
    return http_reply_json(req, buf);
}

esp_err_t as3935_min_strikes_handler(httpd_req_t *req) {
    char buf[512];
    int content_len = req->content_len;
    
    if (content_len <= 0) {
        // GET: return current min strikes from NVS
        int afe, noise_level, spike_rejection, min_strikes, watchdog;
        bool disturber_enabled;
        as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                          &min_strikes, &disturber_enabled, &watchdog);
        
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"min_strikes\":%d}", min_strikes);
        return http_reply_json(req, buf);
    }

    // POST: set min strikes
    if (!g_sensor_handle) {
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"sensor_not_initialized\"}");
    }
    
    char *body = malloc(content_len + 1);
    if (!body) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) { free(body); return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"recv_failed\"}"); }
    body[ret] = '\0';

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");

    const cJSON *strikes_item = cJSON_GetObjectItemCaseSensitive(root, "min_strikes");
    if (!cJSON_IsNumber(strikes_item) || strikes_item->valueint < 0 || strikes_item->valueint > 3) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"min_strikes_must_be_0_to_3\"}");
    }

    // Set min strikes via direct I2C write (non-blocking, no library function)
    // Min strikes (0-3) goes into bits 5:4 of register 0x02
    // First read current register value to preserve other bits
    uint8_t reg2_current = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x02, &reg2_current);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_register_failed\"}");
    }
    
    // Clear bits 5:4, then set new min strikes value
    uint8_t reg2_new = (reg2_current & 0xCF) | ((strikes_item->valueint & 0x03) << 4);
    err = as3935_i2c_write_byte_nb(0x02, reg2_new);
    int strikes_val = strikes_item->valueint;
    cJSON_Delete(root);
    
    if (err != ESP_OK) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"set_failed\"}");
    
    // Save to NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                      &min_strikes, &disturber_enabled, &watchdog);
    min_strikes = strikes_val;  // Update min strikes
    as3935_save_advanced_settings_nvs(afe, noise_level, spike_rejection, 
                                      min_strikes, disturber_enabled, watchdog);
    
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"min_strikes\":%d}", strikes_val);
    return http_reply_json(req, buf);
}

esp_err_t as3935_disturber_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "[DISTURBER] ENTER handler");
    int content_len = req->content_len;
    ESP_LOGI(TAG, "[DISTURBER] content_len=%d", content_len);
    
    if (content_len <= 0) {
        ESP_LOGI(TAG, "[DISTURBER] GET request detected");
        
        // GET: return current disturber setting from NVS
        int afe, noise_level, spike_rejection, min_strikes, watchdog;
        bool disturber_enabled;
        as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                          &min_strikes, &disturber_enabled, &watchdog);
        
        ESP_LOGI(TAG, "[DISTURBER-GET] Disturber from NVS: %s", disturber_enabled ? "enabled" : "disabled");
        
        char buf[64];
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"disturber_enabled\":%s}", 
                disturber_enabled ? "true" : "false");
        
        ESP_LOGI(TAG, "[DISTURBER-GET] Sending JSON response: %s", buf);
        return http_reply_json(req, buf);
    }

    // POST: set disturber detection
    ESP_LOGI(TAG, "[DISTURBER] POST request detected, content_len=%d", content_len);
    
    // Limit content length for safety
    if (content_len > 1024) {
        ESP_LOGW(TAG, "[DISTURBER-POST] Content too large: %d", content_len);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"content_too_large\"}");
    }
    
    char *body = malloc(content_len + 1);
    if (!body) {
        ESP_LOGE(TAG, "[DISTURBER-POST] Malloc failed for POST body");
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
    }
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) {
        ESP_LOGE(TAG, "[DISTURBER-POST] Failed to receive POST body: %d", ret);
        free(body);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"recv_failed\"}");
    }
    body[ret] = '\0';
    ESP_LOGI(TAG, "[DISTURBER-POST] Received POST body: %s", body);

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) {
        ESP_LOGE(TAG, "[DISTURBER-POST] Failed to parse JSON");
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");
    }

    const cJSON *enabled_item = cJSON_GetObjectItemCaseSensitive(root, "disturber_enabled");
    if (!cJSON_IsBool(enabled_item)) {
        ESP_LOGE(TAG, "[DISTURBER-POST] disturber_enabled is not a boolean");
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"disturber_enabled_must_be_boolean\"}");
    }

    bool enable = cJSON_IsTrue(enabled_item);
    ESP_LOGI(TAG, "[DISTURBER-POST] Setting disturber to: %s", enable ? "enabled" : "disabled");
    
    // Set disturber via direct I2C write (non-blocking, no library function)
    // Disturber is controlled by bit 5 of register 0x03 (0 = enabled, 1 = disabled)
    // First read current register value to preserve other bits
    uint8_t reg3_current = 0;
    esp_err_t set_err = as3935_i2c_read_byte_nb(0x03, &reg3_current);
    if (set_err != ESP_OK) {
        ESP_LOGE(TAG, "[DISTURBER-POST] Failed to read register: %s", esp_err_to_name(set_err));
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_register_failed\"}");
    }
    
    // Set or clear bit 5 based on enable flag (0 = enabled, 1 = disabled)
    uint8_t reg3_new;
    if (enable) {
        reg3_new = reg3_current & 0xDF;  // Clear bit 5 to enable
    } else {
        reg3_new = reg3_current | 0x20;  // Set bit 5 to disable
    }
    
    set_err = as3935_i2c_write_byte_nb(0x03, reg3_new);
    cJSON_Delete(root);
    
    if (set_err != ESP_OK) {
        ESP_LOGE(TAG, "[DISTURBER-POST] Failed to write register: %s", esp_err_to_name(set_err));
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"set_failed\"}");
    }
    
    // Save to NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                      &min_strikes, &disturber_enabled, &watchdog);
    disturber_enabled = enable;  // Update disturber setting
    as3935_save_advanced_settings_nvs(afe, noise_level, spike_rejection, 
                                      min_strikes, disturber_enabled, watchdog);
    
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"disturber_enabled\":%s}", 
            enable ? "true" : "false");
    
    ESP_LOGI(TAG, "[DISTURBER-POST] Sending response: %s", buf);
    return http_reply_json(req, buf);
}

esp_err_t as3935_watchdog_handler(httpd_req_t *req) {
    char buf[512];
    int content_len = req->content_len;
    
    if (content_len <= 0) {
        // GET: return current watchdog threshold from NVS
        int afe, noise_level, spike_rejection, min_strikes, watchdog;
        bool disturber_enabled;
        as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                          &min_strikes, &disturber_enabled, &watchdog);
        
        snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"watchdog\":%d}", watchdog);
        return http_reply_json(req, buf);
    }

    // POST: set watchdog threshold
    if (!g_sensor_handle) {
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"sensor_not_initialized\"}");
    }
    
    char *body = malloc(content_len + 1);
    if (!body) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"malloc_failed\"}");
    
    int ret = httpd_req_recv(req, body, content_len);
    if (ret <= 0) { free(body); return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"recv_failed\"}"); }
    body[ret] = '\0';

    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"json_parse_failed\"}");

    const cJSON *wd_item = cJSON_GetObjectItemCaseSensitive(root, "watchdog");
    if (!cJSON_IsNumber(wd_item) || wd_item->valueint < 0 || wd_item->valueint > 10) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"watchdog_must_be_0_to_10\"}");
    }

    // Set watchdog threshold via direct I2C write (non-blocking, no library function)
    // Watchdog threshold (0-10) goes into bits 3:0 of register 0x01
    // First read current register value to preserve other bits
    uint8_t reg1_current = 0;
    esp_err_t err = as3935_i2c_read_byte_nb(0x01, &reg1_current);
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_register_failed\"}");
    }
    
    // Clear bits 3:0, then set new watchdog threshold value
    uint8_t reg1_new = (reg1_current & 0xF0) | (wd_item->valueint & 0x0F);
    err = as3935_i2c_write_byte_nb(0x01, reg1_new);
    int wd_val = wd_item->valueint;
    cJSON_Delete(root);
    
    if (err != ESP_OK) return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"set_failed\"}");
    
    // Save to NVS
    int afe, noise_level, spike_rejection, min_strikes, watchdog;
    bool disturber_enabled;
    as3935_load_advanced_settings_nvs(&afe, &noise_level, &spike_rejection, 
                                      &min_strikes, &disturber_enabled, &watchdog);
    watchdog = wd_val;  // Update watchdog
    as3935_save_advanced_settings_nvs(afe, noise_level, spike_rejection, 
                                      min_strikes, disturber_enabled, watchdog);
    
    snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"watchdog\":%d}", wd_val);
    return http_reply_json(req, buf);
}

esp_err_t as3935_reboot_handler(httpd_req_t *req) {
    // Send OK response before rebooting
    http_reply_json(req, "{\"status\":\"ok\",\"message\":\"Device rebooting...\"}");
    
    // Schedule a reboot after a short delay to allow response to be sent
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    
    return ESP_OK;  // Never reached but keeps compiler happy
}

/**
 * @brief Test hooks
 */

bool as3935_validate_and_maybe_restore(int baseline_sp, int baseline_li, int duration_s) {
    ESP_LOGI(TAG, "Validation: baseline_sp=%d, baseline_li=%d, duration=%d",
             baseline_sp, baseline_li, duration_s);
    return true;
}

void as3935_test_set_counters(int sp, int li) {
    ESP_LOGI(TAG, "Test counters: sp=%d, li=%d", sp, li);
}

void as3935_test_get_counters(int *sp, int *li) {
    if (sp) *sp = 0;
    if (li) *li = 0;
}

void as3935_set_i2c_write_fn(esp_err_t (*fn)(uint8_t reg, uint8_t val)) {
    // Store function pointer if needed for testing
    (void)fn;
}
