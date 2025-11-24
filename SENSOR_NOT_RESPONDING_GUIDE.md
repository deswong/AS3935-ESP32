# Sensor Not Responding - Troubleshooting Guide

## Problem
After saving pins via the web interface, the AS3935 sensor is not responding / shows "disconnected" status.

## Root Cause
The I2C bus is created once at device startup with initial pins. When you save NEW pins via the web interface:
1. New pins are saved to NVS (flash memory)
2. The **running I2C bus still uses the OLD pins** from device startup
3. The new pins won't take effect until the device is **rebooted**

This is by design - the ESP-IDF I2C master bus cannot be reconfigured after creation without destroying it first.

## Solutions

### Option 1: Reboot After Changing Pins (REQUIRED)
After saving new pins in the web UI:
1. **Save the pins** in the web interface
2. **Check the response** - if it says "I2C pins require restart", reboot is needed
3. **Reboot the device** (power cycle or use ESP-IDF monitor Ctrl+T -> Ctrl+R)
4. On next boot, the new pins will be loaded from NVS and used

### Option 2: Verify Pins Are Being Saved
Check that pins are actually being saved to NVS:

**In Web UI:**
- Open Developer Console (F12)
- Go to Network tab
- Save pins
- Check the response has `"status":"ok","saved":true`

**In Serial Monitor:**
- Watch for log: `Pins saved to NVS: i2c_port=0, sda=XX, scl=YY, irq=ZZ`
- Confirms NVS write was successful

### Option 3: Verify Pins Are Loaded on Boot
Check that saved pins are loaded during startup:

**In Serial Monitor at boot, look for:**
```
as3935_adapter: AS3935 adapter initialized: i2c_addr=0x03, irq_pin=23
as3935_adapter: I2C bus created on port 0, sda=21, scl=22
```

If you see different pin numbers than expected, the NVS load didn't work. Check:
1. Were pins actually saved? (check log during save)
2. Did you reboot after saving?
3. Is NVS partition initialized?

## Detailed Workflow

### Correct Workflow - First Time Setup
```
Device boots
    ↓
Loads default pins from as3935_adapter.c: SDA=21, SCL=22, IRQ=23
    ↓
Creates I2C bus on port 0, pins 21/22
    ↓
I2C is ready, but sensor might not be on these pins
    ↓
User opens web UI → AS3935 Sensor - I2C Configuration
    ↓
User changes pins (e.g., SDA=4, SCL=5) and clicks "Save Pins"
    ↓
CURRENT BEHAVIOR: I2C bus still uses pins 21/22 (old pins)
    ↓
User must REBOOT the device
    ↓
On next boot: NVS loads new pins SDA=4, SCL=5
    ↓
New I2C bus created on pins 4/5
    ↓
Sensor now responds if it's actually on pins 4/5
```

### If Sensor Still Doesn't Respond After Reboot

**1. Verify Correct Pins Used:**
- Check serial monitor at boot for I2C bus creation message
- Confirm it shows your new SDA/SCL pins
- Example: `I2C bus created on port 0, sda=4, scl=5` ✓

**2. Verify I2C Bus is Connected:**
- With a logic analyzer or scope, probe the SDA/SCL pins
- Look for I2C bus activity (high/low transitions)
- Should see pull-up resistors (lines normally high)

**3. Verify Sensor Hardware:**
- Check AS3935 is powered (3.3V on VCC, GND on GND)
- Check SDA/SCL are connected to correct pins
- Check for loose/broken connections
- Verify I2C address (0x03 is typical, check with `i2cdetect`)

**4. Check I2C Address Match:**
- Default address: `0x03`
- Some boards use: `0x02`
- Web UI shows current address in "I2C Address (Hex)" field
- If address is wrong, sensor won't respond

**5. Check Pull-up Resistors:**
- I2C requires pull-up resistors on SDA/SCL (typically 4.7kΩ)
- Some development boards have them built-in
- If missing, add external 4.7kΩ resistors from SDA/SCL to 3.3V

## Testing Procedure

### Step 1: Physical Setup
```
ESP32-C3                AS3935
├─ 3.3V ──────────────→ VCC
├─ GND ──────────────→  GND
├─ GPIO21 (SDA) ─────→ SDA
├─ GPIO22 (SCL) ─────→ SCL
└─ GPIO23 (IRQ) ─────→ INT
```

### Step 2: Boot Device
- Connect serial monitor to see logs
- Power on device
- Look for: `I2C bus created on port 0, sda=21, scl=22`

### Step 3: Check Status in Web UI
- Open `http://device.local/`
- Look at "Sensor Status" field
- Should show either "connected" or "disconnected"

### Step 4: If Disconnected, Try Different Pins
- Try the default pins first (SDA=21, SCL=22)
- If those don't work, try: SDA=4, SCL=5 (GPIO4, GPIO5)
- Or check your board's pin diagram for available I2C pins

### Step 5: Save New Pins and Reboot
```
1. Change pins in web UI
2. Click "Save Pins"
3. Check response shows "saved":true
4. Reboot device (power cycle)
5. Check serial monitor for new pins in boot message
6. Check web UI sensor status
```

## Important Notes

### Pins Cannot Be Changed Live
- I2C bus creation is a one-time operation
- Cannot reconfigure running I2C bus with new pins
- **Must reboot after pin changes**
- This is a hardware limitation, not a software bug

### NVS Persistence
- Pins are saved to NVS (non-volatile storage)
- Survives power cycles
- Lost only if NVS is erased (factory reset)

### IRQ Pin
- IRQ pin is NOT critical for basic I2C communication
- Only needed if you want lightning detection interrupts
- Changing IRQ pin does NOT require reboot

### I2C Address
- Changing I2C address **does NOT require reboot**
- Address is queried at runtime, not cached
- Can change address and test immediately

## Troubleshooting Checklist

- [ ] Device boots without errors
- [ ] "I2C bus created" message shows expected pins
- [ ] Sensor physical connections are correct
- [ ] Pin numbers match your wiring (not default pins if not using them)
- [ ] Pull-up resistors are present on SDA/SCL
- [ ] AS3935 has 3.3V power
- [ ] I2C address in web UI matches sensor hardware (usually 0x03)
- [ ] After changing pins, device was rebooted
- [ ] Serial monitor shows new pins in boot message after reboot

## Web UI Response Messages

### After Saving Pins

**Success without restart needed:**
```json
{"status":"ok","saved":true}
```
(Means only IRQ pin changed, no reboot needed)

**Success with restart needed:**
```json
{"status":"ok","saved":true,"warning":"I2C pins require restart to apply"}
```
(Means SDA/SCL/port changed, MUST reboot)

**Error:**
```json
{"status":"error","code":261}
```
(NVS write failed - check flash storage)

## Common Issues

### "I2C bus created on port 0, sda=21, scl=22" but sensor disconnected
→ Sensor is not physically on pins 21/22
→ Change pins to match your wiring, save, and reboot

### Saves pins but still shows old pins in boot message
→ Device wasn't rebooted after saving
→ Must power cycle or reboot after saving pins

### Address keeps showing 0x03 even after saving
→ Check if address was actually saved (check NVS logs)
→ Verify form submitted (check browser console)
→ Try clearing browser cache and reload

### I2C bus creation fails with error
→ Check if pins are used by other hardware (UART, SPI, etc.)
→ Try different pins
→ Check pin numbers are valid for your MCU

---

**Key Takeaway**: I2C pins **require a device reboot** to take effect. Save pins, then reboot. New pins will be loaded on next boot.
