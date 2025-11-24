# Live Status Display - Visual Quick Guide

## 🎬 What You'll See on Config Page

### WORKING SENSOR (Green) ✅

```
════════════════════════════════════════════════════
                 Sensor Status            🔄 Refresh
════════════════════════════════════════════════════
✓ Sensor Active & Responding
Register Status:
R0 (System):   0x24
R1 (Config):   0x22
R3 (Lightning): 0x00
R8 (Distance): 0x00
════════════════════════════════════════════════════
```

**Status:** 🟢 **GREEN** (LEFT BORDER)  
**Meaning:** Sensor responding perfectly!  
**Update:** Every 3 seconds automatically  

---

### NOT RESPONDING (Red) ❌

```
════════════════════════════════════════════════════
                 Sensor Status            🔄 Refresh
════════════════════════════════════════════════════
✗ Sensor Not Responding
Troubleshooting:
• Check I2C address (try 0x01, 0x02, 0x03)
• Verify GPIO 7 (SDA) and GPIO 4 (SCL) wiring
• Confirm 3.3V power and GND connection
• See URGENT_I2C_NACK_ERROR.md for help
════════════════════════════════════════════════════
```

**Status:** 🔴 **RED** (LEFT BORDER)  
**Meaning:** Sensor not found or not responding  
**Action:** Follow the troubleshooting tips shown  

---

## 📊 Register Values Explained

### What Each Register Tells You

```
R0 (System Register)
├─ Value: 0x24
├─ Meaning: System initialized and ready
└─ Normal: Always same when powered

R1 (Configuration Register)
├─ Value: 0x22
├─ Meaning: Sensor configured with settings
└─ Normal: Should stay same between readings

R3 (Lightning Detection)
├─ Value: 0x00 (no strike)
├─ Value: != 0x00 (lightning detected!)
└─ Changes: When lightning strikes nearby

R8 (Distance Estimation)
├─ Value: 0x00 (no data)
├─ Value: 0x01-0x3F (distance estimate)
└─ Updates: When lightning detected
```

---

## 🎯 Reading the Status

### Green Status = All Good ✅

**What it means:**
- ✅ I2C communication working
- ✅ Sensor found at configured address
- ✅ Sensor responding to register reads
- ✅ Ready to detect lightning!

**Register values shown:**
- Live values from sensor
- Update every 3 seconds
- Change when lightning strikes

---

### Red Status = Problem Found ❌

**What it means:**
- ❌ Sensor not responding at this address
- ❌ Wiring issue or power problem
- ❌ Wrong I2C address configured

**What to do:**
- Try different I2C address (0x01, 0x02, 0x03)
- Check wiring (GPIO 7, 4, GND, 3.3V)
- Verify module has power (LED on)
- See detailed guide for more help

---

## 🔄 Auto-Refresh Explained

**Every 3 seconds, the status automatically:**
1. Queries sensor at current address
2. Reads all 4 registers
3. Updates the display
4. No page reload needed!

**Example Timeline:**
```
00:00 → Page loads, status shows GREEN
00:03 → Status refreshes, R0=0x24 (unchanged, normal)
00:06 → Status refreshes again
00:09 → Lightning strikes! R3 changes to 0x03 ⚡
00:12 → Status updates showing new R3 value!
```

---

## 🖱️ Manual Refresh

**Click the "🔄 Refresh" button anytime to:**
- Immediately check sensor status
- Get fresh register values
- Doesn't interrupt auto-refresh

---

## 📍 Where to Find It

```
Config Page
    ↓
AS3935 Sensor Section
    ↓
STATUS BOX (Top, right above GPIO pins)
    ↓
Shows: Green or Red status with details
    ↓
Auto-updates every 3 seconds!
```

---

## ⚡ Live Updates Example

### Scenario: Sensor Working, Lightning Detected

**Before lightning:**
```
✓ Sensor Active & Responding
R0: 0x24  (System OK)
R1: 0x22  (Config OK)
R3: 0x00  (No lightning)
R8: 0x00  (No distance)
```

**After lightning strike (automatic update in 3 sec):**
```
✓ Sensor Active & Responding
R0: 0x24  (System OK)
R1: 0x22  (Config OK)
R3: 0x03  ← CHANGED! Lightning detected!
R8: 0x08  ← CHANGED! Distance: ~8km
```

No page reload needed - you see it live! 🔴⚡

---

## 🎨 Visual Status Indicators

### Color System

```
GREEN LEFT BORDER
├─ Means: Sensor is responding
├─ Action: Monitor and enjoy!
└─ Risk: None

RED LEFT BORDER
├─ Means: Sensor not found
├─ Action: Troubleshoot
└─ Risk: Cannot detect lightning
```

---

## 💻 Technical

### What Happens Behind Scenes

1. **Page loads** → JavaScript calls `/api/as3935/status`
2. **API responds** with sensor data
3. **Data shown** in status box (Green or Red)
4. **Timer starts** → Will call API again in 3 seconds
5. **Repeats** until user leaves page

### API Response Format

```json
{
  "initialized": true,
  "r0": "0x24",
  "r1": "0x22",
  "r3": "0x00",
  "r8": "0x00"
}
```

---

## ✅ Verification Checklist

- [ ] Web page loads
- [ ] Status box visible
- [ ] Status is GREEN or RED (not gray)
- [ ] Register values shown if GREEN
- [ ] Status changes after 3 seconds
- [ ] Click "Refresh" button works
- [ ] No error messages in browser console

---

## 🎓 Summary

| Feature | Benefit |
|---------|---------|
| **Live Display** | Know immediately if sensor works |
| **Auto-Refresh** | See updates without reloading |
| **Register Values** | Monitor sensor internals |
| **Color Coded** | Easy to see status at a glance |
| **Troubleshooting** | Help text shown automatically |

---

## 🚀 You're Ready!

The enhanced UI now shows:
- ✅ Real-time sensor status
- ✅ Live register values updating
- ✅ Automatic refresh every 3 seconds
- ✅ Clear visual feedback
- ✅ Built-in troubleshooting help

**Rebuild and flash to see it in action!** 🎉

---

**Status:** ✅ Live Display Ready  
**Auto-Refresh:** Every 3 seconds  
**Update Frequency:** Real-time  
**Visual Feedback:** Color-coded status
