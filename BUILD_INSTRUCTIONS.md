# Build and Flash Instructions

## Prerequisites
Ensure you have ESP-IDF v6.0 environment set up with:
- CMake
- Ninja
- RISC-V ESP-ELF compiler
- ESP-IDF Python environment

## Quick Build & Flash

### On Windows PowerShell:

```powershell
# Set up environment paths
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"

# Navigate to project
cd D:\AS3935-ESP32

# Clean build directory (optional, only if you want fresh rebuild)
Remove-Item -Path "build" -Recurse -Force -ErrorAction SilentlyContinue

# Build firmware
python -m idf.py build

# Flash to device (replace COM port as needed)
python -m idf.py -p COM3 flash

# Monitor output
python -m idf.py -p COM3 monitor
```

### On Linux/macOS:

```bash
cd /path/to/AS3935-ESP32

# Clean build
rm -rf build

# Build
idf.py build

# Flash (adjust -p port as needed)
idf.py -p /dev/ttyUSB0 flash

# Monitor
idf.py -p /dev/ttyUSB0 monitor
```

## What Changed

This build includes fixes for:
1. **JavaScript Event Listener Errors** - All DOM elements are now safely accessed with null checks
2. **HTTP 405 Errors** - Advanced settings endpoints now properly support GET and POST methods
3. **Missing Header Include** - `app_main.c` now includes `as3935_adapter.h`

## Verification After Flash

Once flashed and running:

1. Open web interface at device's IP (e.g., http://192.168.1.153)
2. Navigate to "Advanced Settings" tab
3. Browser Console (F12 → Console):
   - Should show NO "Element not found" warnings
   - Should show NO "Cannot read properties of null" errors
   - Should show "Starting loadSettings..." then "loadSettings completed successfully"
4. Check browser Network tab:
   - GET requests to `/api/as3935/settings/*` should return **200 OK**
   - POST requests to `/api/as3935/settings/*` should return **200 OK**
   - No 405 errors

## Troubleshooting Build

### CMake not found
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;$env:PATH"
```

### Ninja not found
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"
```

### Compiler not found
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;$env:PATH"
```

### All at once (recommended):
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"
```

## Build Output

Successful build will end with:
```
[100%] Built target app-flash_args
...
esptool.py v4.7.0
...
Wrote 0x... bytes to file build/bootloader.bin
Wrote 0x... bytes to file build/partition-table.bin
...
Project build complete. The following files were generated:
  build/as3935_esp32.elf
  build/as3935_esp32.bin
  ... (other artifacts)
```

## Next Steps

1. Build and flash this version
2. Test the Advanced Settings in the web UI
3. Verify all endpoints return 200 OK responses
4. Test loading current settings
5. Test applying new settings
6. Monitor ESP logs for any errors

---

**For support:** Check the console output at runtime and review browser developer tools (F12) for detailed error messages.
