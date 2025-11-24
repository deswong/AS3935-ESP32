# ESP-IDF v6 Specific Quirks and Fixes

This document outlines all known ESP-IDF v6 specific issues encountered and how they were resolved in this project.

## Critical Issues Fixed

### 1. **Double Event Loop and NetIF Initialization**

**Issue:** `wifi_prov_init()` was calling `esp_netif_init()` and `esp_event_loop_create_default()` even though these were already called from `app_main.c` during the init task.

**Root Cause:** In ESP-IDF v6, these initialization functions must be called **exactly once** per boot. Calling them multiple times causes undefined behavior, including potential memory corruption and crashes.

**Symptom:** Would cause crashes or unpredictable behavior during WiFi initialization.

**Fix Applied:**
- **File:** `components/main/wifi_prov.c` (lines 145-152)
- **Change:** Removed calls to `esp_netif_init()` and `esp_event_loop_create_default()` from `wifi_prov_init()`
- **Result:** These critical initialization functions are now called only once from `app_main.c` (lines 196, 199)

```c
// BEFORE (WRONG - causes crashes):
esp_err_t wifi_prov_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());  // DUPLICATE!
    ESP_ERROR_CHECK(esp_event_loop_create_default());  // DUPLICATE!
    ESP_ERROR_CHECK(esp_event_handler_instance_register(...));
    return ESP_OK;
}

// AFTER (CORRECT):
esp_err_t wifi_prov_init(void)
{
    // NOTE: esp_netif_init() and esp_event_loop_create_default() are called from app_main
    // DO NOT reinitialize them here - it causes undefined behavior in ESP-IDF v6
    ESP_ERROR_CHECK(esp_event_handler_instance_register(...));
    return ESP_OK;
}
```

### 2. **Double WiFi Driver Initialization (esp_wifi_init)**

**Issue:** `esp_wifi_init()` was being called in two places:
1. From `app_main.c` line 204 during init task
2. From `wifi_prov_start_connect_with_fallback()` line 225

**Root Cause:** WiFi driver can only be initialized once per boot in ESP-IDF v6. Calling `esp_wifi_init()` twice causes assertion failures or crashes.

**Symptom:** Runtime crash with "assertion failed" or "Load access fault" during WiFi initialization.

**Fix Applied:**
- **File:** `components/main/wifi_prov.c` (line 217-240)
- **Change:** Removed the duplicate `esp_wifi_init()` call and the `wifi_init_config_t cfg` initialization
- **Result:** WiFi driver is initialized only once from `app_main.c`

```c
// BEFORE (WRONG - double initialization):
void wifi_prov_start_connect_with_fallback(void)
{
    s_sta_netif = esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));  // DUPLICATE - CRASH!
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ... rest of config
}

// AFTER (CORRECT):
void wifi_prov_start_connect_with_fallback(void)
{
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    // Don't call esp_wifi_init again - it's already called from app_main
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // ... rest of config
}
```

### 3. **Duplicate WiFi STA Network Interface Creation**

**Issue:** `esp_netif_create_default_wifi_sta()` was being called without checking if it was already created.

**Locations:**
1. `wifi_prov_start_connect_with_fallback()` line 224
2. `wifi_connect_task()` line 310

**Root Cause:** Calling `esp_netif_create_default_wifi_sta()` multiple times without proper guards can cause memory leaks or interface conflicts in ESP-IDF v6.

**Fix Applied:**
- **Files:** `components/main/wifi_prov.c` (lines 224, 310)
- **Changes:** Added guard checks before creating the STA netif
- **Result:** Each netif is created only once, stored in static `s_sta_netif` variable

```c
// BEFORE (NO GUARD - potential issues):
void wifi_prov_start_connect_with_fallback(void)
{
    s_sta_netif = esp_netif_create_default_wifi_sta();  // May create multiple times
    // ...
}

// AFTER (WITH GUARD - correct):
void wifi_prov_start_connect_with_fallback(void)
{
    if (!s_sta_netif) {  // Guard - only create if not already created
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    // ...
}
```

## Verified Non-Issues (Already Correct)

### HTTP Server Configuration
✅ **Status:** Already correctly configured
- `max_uri_handlers = 32` (sufficient for all endpoints)
- `stack_size = 8192` (adequate for HTTP handler tasks with NVS operations)
- **File:** `components/main/app_main.c` lines 153-155

### WiFi Event Handler Registration
✅ **Status:** Correct in current code
- Uses `esp_event_handler_instance_register()` (modern ESP-IDF v6 API)
- Registers for both `WIFI_EVENT` and `IP_EVENT`
- **File:** `components/main/wifi_prov.c` lines 147-148

### MQTT Event Loop Integration
✅ **Status:** Correctly implemented
- MQTT initialization happens AFTER event loop creation
- MQTT event handler properly registers with `esp_mqtt_client_register_event()`
- **File:** `components/main/mqtt_client.c` line 74

### Memory Allocation Patterns
✅ **Status:** Correct usage
- Uses standard `malloc()`/`free()` appropriately in HTTP request handlers
- No custom heap manipulation needed for ESP-IDF v6
- **Files:** `components/main/mqtt_client.c`, `wifi_prov.c`, `ota.c`

### FreeRTOS API Usage
✅ **Status:** All modern APIs used
- `xTaskCreate()` with proper parameters
- `vTaskDelete(NULL)` for self-deletion
- `pdMS_TO_TICKS()` for time conversion
- No deprecated `uxTaskGetStackHighWaterMark()` or `vTaskSuspend()` calls
- **Files:** Throughout main components

### NVS Flash Initialization
✅ **Status:** Correct pattern
- Handles `ESP_ERR_NVS_NO_FREE_PAGES` and `ESP_ERR_NVS_NEW_VERSION_FOUND`
- Erases and re-initializes NVS on version mismatch
- **File:** `components/main/app_main.c` lines 185-189

### Logging and Console Setup
✅ **Status:** Removed manual UART setup (correct for v6)
- No more `uart_driver_install()` calls conflicting with ESP-IDF's logging
- Relies on ESP-IDF's built-in console initialization
- **Reference:** `docs/CONSOLE_UART_CONFLICT_FIX.md`

## Initialization Order - Final Correct Sequence

The init_task() now follows the correct initialization order for ESP-IDF v6:

```c
1. nvs_flash_init()              // Initialize NVS flash
2. settings_init()               // Load settings from NVS
3. esp_event_loop_create_default()  // Create event loop (CRITICAL - before WiFi/MQTT)
4. esp_netif_init()              // Initialize TCP/IP stack
5. esp_wifi_init(&cfg)           // Initialize WiFi driver (once only)
6. wifi_prov_init()              // Register WiFi event handlers
7. wifi_prov_start_*()           // Start WiFi connection (AP or STA)
8. mqtt_init()                   // Initialize MQTT (safe - event loop ready)
9. as3935_init_from_nvs()        // Initialize sensor
10. start_webserver()            // Start HTTP server
11. httpd_register_uri_handler() // Register API endpoints
```

This sequence ensures:
- ✅ Event loop exists before any async operations (WiFi, MQTT)
- ✅ Each initialization function called exactly once
- ✅ No conflicting UART/console setup
- ✅ All dependencies initialized in correct order

## Configuration Summary

**File:** `sdkconfig`
- `ESP_MAIN_TASK_STACK_SIZE = 3584` - Sufficient for init task launcher
- `CONFIG_LWIP_TCPIP_TASK_STACK_SIZE = 3072` - Standard for TCP/IP
- `HTTP max_uri_handlers = 32` - Runtime configured in app_main.c
- `HTTP stack_size = 8192` - Runtime configured in app_main.c

## Testing and Validation

After these fixes, verify:
1. ✅ Firmware compiles without errors or warnings
2. ✅ Device boots without crashes
3. ✅ WiFi initialization logs appear correctly
4. ✅ WiFi connects successfully to saved SSID or starts AP
5. ✅ HTTP server is accessible at `http://192.168.4.1`
6. ✅ MQTT connects after 5-10 seconds (if configured)
7. ✅ All HTTP API endpoints respond correctly

## References

- **Main fixes:** `components/main/wifi_prov.c`
- **Related documentation:** 
  - `docs/CONSOLE_UART_CONFLICT_FIX.md` - Manual console setup removal
  - `docs/RUNTIME_CRASH_FIX.md` - Event loop initialization order
  - `docs/MQTT_CONNECTION_STATE_FIX.md` - MQTT event handling
