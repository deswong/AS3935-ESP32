# AS3935 I2C Pinout Connection Guide

## Overview

This project now uses **I2C (Inter-Integrated Circuit)** interface instead of SPI to communicate with the AS3935 lightning detector sensor. I2C is simpler, requiring only 2 data lines plus ground, making it easier to wire and configure.

---

## I2C Interface - Hardware Connections

### ESP32-C3 I2C Pins

The ESP32-C3 has two I2C interfaces:
- **I2C Port 0 (default)**: Can use flexible GPIO pins
- **I2C Port 1**: Alternative I2C interface

### Recommended Default Wiring

| Signal | ESP32-C3 GPIO | AS3935 Pin | Notes |
|--------|---------------|-----------|-------|
| **SDA** (Data) | GPIO 7 | SDA | I2C Data Line |
| **SCL** (Clock) | GPIO 4 | SCL | I2C Clock Line |
| **IRQ** | GPIO 0 | IRQ | Interrupt for lightning detection (optional) |
| **GND** | GND | GND | Ground |
| **VCC** | 3.3V | VCC | Power (typically 3.3V) |

### AS3935 I2C Slave Address

- **Default I2C Address**: `0x03`
- **Alternative**: Some modules may use `0x02` - check your sensor documentation

---

## Pin Configuration via Web UI

1. Navigate to the web interface at `http://<device-ip>/`
2. Go to the **"AS3935 Sensor - I2C Configuration"** section
3. Enter your GPIO pins:
   - **I2C Port**: `0` or `1` (default: `0`)
   - **SDA Pin**: GPIO number for data line (default: `21`)
   - **SCL Pin**: GPIO number for clock line (default: `22`)
   - **IRQ Pin**: GPIO number for interrupt (default: `0`, use `-1` to disable)
4. Click **"Save Pins"** to apply configuration
5. The sensor will reinitialize with the new pins

---

## GPIO Restrictions on ESP32-C3

⚠️ **Reserved GPIO Pins - DO NOT USE**:
- **GPIO 11-16**: Internal flash/PSRAM interface
- **GPIO 17-19**: Reserved
- **GPIO 20-21**: UART console (serial monitor)

✅ **Safe GPIO Pins for I2C**:
- **GPIO 0-10**: Available (caution: GPIO 0 is boot pin)
- **GPIO 22-27**: Available

---

## Common I2C Wiring Layouts

### Scenario 1: Standard Layout (Recommended)
```
ESP32-C3         AS3935 Module
━━━━━━━           ━━━━━━━━━━
GPIO 21 (SDA) ←→ SDA (pin 7)
GPIO 22 (SCL) ←→ SCL (pin 8)
GPIO 0 (IRQ)  ←→ IRQ (pin 5)
GND ──────────→ GND
3.3V ─────────→ VCC
```

### Scenario 2: Alternative Pins
```
ESP32-C3         AS3935 Module
━━━━━━━           ━━━━━━━━━━
GPIO 10 (SDA) ←→ SDA
GPIO 9 (SCL)  ←→ SCL
GPIO 8 (IRQ)  ←→ IRQ
GND ──────────→ GND
3.3V ─────────→ VCC
```

---

## I2C Pullup Resistors

I2C requires **pullup resistors** on SDA and SCL lines. Options:

### Option A: Use Internal Pullups (Recommended for Testing)
- The ESP32 I2C driver can enable internal pullups
- Automatically configured when I2C is initialized
- Works for short distances (< 1 meter)

### Option B: External Pullup Resistors (Recommended for Reliability)
```
VCC (3.3V)
  │
 ┌┴┐ 4.7kΩ resistor
 └┬┘
  ├─→ SDA (GPIO 21)
  
VCC (3.3V)
  │
 ┌┴┐ 4.7kΩ resistor
 └┬┘
  ├─→ SCL (GPIO 22)
```

**Resistor Values**:
- Standard: **4.7kΩ** (most common)
- High-capacitance lines: **2.2kΩ**
- Long cables: **1kΩ** (minimum)

---

## I2C Speed Configuration

Current configuration: **100 kHz** (standard I2C speed)

This is suitable for:
- Short cable runs (< 1 meter)
- Most AS3935 modules
- General reliability

Alternative speeds (if needed):
- **100 kHz**: Standard (default)
- **400 kHz**: Fast mode
- **1 MHz**: Fast-mode plus (requires driver support)

---

## Testing I2C Connection

### Method 1: Web UI
1. Go to web interface → **"AS3935 Sensor - I2C Configuration"**
2. Click **"View Details"** to see sensor status
3. A `PASSED` status indicates successful I2C communication

### Method 2: Serial Monitor
1. Connect device and open serial monitor
2. Look for boot messages:
   ```
   I (365) as3935: Configuring I2C port 0: SDA=21, SCL=22
   I (365) as3935: ✓ I2C bus initialized successfully
   I (365) as3935: ✓ I2C device added successfully (AS3935 @ 0x03, 100kHz)
   ```

### Method 3: HTTP API
```bash
# Check if I2C sensor is responding
curl http://<device-ip>/api/as3935/post

# Expected response if sensor working:
{"status":"PASSED","initial_reg0":"0x03",...}

# If sensor not present:
{"status":"FAILED","error":"Sensor not responding - all registers read 0x00",...}
```

---

## Troubleshooting

### Problem: I2C device not found / Not initialized

**Check**:
1. ✓ Verify GPIO pins are physically connected
2. ✓ Ensure no GPIO pin conflicts with other devices
3. ✓ Check power supply (3.3V) is stable
4. ✓ Verify AS3935 module is powered

**Solutions**:
- Try different GPIO pins (GPIO 9, 10, 19 are usually safe)
- Increase pullup resistors to 2.2kΩ
- Add a 100nF decoupling capacitor near AS3935 VCC

---

### Problem: Serial timeouts after WiFi connects

**This is FIXED in I2C version!** The I2C implementation doesn't have the blocking operations that caused timeouts in the SPI version.

---

### Problem: Sensor responds but status is "Not initialized"

This means:
- I2C communication is working
- Sensor is not responding with valid data
- Check: Is the AS3935 actually present and powered?

---

## Advantages of I2C over SPI

| Feature | SPI | I2C |
|---------|-----|-----|
| **Wires needed** | 5-6 (SCLK, MOSI, MISO, CS, GND) | 3-4 (SDA, SCL, GND, optional IRQ) |
| **Complexity** | Higher (4 data lines) | Lower (2 data lines) |
| **Wiring** | More error-prone | Simpler to debug |
| **Cable length** | 30 cm typical | 1+ meter possible |
| **Multiple devices** | Requires separate CS pin each | Single bus for many devices |
| **Speed** | Faster (up to 50 MHz) | Slower (up to 1 MHz) |

---

## Reverting to SPI (If Needed)

This project has completely migrated to I2C. If you need to revert to SPI:
1. Use a previous version from git history
2. Or contact support for legacy SPI branch

---

## References

- **I2C Specification**: [I2C Bus (Wikipedia)](https://en.wikipedia.org/wiki/I%C2%B2C)
- **AS3935 Datasheet**: Check your module's documentation for register map
- **ESP32 I2C Driver**: [ESP-IDF I2C Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2c.html)
- **PN5180**: Typically communicates via I2C at slave address 0x03

---

## Quick Start Checklist

- [ ] Wire SDA (GPIO 21) and SCL (GPIO 22) to AS3935 module
- [ ] Connect IRQ pin (GPIO 0) if using lightning detection
- [ ] Power the AS3935 with 3.3V
- [ ] Add pullup resistors (4.7kΩ) to SDA and SCL
- [ ] Access web UI and navigate to AS3935 configuration
- [ ] Click "Save Pins" to initialize
- [ ] Check sensor status - should show ✓ (connected)
- [ ] Test with POST endpoint or view serial logs
- [ ] Done! Your I2C lightning detector is ready

---

## Need Help?

- Check boot logs for I2C initialization messages
- Verify GPIO pins with multimeter (should see ~1.6V on I2C lines when idle)
- Test I2C scan with external tool (i2cdetect)
- Review register values via `/api/as3935/status` endpoint
