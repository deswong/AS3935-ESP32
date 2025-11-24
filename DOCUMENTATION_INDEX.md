# AS3935 Lightning Monitor - Documentation

This is the documentation for the AS3935 Lightning Monitor firmware for ESP32. The project provides a web UI for configuration, I2C interface to the sensor, and MQTT publishing for lightning events.

## 📚 Quick Start by Role

### 👤 I'm a First-Time User
1. **[README.md](README.md)** - Start here! Overview and building instructions
2. **[I2C_PINOUT_GUIDE.md](I2C_PINOUT_GUIDE.md)** - How to wire the hardware
3. **[I2C_ADDRESS_QUICK_START.md](I2C_ADDRESS_QUICK_START.md)** - Get the sensor working

### 🔧 I'm Configuring the Sensor
1. **[AS3935_ADVANCED_SETTINGS_QUICK.md](AS3935_ADVANCED_SETTINGS_QUICK.md)** ⭐ **START HERE** - Quick reference for the web UI
2. **[AS3935_REGISTER_CONFIGURATION.md](AS3935_REGISTER_CONFIGURATION.md)** - Complete guide to all registers and tuning
3. **[LIVE_STATUS_DISPLAY.md](LIVE_STATUS_DISPLAY.md)** - How to monitor sensor status in real-time

### 🆘 Troubleshooting
1. **[URGENT_I2C_NACK_ERROR.md](URGENT_I2C_NACK_ERROR.md)** - Sensor not responding? Start here
2. **[I2C_ADDRESS_CONFIGURATION.md](I2C_ADDRESS_CONFIGURATION.md)** - Change I2C address or diagnose address issues

### 💻 I'm Integrating with Code
1. **[AS3935_REGISTER_API.md](AS3935_REGISTER_API.md)** - REST API endpoints for register control
2. **[REGISTER_CONFIG_FEATURE.md](REGISTER_CONFIG_FEATURE.md)** - Overview of the register configuration system

### � Visual Guides
- **[LIVE_STATUS_VISUAL.md](LIVE_STATUS_VISUAL.md)** - ASCII diagrams of what the UI looks like
- **[I2C_PINOUT_GUIDE.md](I2C_PINOUT_GUIDE.md)** - Pin wiring diagrams

## 📖 Complete File Reference

| File | Purpose | Audience |
|------|---------|----------|
| **README.md** | Project overview, build & flash instructions | Everyone |
| **I2C_PINOUT_GUIDE.md** | Hardware wiring and pin assignment | Hardware setup |
| **I2C_ADDRESS_QUICK_START.md** | First-time sensor setup (30 sec) | New users |
| **I2C_ADDRESS_CONFIGURATION.md** | I2C address setup & troubleshooting | Hardware config |
| **AS3935_ADVANCED_SETTINGS_QUICK.md** | Web UI quick reference & fixes | Users tuning sensor |
| **AS3935_REGISTER_CONFIGURATION.md** | Complete register guide & tuning scenarios | Advanced users |
| **AS3935_REGISTER_API.md** | REST API endpoints & code examples | Developers |
| **REGISTER_CONFIG_FEATURE.md** | Feature overview & technical summary | Developers |
| **LIVE_STATUS_DISPLAY.md** | Real-time status monitoring guide | Users |
| **LIVE_STATUS_VISUAL.md** | Visual examples & ASCII diagrams | Visual learners |
| **URGENT_I2C_NACK_ERROR.md** | Sensor not responding troubleshooting | When stuck |
| **RELEASE.md** | Release checklist & validation steps | Maintainers |

---

## 🎯 Common Questions → Documentation

| Question | See File |
|----------|----------|
| How do I build and flash? | README.md |
| How do I wire the hardware? | I2C_PINOUT_GUIDE.md |
| Sensor not responding? | URGENT_I2C_NACK_ERROR.md |
| How do I use the web UI? | AS3935_ADVANCED_SETTINGS_QUICK.md |
| Too many false lightning alerts? | AS3935_REGISTER_CONFIGURATION.md → Scenario 1 |
| Missing real lightning? | AS3935_REGISTER_CONFIGURATION.md → Scenario 2 |
| How do I control registers from code? | AS3935_REGISTER_API.md |
| What changed in this release? | REGISTER_CONFIG_FEATURE.md |
| What do the status lights mean? | LIVE_STATUS_DISPLAY.md |

---

## 🚀 Quickest Start (5 Minutes)

```bash
# 1. Build
idf.py build

# 2. Flash
idf.py flash monitor

# 3. Open web UI (check serial output for IP)
http://192.168.x.x/

# 4. Configure I2C address (default 0x03)
# See: I2C_ADDRESS_QUICK_START.md

# 5. Adjust sensor sensitivity if needed
# See: AS3935_ADVANCED_SETTINGS_QUICK.md
```

---

## 🔑 Key Features

✨ **Advanced Sensor Settings** - Configure all 9 AS3935 registers from web UI  
📡 **I2C Configuration** - Change I2C address and pins without recompiling  
📊 **Live Status Display** - Real-time register monitoring every 3 seconds  
💾 **Persistent Config** - Settings saved to device memory (NVS)  
🌐 **Web UI** - Clean, responsive interface for all settings  
� **MQTT Integration** - Publish lightning events to broker  
📱 **Responsive Design** - Works on mobile and desktop

---

## Files Modified

### Code Changes
- **`components/main/as3935.c`**
  - Enhanced SPI write function (lines ~273-289)
  - Enhanced SPI read function (lines ~301-325)
  - Enhanced SPI initialization (lines ~300-365)
  - Enhanced POST function (lines ~404-454)

### Documentation (NEW)
- **`POST_QUICK_START.md`** - Quick reference for users
- **`DIAGNOSTICS_ENHANCEMENTS.md`** - Complete technical documentation
- **`CODE_CHANGES_REFERENCE.md`** - Detailed code changes
- **`ENHANCEMENT_SUMMARY.md`** - Implementation overview
- **`DOCUMENTATION_INDEX.md`** - This file

---

## Example Output

### Success Case ✅
```
I (123) as3935: AS3935: Starting power-on self-test (POST)...
I (125) as3935: ✓ Register read successful: reg[0x00]=0x1E
I (127) as3935: ✓ Register write successful: wrote 0x0E to reg[0x00]
I (129) as3935: ✓ Register readback successful: reg[0x00]=0x0E
I (131) as3935: AS3935 POST Complete - Register snapshot:
I (133) as3935:   reg[0x00] (AFE_GAIN)  = 0x0E
I (135) as3935:   reg[0x01] (THRESHOLD) = 0x42
I (137) as3935:   reg[0x03] (STATUS)    = 0x00
I (139) as3935:   reg[0x08] (DIST)      = 0xFF
I (141) as3935: ✓ AS3935 POST passed - sensor is responsive
```
**Interpretation:** Sensor is working perfectly! ✅

### Failure Case ❌
```
I (123) as3935: AS3935: Starting power-on self-test (POST)...
W (125) as3935: SPI read reg 0x00 failed: error 12 (possible causes: SPI bus busy, device not responding, GPIO conflict)
E (127) as3935: POST FAILED: Cannot read register 0x00 (SPI communication error: 12)
```
**Interpretation:** SPI communication not working. Check GPIO wiring and power supply.

---

## How to Use These Docs

### Scenario 1: Quick Test
1. Read: `POST_QUICK_START.md`
2. Follow build steps
3. Look for expected output
4. Done!

### Scenario 2: Detailed Understanding
1. Read: `ENHANCEMENT_SUMMARY.md`
2. Skim: `DIAGNOSTICS_ENHANCEMENTS.md` (specific sections)
3. Reference: `CODE_CHANGES_REFERENCE.md` if needed
4. Done!

### Scenario 3: Troubleshooting
1. Note exact error message from boot log
2. Open: `DIAGNOSTICS_ENHANCEMENTS.md`
3. Find error in "Troubleshooting Guide" section
4. Follow recommended actions
5. Done!

### Scenario 4: Code Review
1. Open: `CODE_CHANGES_REFERENCE.md`
2. Read section for specific function
3. See before/after comparison
4. Review changes in actual file
5. Done!

---

## Key Features

### ✅ POST (Power-On Self-Test)
- 5-step diagnostic sequence
- SPI read verification
- SPI write verification
- Write readback confirmation
- Register state snapshot
- Clear pass/fail indication

### ✅ Enhanced Error Messages
- Specific error codes
- Possible cause hints
- Contextual information
- Actionable guidance

### ✅ Comprehensive Logging
- Initialization step logging
- Function entry/exit diagnostics
- Register state reporting
- Diagnostic snapshots

### ✅ User-Friendly Output
- Clear success markers (✓)
- Clear failure markers (✗)
- Hierarchical information
- Easy to scan output

---

## Testing Workflow

### Step 1: Build
```bash
idf.py build
```
Expected: No compilation errors ✅

### Step 2: Flash
```bash
idf.py flash
```
Expected: Flash completes successfully ✅

### Step 3: Monitor
```bash
idf.py monitor
```
Expected: Boot log with POST section ✅

### Step 4: Verify
Look for POST messages in boot log:
- ✅ All green (✓ marks) = Sensor working
- ❌ Any red (✗ or E marks) = Refer to troubleshooting

### Step 5: Check Web UI
Open http://device-ip
- Status section shows ✓ ACTIVE if working
- Shows ✗ NOT INITIALIZED if not working

---

## Troubleshooting Matrix

| Issue | Look For | Likely Cause | See |
|-------|----------|-------------|-----|
| POST failed at read | `Cannot read register 0x00` | SPI communication dead | POST_QUICK_START.md → Common Issues |
| POST failed at write | `Cannot write register 0x00` | SPI write issue | DIAGNOSTICS_ENHANCEMENTS.md → Troubleshooting |
| GPIO conflicts | `GPIO pins already in use` | GPIO 11-21 used | DIAGNOSTICS_ENHANCEMENTS.md → Symptom list |
| All registers 0xFF | `DIST = 0xFF, STATUS = 0xFF` | No data from sensor | POST_QUICK_START.md → Common Issues |
| SPI bus init failed | `error code 259` | Invalid GPIO or conflict | DIAGNOSTICS_ENHANCEMENTS.md → Error codes |

---

## Quick Reference

### Error Codes
| Code | Meaning | Action |
|------|---------|--------|
| 12 | ESP_ERR_INVALID_STATE | SPI device not initialized or already in use |
| 259 | ESP_ERR_NO_MEM / ESP_ERR_INVALID_ARG | Invalid GPIO pins or bus conflict |

### Key Registers (from POST snapshot)
| Register | Name | What It Shows |
|----------|------|---------------|
| 0x00 | AFE_GAIN | Gain configuration (0x0E = good) |
| 0x01 | THRESHOLD | Detection threshold (0x00-0xFF) |
| 0x03 | STATUS | Current sensor status |
| 0x08 | DIST | Distance estimation (0xFF = no event) |

### Expected Behavior
- **Normal:** Mixed register values, not all 0x00 or all 0xFF
- **Good:** Some 0x00 values mixed with other values
- **Bad:** All 0xFF (no response) or all 0x00 (uninitialized)

---

## Support Resources

### Documentation Files in This Project
- `POST_QUICK_START.md` - Quick start guide
- `DIAGNOSTICS_ENHANCEMENTS.md` - Complete reference
- `CODE_CHANGES_REFERENCE.md` - Code details
- `ENHANCEMENT_SUMMARY.md` - Overview
- `DOCUMENTATION_INDEX.md` - This file

### In the Code
- `components/main/as3935.c` - Enhanced driver with POST

---

## Next Steps

### For Immediate Testing
1. Review `POST_QUICK_START.md`
2. Run `idf.py build && idf.py flash`
3. Monitor boot output
4. Verify POST passes

### For Detailed Understanding
1. Review `ENHANCEMENT_SUMMARY.md`
2. Read `DIAGNOSTICS_ENHANCEMENTS.md` sections of interest
3. Reference `CODE_CHANGES_REFERENCE.md` as needed

### For Troubleshooting
1. Note exact error message
2. Find in `DIAGNOSTICS_ENHANCEMENTS.md` troubleshooting section
3. Follow recommended actions
4. Share findings if further help needed

---

## Version Information

**Enhancement Version:** 1.0
**Date:** Current
**Compatibility:** ESP-IDF v6.0-beta1, ESP32-C3
**Files Modified:** 1 (components/main/as3935.c)
**Code Changes:** ~100 lines
**Status:** ✅ Ready for use

---

## Summary

This enhancement package provides:
- ✅ Comprehensive power-on self-test (POST)
- ✅ Enhanced SPI communication diagnostics
- ✅ Detailed error reporting
- ✅ Clear pass/fail indicators
- ✅ Register state snapshots
- ✅ Troubleshooting guidance
- ✅ Complete documentation

**Result:** Users can now easily diagnose why sensor initialization fails and take corrective action.

---

**Happy Testing! 🚀**

For questions or issues, refer to the appropriate documentation file above.
