# Quick Start: Build After Refactoring

## For VSCode Users (RECOMMENDED)

If you have the ESP-IDF extension installed in VSCode:

1. **Open Command Palette**: `Ctrl+Shift+P`
2. **Type**: "ESP-IDF: Build your project"
3. **Press Enter**
4. **Wait** for build to complete

The VSCode extension handles all environment setup automatically.

---

## Alternative: Manual Build with PowerShell

### Step 1: Verify Prerequisites
```powershell
# Check CMake
cmake --version

# Check Ninja  
ninja --version

# Check Python
python --version
```

### Step 2: Navigate to Project
```powershell
cd d:\AS3935-ESP32
```

### Step 3: Run Build
```powershell
# Method 1: Simple (if paths are already set up)
idf.py build

# Method 2: With explicit Python environment
& "C:\Users\Des\.espressif\python_env\idf6.0_py3.11_env\Scripts\python.exe" "C:\Users\Des\esp\v6.0\esp-idf\tools\idf.py" build
```

### Step 4: Wait for Completion
Build should complete with output like:
```
[100%] Linking CXX executable ...
Project build complete. The binary you wanted is in: build/as3935_esp32.bin
```

---

## If Build Fails

### Problem: "cmake" not found
```powershell
# Add CMake to PATH for current session
$env:PATH = "C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;$env:PATH"
idf.py build
```

### Problem: "ninja" not found
```powershell
# Add Ninja to PATH
$env:PATH = "C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"
idf.py build
```

### Problem: Python module errors
```powershell
# Upgrade Python dependencies
& "C:\Users\Des\.espressif\python_env\idf6.0_py3.11_env\Scripts\pip.exe" install --upgrade -r "C:\Users\Des\esp\v6.0\esp-idf\requirements.txt"
idf.py build
```

### Problem: "embed_web.py" not found
```powershell
# Clean and rebuild
idf.py fullclean
idf.py build
```

---

## Next Steps After Build

### 1. Verify Build Success
```powershell
idf.py size    # Shows binary size
```

### 2. Flash to ESP32-C3
```powershell
idf.py -p COM3 flash monitor
# Replace COM3 with your actual port (check Device Manager)
```

### 3. Verify on Device
Watch serial output for:
```
AS3935 initialized successfully
I2C bus initialized
Device ready
```

### 4. Test Web UI
- Open browser: `http://<device-ip>/`
- Navigate to AS3935 settings
- Try reading sensor status
- Check that sensor shows as connected

---

## Common Ports

**Windows**:
- COM1, COM3, COM4 (check Device Manager)

**Finding Your Port**:
```powershell
# List all COM ports
Get-WmiObject -Class Win32_SerialPort | Select Name, Description
```

---

## Success Indicators

✅ Build completes without errors  
✅ Binary created at `build/as3935_esp32.bin`  
✅ Device flashes successfully  
✅ Serial monitor shows boot messages  
✅ Web UI loads  
✅ Sensor status shows "Connected"  

---

## Troubleshooting Checklist

- [ ] Python version 3.11+ installed
- [ ] CMake available in PATH
- [ ] Ninja available in PATH
- [ ] No spaces in project path (currently OK: `D:\AS3935-ESP32`)
- [ ] Git installed (for component cloning)
- [ ] Sufficient disk space (~500MB for build)
- [ ] USB device properly connected
- [ ] Device driver installed (CH340 or similar)

---

## If Still Stuck

1. **Clean and retry**:
   ```powershell
   idf.py fullclean
   idf.py build
   ```

2. **Use VSCode extension** (most reliable)
3. **Check build logs**:
   ```powershell
   cat build\log\idf_py_stderr_output_*.txt
   ```

---

## What Changed

The refactoring should be transparent to the build system:
- Old `components/main/as3935.c` → New `as3935_adapter.c`
- Old custom driver → New `esp_as3935` library from k0i05
- Same API, same functionality, cleaner architecture

**No code changes needed in app_main.c or anywhere else.**
