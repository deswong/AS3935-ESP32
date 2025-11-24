# ⚠️ FIRMWARE BUILD REQUIRED - Disturber Endpoint Still Crashing

## Current Status

**Problem:** Disturber endpoint still returns `net::ERR_CONNECTION_RESET`  
**Reason:** Firmware has NOT been rebuilt with the latest fixes  
**Solution:** Follow the build & flash steps below

---

## What's Been Fixed (In Code, Not Yet in Firmware)

✅ **Disturber Handler Rewritten**
- Safer register bit manipulation
- Better error handling
- Detailed logging
- Buffer overflow protection

✅ **Enhanced Setting Descriptions**
- All 6 settings have comprehensive info
- Info buttons fully functional

✅ **AFE Non-Auto-Selection**
- Shows "Select..." on initial load
- User must explicitly choose INDOOR/OUTDOOR

---

## Build & Flash Instructions

### Option 1: Use PowerShell Script (Easiest)
```powershell
# Navigate to project
cd d:\AS3935-ESP32

# Run the build script
.\build_and_flash.ps1
```

The script will:
1. Setup environment variables
2. Clean previous build
3. Build firmware
4. Ask for COM port and flash automatically

### Option 2: Manual Commands
```powershell
# Setup environment
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"

# Navigate to project
cd d:\AS3935-ESP32

# Clean and build
python -m idf.py fullclean
python -m idf.py build

# Flash (replace COM3 with your port)
python -m idf.py -p COM3 flash monitor
```

---

## Finding Your COM Port

If you don't know your COM port:
```powershell
Get-CimInstance Win32_PnPEntity | Where-Object {$_.Name -match "COM"} | Select-Object Name
```

Look for something like:
- "Silicon Labs CP210x USB to UART Bridge (COM3)"
- "USB Serial Device (COM3)"

---

## What to Expect During Build

**Build Output (Normal):**
```
Executing action: all (aliases: build)
Running cmake in directory D:\AS3935-ESP32\build
... [many lines of compilation] ...
Building C object components/main/CMakeFiles/main.dir/as3935_adapter.c.obj
Linking C executable as3935_esp32.elf
[100%] Built target as3935_esp32.elf
Project build complete. Your app partition table is:
...
```

**Time:** 1-3 minutes

**If it fails:** See troubleshooting below

---

## After Flashing - Verification

### Step 1: ESP32 Monitor
When you flash, you should see:
```
Hard resetting via RTS pin...
ESP-ROM:esp_rom_print_flash_desc: 0 header_byte
... [boot messages] ...
I (1023) main: Starting AS3935 Lightning Detector...
```

### Step 2: Open Web UI
1. Go to http://192.168.1.153 (or your device IP)
2. Click "⚙️ AS3935 Sensor - Advanced Settings" tab
3. Click "🔄 Reload Current" button

### Step 3: Check Browser Console
Open DevTools (F12) → Console tab

**Expected (After Fix):**
```
✅ AFE response status: 200 true
✅ Noise response status: 200 true
✅ Spike response status: 200 true
✅ Strikes response status: 200 true
✅ Watchdog response status: 200 true
✅ Disturber response status: 200 true  ← NOW WORKS!
✅ All settings loaded: {...}
```

**If Disturber Still Fails:**
- Build/flash didn't work
- Old firmware still running
- Try flashing again

---

## Troubleshooting

### Build Error: "cmake not found"
The PATH variable isn't set properly. Use the PowerShell script or run this first:
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"
```

### Build Error: "Compiler not found"
Make sure `riscv32-esp-elf-gcc.exe` exists:
```powershell
Get-ChildItem -Path "C:\Users\Des\.espressif\tools\riscv32-esp-elf" -Recurse -Filter "riscv32-esp-elf-gcc.exe"
```

If not found, your ESP-IDF install might be corrupted.

### Flash Error: "Port not found"
- Check COM port is correct
- Verify USB cable is connected
- Try a different USB port
- Restart device

### Disturber STILL Returns Error After Flash
1. Check ESP32 serial monitor for error messages
2. Verify sensor is initialized (check status endpoint: /api/as3935/status)
3. Check I2C pins (GPIO 7=SDA, GPIO 4=SCL)
4. Try rebooting ESP32

---

## Files Changed in This Session

| File | Change |
|------|--------|
| `components/main/as3935_adapter.c` | Disturber handler fixed |
| `components/main/web/index.html` | Settings info enhanced, AFE loading fixed |
| `components/main/include/web_index.h` | Regenerated (embeds web UI) |
| `build_and_flash.ps1` | NEW - Automated build script |

---

## Next Steps

**Do This Now:**
1. Open PowerShell in project directory
2. Run: `.\build_and_flash.ps1`
3. When prompted, enter your COM port (e.g., COM3)
4. Wait for flashing to complete
5. Test in browser using verification steps above

**If You Need Help:**
- Share the complete build error output
- Include your COM port number
- Describe what you see in the ESP32 monitor

---

**Expected Result:** All 6 settings load without errors, including Disturber endpoint ✅

**Time Required:** 5-10 minutes total

