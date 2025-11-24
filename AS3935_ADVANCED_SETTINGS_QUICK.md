# AS3935 Advanced Settings - Quick Reference

## What Can I Configure?

The **Advanced Settings** section lets you adjust the sensor's 9 registers individually:

| Register | Name | What It Does | Read/Write |
|----------|------|-------------|-----------|
| **0x00** | AFE Gain | Controls sensitivity | ✏️ Write |
| **0x01** | Threshold | Lightning detection level | ✏️ Write |
| **0x02** | Config | Noise/spike settings | ✏️ Write |
| **0x03** | Status | Event/interrupt status | 👁️ Read Only |
| **0x04-06** | Energy | Lightning strike strength | 👁️ Read Only |
| **0x07** | Calibration | RLC tuning | ✏️ Write |
| **0x08** | Distance | Strike distance (km) | 👁️ Read Only |

---

## Quick Fixes

### ❌ Getting false lightning alerts?
→ **Increase Register 0x01** from `0x22` to `0x30` or `0x40`
→ Higher numbers = harder to trigger

### ❌ Missing real lightning strikes?
→ **Decrease Register 0x01** from `0x22` to `0x10` or `0x14`
→ Lower numbers = easier to trigger

### ❌ Want more overall sensitivity?
→ **Increase Register 0x00** from `0x24` to `0x40`
→ Higher numbers = more gain

### ❌ Too much noise/interference?
→ **Decrease Register 0x00** from `0x24` to `0x18`
→ Lower numbers = less gain

---

## How to Use the UI

### Step 1: View Current Registers
```
Click "📥 Load All Registers"
```
This shows all 9 register values from your sensor right now.

### Step 2: Change a Register Value
```
Enter new value in the input box (hex or decimal)
Examples: 0x30 or 48 (same thing)
Click "Write" button
```
Changes apply **instantly** - no restart needed!

### Step 3: Try It Out
```
Wait a few minutes
Monitor if behavior changes
Adjust more if needed
```

### Step 4: Save for Future
```
When you find good settings:
Click "Save Current Register Values"
This saves to device memory (NVS)
Settings persist after reboot
```

---

## Value Formats

You can enter values in two ways:

**Hexadecimal** (preferred):
```
0xFF   = 255 decimal
0x30   = 48 decimal
0x00   = 0 decimal
```

**Decimal** (also works):
```
255    = 0xFF hex
48     = 0x30 hex
0      = 0x00 hex
```

**Tip**: Web UI auto-detects. Use whatever you prefer!

---

## Advanced: Custom Write

For power users, use the **"Custom Register Write"** section:

```
Register Address:  0x01 (the register to change)
Value:             0x30 (new value to write)
→ Click "Write Register"
```

---

## What Do These Values Mean?

### Register 0x00 - AFE Gain (Sensitivity)

**Default**: `0x24` (36 decimal)

**Scale**:
- `0x00` = Minimum sensitivity (ignore most signals)
- `0x24` = Default (balanced)
- `0xFF` = Maximum sensitivity (catch everything)

**When to adjust**:
- Indoors/noisy area? → Lower value (0x00-0x18)
- Outdoors/remote? → Higher value (0x30-0x60)
- Urban EM noise? → Much lower (0x00-0x10)

---

### Register 0x01 - Threshold (Detection Level)

**Default**: `0x22` (34 decimal)

**Scale**:
- `0x00` = Ultra-sensitive (lots of false alerts)
- `0x22` = Default (good balance)
- `0xFF` = Insensitive (misses real strikes)

**When to adjust**:
- Too many false positives? → Increase (0x30-0x50)
- Missing real strikes? → Decrease (0x10-0x18)

---

### Register 0x07 - Calibration (Tuning)

**Default**: `0x00` (no offset)

**Range**: `0x00` to `0x0F`

**When to adjust**: Rarely! Only if:
- Auto-calibration didn't work properly
- Running calibration sweep gave best results at offset value

**Typical case**: Leave at `0x00`

---

## Live Monitoring

After adjusting values, check Register 0x08 (Distance) to see if detection is working:

```
Load registers
Watch the Distance (0x08) value
When lightning strikes: value changes to 0-63 (kilometers away)
No change = sensor not detecting
```

---

## Common Configuration Profiles

### Profile: Balanced (Best for Most People)
```
0x00: 0x24
0x01: 0x22
0x07: 0x00
→ Good detection, few false positives
```

### Profile: High Sensitivity (Outdoor/Remote Areas)
```
0x00: 0x40
0x01: 0x10
0x07: 0x00
→ Detects weak/distant strikes
```

### Profile: Low Sensitivity (Urban/Indoor)
```
0x00: 0x18
0x01: 0x40
0x07: 0x00
→ Reduces noise, only detects strong nearby strikes
```

### Profile: Testing (Very Sensitive)
```
0x00: 0xFF
0x01: 0x00
0x07: 0x00
→ Catches EVERYTHING (use for diagnostics only!)
```

---

## Troubleshooting

### Q: I clicked Write but nothing changed?
**A**: Check sensor is working:
1. Click "View Details" in I2C Configuration section
2. Should show Register values (0x00, 0x01, etc.)
3. If shows 0x00 for all = sensor not responding

### Q: Value changed but behavior didn't?
**A**: Normal! Some registers have subtle effects:
- Try larger changes (0x20 step, not 0x01 step)
- Wait 2-3 minutes between tests
- Restart sensor for major recalibrations

### Q: How do I reset to defaults?
**A**: Use these values:
```
0x00: 0x24
0x01: 0x22
0x07: 0x00
```

### Q: Can I break something by wrong register values?
**A**: No! Worst case = no detection. Can always write new values to fix.

---

## Pro Tips

1. **One change at a time**: Adjust one register, test, then adjust another
2. **Document what works**: Note down good settings for your location
3. **Test in same conditions**: Thunder storm, not just lab testing
4. **Check Distance register**: 0x08 should update during strikes (not stuck at 0xFF)
5. **Backup good settings**: Screenshot or write down working values
6. **Use APIs**: See `AS3935_API_ENDPOINTS.md` for automation/integration

---

## Need Help?

Check these resources:
- **Full Register Guide**: See `AS3935_REGISTER_CONFIGURATION.md`
- **Sensor Not Responding?**: See `URGENT_I2C_NACK_ERROR.md`
- **API Integration**: See `AS3935_API_ENDPOINTS.md`
- **I2C Configuration**: See `I2C_ADDRESS_CONFIGURATION.md`

---

*Last Updated: November 2025*
*Quick Reference v1.0*
