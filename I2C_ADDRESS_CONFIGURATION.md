# AS3935 I2C Address Configuration Guide

## Overview

The AS3935 Lightning Detector can operate on different I2C slave addresses depending on the hardware module configuration. This guide explains how to configure and change the I2C address through the web UI.

---

## Default I2C Address

**Default:** `0x03` (decimal: 3)

This is the standard address for most AS3935 modules. However, some modules may have hardware address pins that allow selecting different addresses.

---

## Supported I2C Addresses

The I2C address can be any value from **0x00 to 0xFF** (0-255 decimal), but the AS3935 sensor typically uses:

| Address (Hex) | Address (Dec) | Module Variant |
|---|---|---|
| `0x03` | 3 | Standard (Default) |
| `0x02` | 2 | Alternate (some modules) |
| `0x01` | 1 | Alternate (some modules) |

> **Note:** Check your AS3935 module datasheet or documentation to determine which address your module uses.

---

## How to Configure Address via Web UI

### 1. Access the Web Interface
```
http://<device-ip>/
```

### 2. Find the "AS3935 Sensor - I2C Configuration" Section

You'll see a form with the following fields:
- I2C Port (0 or 1)
- SDA Pin (Data)
- SCL Pin (Clock)
- IRQ Pin
- **I2C Address (Hex)** ← Address configuration field

### 3. Enter the I2C Address

The address field accepts both **hexadecimal** and **decimal** formats:

**Hexadecimal (recommended):**
```
0x03   (default)
0x02
0x01
```

**Decimal:**
```
3      (equivalent to 0x03)
2      (equivalent to 0x02)
1      (equivalent to 0x01)
```

### 4. Click "Save Address" Button

- The device will:
  1. Validate the address (0x00-0xFF range)
  2. Save it to NVS (non-volatile storage)
  3. Re-initialize the I2C bus with the new address
  4. Show a confirmation message

### 5. Verify the Address Changed

If successful, you'll see:
```
✓ AS3935 I2C address saved successfully! (0x03)
```

---

## Persistent Storage

✅ **Address is automatically saved to NVS** (non-volatile storage)
- Survives device reboots
- Survives firmware updates
- Can be reset via NVS reset procedure

---

## API Endpoints

### Get Current Address
```bash
curl http://<device-ip>/api/as3935/address/status
```

Response:
```json
{
  "i2c_addr": "0x03",
  "i2c_addr_dec": 3
}
```

### Set I2C Address
```bash
curl -X POST http://<device-ip>/api/as3935/address/save \
  -H "Content-Type: application/json" \
  -d '{"i2c_addr": 0x02}'
```

Response (success):
```json
{
  "ok": true,
  "i2c_addr": "0x02"
}
```

Response (error):
```json
{
  "ok": false,
  "error": "Address must be 0x00-0xFF (0-255)"
}
```

---

## Troubleshooting

### Error: "I2C init failed with new address"

**Possible causes:**
1. **Wrong address:** The AS3935 module isn't responding at the configured address
2. **Device not powered:** Verify 3.3V power to the module
3. **Wiring issue:** Check GPIO 7 (SDA) and GPIO 4 (SCL) connections
4. **Incorrect pins:** Verify I2C pins are correctly configured

**Solution:**
1. Verify your AS3935 module datasheet for correct address
2. Try the default address (0x03)
3. Check physical connections
4. Test with `curl` to see I2C error code:
   ```bash
   curl http://<device-ip>/api/as3935/post
   ```

### Error: "Address must be 0x00-0xFF"

**Cause:** Address value is outside the valid range

**Solution:** Use values 0-255 (0x00-0xFF in hex)

### Address Reverted After Reboot

**Cause:** NVS wasn't properly saved, or NVS storage corrupted

**Solution:** 
1. Set address again via web UI
2. If problem persists, reset NVS: See `RESET_NVS_GUIDE.md`

---

## Finding the Correct Address for Your Module

### Method 1: Module Datasheet
Check your AS3935 module documentation for the I2C slave address.

### Method 2: Address Pin Configuration
Some modules have address select pins. Check for:
- **AD0 pin:** Ground = address 0x01, VCC = address 0x03
- **AD1 pin:** Additional address variants

### Method 3: Scan All Addresses
If you have an I2C scanner tool, run it to find active devices:
```bash
# Using i2cdetect (if available on your network)
# Will show all active I2C addresses on the bus
```

### Method 4: Try Common Addresses
Test the most common AS3935 addresses:
1. `0x03` (default) ← Start here
2. `0x02` (common alternate)
3. `0x01` (less common)

---

## Multi-Module Setup (Future)

Currently, the firmware supports **one AS3935 module**. Supporting multiple modules on different addresses would require:

1. Modifying the driver to maintain multiple device handles
2. Updating the web UI to select which module to configure
3. Creating per-module API endpoints

This can be implemented in a future version if needed.

---

## Verifying Configuration

### Via Web UI
1. Load the web interface
2. The "I2C Address (Hex)" field shows the current address

### Via API
```bash
curl http://<device-ip>/api/as3935/address/status | jq .
```

### Via Log Output (Serial Monitor)
When the device boots, it logs the loaded configuration:
```
Loaded I2C address from NVS: 0x03
✓ I2C device added successfully (AS3935 @ 0x03, 100kHz)
```

---

## Factory Reset to Default Address

To reset address to default (0x03):

**Via Web UI:**
1. Enter `0x03` in the "I2C Address (Hex)" field
2. Click "Save Address"

**Via API:**
```bash
curl -X POST http://<device-ip>/api/as3935/address/save \
  -H "Content-Type: application/json" \
  -d '{"i2c_addr": 3}'
```

**Via NVS Reset:**
See `RESET_NVS_GUIDE.md` for complete NVS reset instructions.

---

## Technical Details

### Code Locations

**NVS Functions (as3935.c):**
- `as3935_save_addr_nvs(int i2c_addr)` - Save address to NVS
- `as3935_load_addr_nvs(int *i2c_addr)` - Load address from NVS

**HTTP Handlers (as3935.c):**
- `as3935_addr_status_handler()` - GET `/api/as3935/address/status`
- `as3935_addr_save_handler()` - POST `/api/as3935/address/save`

**Data Structure (as3935.h):**
```c
typedef struct {
    int i2c_port;    // I2C port (0 or 1)
    int sda_pin;     // SDA pin number
    int scl_pin;     // SCL pin number
    int irq_pin;     // IRQ pin number
    int i2c_addr;    // I2C slave address (added in address config update)
} as3935_config_t;
```

**Initialization Flow:**
```
as3935_init_from_nvs()
├─ Load pins from NVS
├─ Load address from NVS
├─ Create config struct with address
└─ Call as3935_init(&cfg) with address
    └─ i2c_master_bus_add_device() uses cfg->i2c_addr
```

### NVS Keys

| Key | Type | Default | Range |
|---|---|---|---|
| `i2c_port` | i32 | 0 | 0-1 |
| `sda` | i32 | 21 | 0-46 |
| `scl` | i32 | 22 | 0-46 |
| `irq` | i32 | 0 | -1 to 46 |
| `i2c_addr` | i32 | 3 | 0-255 |

All stored in NVS namespace: `"as3935"`

---

## Next Steps

1. ✅ Configure GPIO pins (SDA, SCL, IRQ)
2. ✅ **Configure I2C address (this guide)**
3. Connect physical AS3935 module
4. Test sensor communication via API
5. Monitor for lightning events

---

## Support & Questions

For issues or questions about I2C address configuration:

1. Check `I2C_TROUBLESHOOTING.md` for common errors
2. Review `BUILD_AND_TEST_GUIDE.md` for testing procedures
3. Verify module datasheet for address information
4. Enable debug logging by checking boot messages

---

**Status:** ✅ Ready to use  
**Last Updated:** November 21, 2025  
**Version:** 1.0
