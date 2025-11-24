# Console UART/VFS Conflict Fix

## Problem

The firmware was crashing with "Guru Meditation Error: Load access fault" during WiFi initialization when trying to output logs. The crash occurred in `_vfprintf_r()` while the WiFi driver was logging.

## Root Cause Analysis

The issue was caused by **manual UART driver installation and VFS redirection conflicts**:

1. **Double UART Initialization**: The `ensure_console()` function was calling `uart_driver_install()` to manually set up the UART driver
2. **VFS Redirection Conflicts**: The manual `uart_vfs_dev_use_driver()` or `esp_vfs_dev_usb_serial_jtag_use_driver()` calls were conflicting with ESP-IDF's built-in logging infrastructure
3. **Task Context Issues**: When WiFi driver tried to log from a different task context, the manually-configured UART/VFS stack was in an inconsistent state
4. **Printf Implementation Mismatch**: The manually-configured console was incompatible with how the ESP-IDF logging system works internally

## Solution

**Removed all manual console initialization code**. ESP-IDF v6.0 already has built-in console initialization:

1. **Removed `ensure_console()` function** - This function was unnecessary and harmful
2. **Removed manual UART includes** - No need for conditional UART/VFS header includes
3. **Removed VFS setup code** - ESP-IDF handles this automatically
4. **Removed `ensure_console()` call from `app_main()`** - Let ESP-IDF's default logging handle output

## Changes Made

### File: `components/main/app_main.c`
- Removed: `extern void ensure_console(void);` forward declaration
- Removed: `ensure_console();` call from `app_main()`

### File: `components/main/mqtt_client.c`
- Removed: Entire `ensure_console()` function (30+ lines)
- Removed: All UART and VFS header includes with conditional compilation
- Removed: UART driver installation (`uart_driver_install()`)
- Removed: Manual VFS redirection (`uart_vfs_dev_use_driver()`, `esp_vfs_dev_usb_serial_jtag_use_driver()`)
- Removed: Forward declarations for UART functions
- Cleaned up: `#define MQTT_HAVE_USB_SERIAL_JTAG` and related conditionals

## How ESP-IDF Handles Console by Default

ESP-IDF v6.0 automatically:
1. Initializes the UART driver for GPIO 20/21 (RX/TX on ESP32-C3)
2. Sets up VFS redirection for stdout/stderr
3. Configures the logging system to use UART output
4. All of this happens during bootloader and during main task initialization

**Manual intervention breaks this system** by:
- Installing UART driver twice (conflicting state)
- Reconfiguring VFS while ESP-IDF's logging is using it
- Creating race conditions between task logging attempts

## Result

After this fix:
- ✅ WiFi initialization proceeds without crashes
- ✅ All logs appear correctly on serial output
- ✅ No UART/VFS conflicts
- ✅ Cleaner, more maintainable code
- ✅ Follows ESP-IDF best practices

## Key Lesson

**Don't manually initialize UART/VFS for console when using ESP-IDF v6+**. The framework handles this automatically and robustly. Manual intervention should only be used for custom logging solutions, not standard console output.
