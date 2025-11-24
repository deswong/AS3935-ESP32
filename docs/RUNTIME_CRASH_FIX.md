# Runtime Crash Fix - Event Loop and Console Initialization Order

## Issue
The firmware was crashing with a "Guru Meditation Error: Load access fault" during WiFi initialization. The crash occurred in the logging/printf infrastructure during WiFi driver task startup.

## Root Causes

### 1. **Event Loop Created Too Late**
- MQTT client initialization requires the default event loop to be already created
- Previously, `mqtt_init()` was called BEFORE `esp_event_loop_create_default()`
- This caused the MQTT client event handler registration to fail or access invalid event loop memory

### 2. **Console Reinitialization Conflict**
- `ensure_console()` was being called from both `app_main()` AND `mqtt_init()`
- Multiple calls to `uart_driver_install()` and VFS redirection setup could cause state corruption
- The static `console_inited` flag prevents reinit, but the function was still being called unnecessarily

### 3. **Initialization Order**
Previous order:
1. app_main() calls ensure_console() 
2. init_task() loads MQTT settings and calls mqtt_init() 
3. mqtt_init() calls ensure_console() again (conflicts!)
4. WiFi initialization (at this point, logging/VFS might be corrupted)

## Solutions Applied

### 1. **Reordered Initialization in init_task()**
New order:
1. NVS flash initialization
2. Settings (NVS) initialization
3. **Event loop creation (CRITICAL - must be before MQTT or any event-based initialization)**
4. TCP/IP stack initialization
5. WiFi driver initialization
6. WiFi provisioning setup
7. **MQTT initialization (after event loop is ready)**
8. AS3935 sensor initialization
9. HTTP endpoint registration

### 2. **Removed Redundant ensure_console() Call**
- Removed `ensure_console()` from `mqtt_init()` function
- Console is now initialized only once in `app_main()` before creating any tasks
- This prevents UART/VFS reinitialization conflicts

### 3. **Protected UART/VFS Setup**
- The `ensure_console()` function has a static guard (`console_inited` flag)
- But by only calling it once, we avoid any edge cases

## Changes Made

### File: `components/main/app_main.c`
- Moved MQTT initialization to AFTER event loop creation
- WiFi setup now happens before MQTT initialization
- HTTP server creation still happens early

### File: `components/main/mqtt_client.c`
- Removed `ensure_console()` call from `mqtt_init()` function
- Simplified mqtt_init() initialization sequence

## Why This Fixes the Crash

1. **Event Loop Ready**: MQTT client can now safely register its event handler because the default event loop exists
2. **Clean UART/VFS Setup**: Console I/O is set up only once during app startup, preventing VFS corruption
3. **Proper Initialization Order**: Dependencies are initialized in the correct order (event loop → WiFi → MQTT)
4. **Reduced Stack Pressure**: By separating WiFi setup from MQTT setup timing, potential stack conflicts are reduced

## Testing Notes

After this fix, the firmware should:
- ✅ Boot without crashes during WiFi initialization
- ✅ Successfully initialize the event loop
- ✅ Properly initialize MQTT client with event handling
- ✅ Display all WiFi initialization logs without access faults
- ✅ Continue to successful HTTP server startup

If crashes continue, they may be caused by:
- NVS data corruption (try erasing flash: `idf.py erase_flash`)
- WiFi configuration conflicts
- Insufficient task stack size (already set to 8192 bytes)
