# ⚠️ CRITICAL: Build & Flash Required!

## Current Status
✅ All code fixes have been implemented  
⚠️ **BUT firmware has NOT been rebuilt yet**

The disturber endpoint is STILL crashing because you're running the **OLD firmware** that was compiled BEFORE the fixes.

---

## Why It's Still Failing

1. ✅ Code was modified: `components/main/as3935_adapter.c` (disturber handler)
2. ✅ Web header was regenerated: `components/main/include/web_index.h`
3. ❌ **Firmware was NOT rebuilt** ← This is the issue
4. ❌ Old firmware was NOT reflashed

You're still running the OLD code that crashes.

---

## What You Must Do Now

### Step 1: Setup Build Environment Variables
Copy and paste this command EXACTLY into PowerShell:

```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"
```

### Step 2: Clean Build (Important!)
```powershell
cd d:\AS3935-ESP32
python -m idf.py fullclean
python -m idf.py build
```

**Expected output:**
```
Building...
[100%] Built target as3935_esp32.elf
Project build complete. Your app partition table is:
...
```

If you see compilation errors, DON'T proceed - let me know immediately.

### Step 3: Flash to ESP32
```powershell
python -m idf.py -p COM3 flash monitor
```

Replace `COM3` with your actual serial port. You should see:
```
Erasing flash...
Writing at 0x...
Verifying...
Hard resetting via RTS pin...
```

### Step 4: Verify in Browser
1. Open http://192.168.1.153 (or your device IP)
2. Go to "⚙️ AS3935 Sensor - Advanced Settings"
3. Click "🔄 Reload Current"
4. Check browser console for:

**BEFORE (Current - Still Broken):**
```
✅ AFE response status: 200 true
✅ Noise response status: 200 true
✅ Spike response status: 200 true
✅ Strikes response status: 200 true
✅ Watchdog response status: 200 true
❌ GET http://192.168.1.153/api/as3935/settings/disturber net::ERR_CONNECTION_RESET
```

**AFTER (What You'll See After Rebuild+Flash):**
```
✅ AFE response status: 200 true
✅ Noise response status: 200 true
✅ Spike response status: 200 true
✅ Strikes response status: 200 true
✅ Watchdog response status: 200 true
✅ Disturber response status: 200 true  ← FIXED!
```

---

## Why This Matters

The fixes I made are in the **source code**, but the ESP32 runs **compiled firmware**. Think of it like:
- Source code (C files) = Recipe
- Compiled firmware = Cooked meal
- ESP32 = Eats the meal

You updated the recipe, but haven't cooked yet! The ESP32 is still eating the old meal.

---

## Troubleshooting Build Issues

### If build fails with "cmake not found"
The PATH setup might not have worked. Try:
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"
cd d:\AS3935-ESP32
python -m idf.py build 2>&1 | Select-Object -Last 50
```

### If build fails with "compiler not found"
Try the build folder cleanup:
```powershell
Remove-Item -Path "d:\AS3935-ESP32\build" -Recurse -Force
python -m idf.py build
```

### If flash fails
Check your serial port:
```powershell
Get-CimInstance Win32_PnPEntity | Where-Object {$_.Name -match "COM"}
```

---

## Quick Checklist

- [ ] 1. Copied and ran PATH setup command
- [ ] 2. Ran `fullclean`
- [ ] 3. Ran `build` (check for SUCCESS message)
- [ ] 4. Ran `flash monitor`
- [ ] 5. Tested in browser
- [ ] 6. Verified all 6 settings load (including Disturber)

---

## Contact if Issues

If the build fails, share:
1. The full error message
2. The last 50 lines of output
3. Your COM port number

**Example:**
```
python -m idf.py build 2>&1 | Select-Object -Last 50
```

---

**This is the final step to get everything working!** 🎯

Once you build and flash, all 3 fixes will be active:
- ✅ Info buttons showing detailed descriptions
- ✅ AFE not auto-selected 
- ✅ Disturber endpoint working (no more crash)
