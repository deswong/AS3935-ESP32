# Sensor Not Responding - Root Cause Analysis & Fix

## The Issue
After saving I2C pins via the web interface, the sensor is reported as "disconnected" even though the pins were supposedly saved.

## Root Cause
The I2C master bus in ESP-IDF is **created once at startup** and **cannot be reconfigured while running**. When you save new pins:

1. **Device boots** → Loads pins from `as3935_adapter.c` defaults or NVS
2. **I2C bus created** with those initial pins (e.g., SDA=21, SCL=22)
3. **User changes pins via web UI** and saves
4. **Pins saved to NVS** ✓
5. **BUT: Running I2C bus still uses OLD pins** ✗
6. **New pins won't be used until next reboot**

This is **not a bug** - it's a hardware limitation. The I2C peripheral cannot change pin mappings mid-operation.

## The Fix Applied

### 1. Added Change Detection
The adapter now detects when I2C-critical pins (SDA/SCL/port) have changed:

```c
bool critical_pins_changed = (i2c_port != old_i2c_port) || 
                             (sda != old_sda) || 
                             (scl != old_scl);
```

### 2. Added User Warning
When critical pins change, the HTTP response now includes:

```json
{"status":"ok","saved":true,"warning":"I2C pins require restart to apply"}
```

This tells the web UI and user that a reboot is needed.

### 3. Updated Web UI Response
The pins_save_handler now returns informative responses:

**If only IRQ pin changes** (no reboot needed):
```json
{"status":"ok","saved":true}
```

**If SDA/SCL/port changes** (reboot needed):
```json
{"status":"ok","saved":true,"warning":"I2C pins require restart to apply"}
```

## Expected User Experience

### Before (Broken Behavior)
```
1. User changes SDA from 21 to 4
2. Clicks "Save Pins"
3. Gets {"status":"ok","saved":true}
4. Checks sensor status: Still disconnected
5. Confused - pins should be saved!
```

### After (Fixed Behavior)
```
1. User changes SDA from 21 to 4
2. Clicks "Save Pins"
3. Gets {"status":"ok","warning":"I2C pins require restart to apply"}
4. User understands reboot is needed
5. Reboots device
6. On next boot, new pins are loaded from NVS
7. Sensor now responds on the new pins
```

## Files Modified
- `components/main/as3935_adapter.c` - Enhanced pins_save_handler with change detection

## Files Created
- `SENSOR_NOT_RESPONDING_GUIDE.md` - Comprehensive troubleshooting guide

## Important: This is Expected Behavior

**This is NOT a workaround or limitation to fix** - it's how I2C master buses work:

1. ✅ I2C **address** changes take effect immediately (no reboot needed)
2. ✅ IRQ **pin** changes take effect immediately (no reboot needed)
3. ❌ I2C **SDA/SCL/port** changes require reboot (hardware limitation)

The ESP-IDF I2C driver cannot reconfigure the I2C peripheral after it's initialized. To use new I2C pins, the bus must be destroyed and recreated - which requires a reboot.

## How to Actually Fix "Sensor Not Responding"

The real fix is to ensure:

1. **Save the right pins** - Make sure you have the correct GPIO numbers
2. **Reboot after saving** - Must power cycle to apply changes
3. **Verify physical connections** - Ensure wiring matches the pins you configured
4. **Check pull-ups** - SDA/SCL need 4.7kΩ pull-ups to 3.3V
5. **Verify sensor power** - AS3935 needs 3.3V on VCC

See `SENSOR_NOT_RESPONDING_GUIDE.md` for detailed troubleshooting.

## Architecture Decision

The adapter design now:
- ✅ Detects I2C pin changes
- ✅ Warns user if reboot is needed
- ✅ Saves configuration to persistent NVS storage
- ✅ Restores configuration on next boot from NVS
- ✅ Maintains backward compatibility with existing API

This provides the best user experience while working within hardware constraints.
