# URGENT: Fix I2C NACK Error 264 - Quick Action

## 🚨 The Issue

Your AS3935 module is not responding at address **0x03** (the default).

```
I2C read reg 0x00 failed: error 264
I2C transaction unexpected nack detected
```

**This means:** Device not found at 0x03, or wiring issue.

---

## ⚡ Quick Fix (5 Minutes)

### Option 1: Try Different Address (Most Likely Fix)

Your module might be at **0x02** or **0x01** instead of 0x03.

**Via Web UI:**
1. Open: `http://<your-device-ip>/`
2. Scroll to: **"AS3935 Sensor - I2C Configuration"**
3. Find: **"I2C Address (Hex)"** field
4. Change value to: `0x02`
5. Click: **"Save Address"** button
6. Wait 3 seconds
7. Check if error disappears ✓

**If error still there, try:**
- `0x01`
- `0x03` (original - to confirm it was wrong)

---

### Option 2: Via cURL (If Web UI Not Working)

```bash
# Try address 0x02
curl -X POST http://<your-device-ip>/api/as3935/address/save \
  -H "Content-Type: application/json" \
  -d '{"i2c_addr": 2}'

# Wait 3 seconds, then test
sleep 3
curl http://<your-device-ip>/api/as3935/post
```

**If successful, response will NOT contain error 264.**

---

## 🔍 Verify It Worked

Check boot log for this message:
```
✓ I2C device added successfully (AS3935 @ 0x02, 100kHz)
```

Or via API:
```bash
curl http://<your-device-ip>/api/as3935/address/status
# Should return the address you just set (e.g., 0x02)
```

---

## ❌ If That Didn't Work

### Check #1: Does Module Have Power? 🔌
- Look for **LED on module**
- If LED is **OFF** → Power not connected
  - Check 3.3V wire
  - Check GND wire
  - Check power jumper on module

### Check #2: Are Pins Connected? 🔗
Verify these connections with visual inspection:
- **GPIO 7 (white/yellow wire?)** → Module **SDA** pin
- **GPIO 4 (red/green wire?)** → Module **SCL** pin
- **GND (black wire)** → Module **GND** pin
- **3.3V (red wire)** → Module **VCC/3V3** pin

**Most common mistake:** SDA and SCL reversed!

### Check #3: Check Boot Log
```bash
idf.py monitor
```

Look for:
```
Loaded I2C pins from NVS: port=0 sda=7 scl=4 irq=0
Loaded I2C address from NVS: 0x03
✓ I2C bus initialized successfully
✓ I2C device added successfully (AS3935 @ 0x03, 100kHz)
```

If pins are different from above, they might be wrong!

---

## 📋 Addresses to Try (In Order)

1. **Try first:** `0x03` ← Default (already failed)
2. **Try second:** `0x02` ← Most common alternate
3. **Try third:** `0x01` ← Less common variant

Just change the address and try again.

---

## 🎯 Success Checklist

Once error 264 is gone:
- [ ] No more "NACK detected" messages
- [ ] No more "error 264" messages
- [ ] API response shows sensor data (not error)
- [ ] Boot log shows correct address (e.g., 0x02)

---

## 🆘 Still Not Working?

### Debug via Detailed Guide

See: **`I2C_ADDRESS_DIAGNOSIS.md`**

Covers:
- Power troubleshooting
- Wiring verification
- Module variant detection
- Step-by-step diagnosis

### Common Issues Table

| Error | Cause | Fix | Time |
|-------|-------|-----|------|
| NACK at 0x03 | Wrong address | Try 0x02, 0x01 | 2 min |
| All NACK | Module not powered | Check 3.3V | 5 min |
| All NACK | Wrong pins | Verify GPIO 7,4 | 10 min |
| All NACK | SDA/SCL reversed | Swap connections | 5 min |

---

## 📞 Need Help?

Read these in order:
1. **I2C_ADDRESS_DIAGNOSIS.md** ← Start here
2. **I2C_TROUBLESHOOTING.md** ← More details
3. **I2C_PINOUT_GUIDE.md** ← Wiring reference

---

## Summary

**Most likely:** Address is **0x02**, not 0x03
**Action:** Change address in web UI or via API
**Time:** < 5 minutes
**Success:** Error 264 disappears

**Try it now!** 👆

---

**Created:** November 21, 2025  
**For:** I2C NACK Error 264  
**Status:** Action required
