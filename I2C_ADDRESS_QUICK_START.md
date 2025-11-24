# I2C Address Configuration - Quick Start

## What's New?

✨ You can now configure the AS3935 I2C slave address via the web UI and save it permanently!

---

## Quick Start (30 seconds)

### 1. Open Web UI
```
http://<device-ip>/
```

### 2. Find "AS3935 Sensor - I2C Configuration" Section

### 3. Enter Address
In the **"I2C Address (Hex)"** field, enter:
- `0x03` (default) 
- `0x02` (alternate)
- `0x01` (alternate)

### 4. Click "Save Address"
✓ Address is saved permanently!

---

## Common Addresses

| What | Address | Format |
|---|---|---|
| Standard module | 0x03 | Hex |
| Alternate module | 0x02 | Hex |
| Another variant | 0x01 | Hex |
| Any value 0-255 | 0x00-0xFF | Hex |

---

## API Usage

### Get Current Address
```bash
curl http://<ip>/api/as3935/address/status
```

Response: `{"i2c_addr":"0x03","i2c_addr_dec":3}`

### Set New Address
```bash
curl -X POST http://<ip>/api/as3935/address/save \
  -H "Content-Type: application/json" \
  -d '{"i2c_addr": 2}'
```

Response: `{"ok":true,"i2c_addr":"0x02"}`

---

## Features

✅ **Hex & Decimal Support**
- Hex: `0x03` ← Recommended
- Decimal: `3`

✅ **Persistent Storage**
- Saved to NVS (survives reboot)

✅ **Validation**
- Auto-validates range 0x00-0xFF
- Rejects invalid values with error message

✅ **Dynamic Reconfiguration**
- I2C bus reinitializes automatically
- No reboot needed

---

## Troubleshooting

### Problem: "I2C init failed with new address"
**Solution:** Module not responding at that address
- Verify address in module datasheet
- Check power and connections
- Try default 0x03

### Problem: Address field shows 0x03 but my module uses 0x02
**Solution:** Just change it!
1. Enter `0x02`
2. Click "Save Address"
3. Done!

### Problem: Address keeps reverting
**Solution:** NVS corruption (rare)
- See RESET_NVS_GUIDE.md

---

## Files Modified

| File | Change |
|---|---|
| as3935.h | Added i2c_addr to config struct |
| as3935.c | Added NVS save/load functions |
| app_main.c | Registered HTTP endpoints |
| index.html | Added UI form fields |

---

## Documentation

📖 **Full Guide:** `I2C_ADDRESS_CONFIGURATION.md`
📋 **Implementation:** `I2C_ADDRESS_CONFIG_IMPLEMENTATION.md`

---

## Verification

### ✅ Check It's Working

1. Open web UI: `http://<ip>/`
2. Find "I2C Address (Hex)" field
3. Default should be `0x03`
4. Try changing to `0x02` and clicking "Save Address"
5. Should show success message ✓

### ✅ Check It Persisted

1. Reload the page (address should stay as 0x02)
2. Reboot device
3. Open web UI again
4. Address should still be 0x02

### ✅ Via API

```bash
curl http://<ip>/api/as3935/address/status | jq .
# Should return: {"i2c_addr":"0x02","i2c_addr_dec":2}
```

---

## Next Steps

1. ✅ Update firmware
2. ✅ Configure address via web UI
3. Connect AS3935 module
4. Test sensor communication
5. Monitor for lightning events

---

**Status:** ✅ Ready to use  
**Version:** 1.0  
**Date:** November 21, 2025
