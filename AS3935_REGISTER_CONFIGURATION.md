# AS3935 Sensor Register Configuration Guide

## Overview

The AS3935 Lightning Detector has nine main registers that control the sensor's behavior and provide status/data information. This guide explains each register and how to configure them through the web UI or API.

## Web UI Configuration

### Method 1: Graphical Register Panel

1. Navigate to the **"⚙️ AS3935 Sensor - Advanced Settings"** section
2. Click **"📥 Load All Registers"** to fetch current values from the sensor
3. Each register will display:
   - Register name and description
   - Current hex value
   - Input field to enter new value (read-write registers only)
4. Enter new value in hex (0xFF) or decimal (255) format
5. Click **"Write"** to apply immediately to the sensor
6. Click **"Save Current Register Values"** to persist to NVS

### Method 2: Custom Register Write

1. Scroll to **"⚙️ Custom Register Write (Advanced)"** section
2. Enter:
   - **Register Address**: Hex (0x00) or Decimal (0-255)
   - **Value**: Hex (0xFF) or Decimal (255)
3. Click **"Write Register"**
4. Value applies immediately; no restart needed

## AS3935 Registers Reference

### Register 0x00 - AFE Gain (Read/Write)

**Purpose**: Controls the receiver amplifier front-end gain

**Default Value**: 0x24 (36 decimal)

**Typical Range**: 0x00 - 0xFF

**Effect on Sensitivity**:
- Lower values = lower gain = less sensitive
- Higher values = higher gain = more sensitive
- Use if you're getting false triggers (increase gain) or missing real strikes

**Common Tuning**:
- 0x00 - 0x10: Very low sensitivity (use in urban noisy environments)
- 0x10 - 0x30: Medium-low sensitivity (recommended for indoor)
- 0x30 - 0x60: Medium sensitivity (recommended starting point)
- 0x60 - 0xFF: High sensitivity (outdoor/remote areas)

**Example**:
```
Lower interference:   0x00 (minimum)
Standard setting:     0x24 (default)
Higher sensitivity:   0xFF (maximum)
```

---

### Register 0x01 - Threshold/Signal (Read/Write)

**Purpose**: Sets the lightning signal strength threshold for detection

**Default Value**: 0x22 (34 decimal)

**Typical Range**: 0x00 - 0xFF

**Effect on Detection**:
- Lower threshold = easier to detect weak signals = more false positives
- Higher threshold = require stronger signals = fewer false positives
- Typical sweet spot is 0x20-0x40

**Tuning Strategy**:
- Start at 0x22 (default)
- If getting many false triggers: increase to 0x30-0x40
- If missing real strikes: decrease to 0x10-0x20
- Adjust in 0x04-0x08 steps for fine-tuning

**Common Values**:
```
Noisy environment:    0x30-0x40 (higher)
Standard outdoor:     0x22 (default)
Clear environment:    0x10-0x20 (lower)
```

---

### Register 0x02 - Configuration (Read/Write)

**Purpose**: General sensor configuration and interrupt setup

**Default Value**: 0x00 (0 decimal)

**Bit Fields** (advanced):
- Bits 0-2: Noise floor level
- Bits 3-5: Spike rejection level
- Bits 6-7: Reserved

**Common Values**:
```
0x00 - Default/recommended
0x02 - Increased noise floor
0x04 - Increased spike rejection
```

**Note**: Most users don't need to modify this register; focus on 0x00 and 0x01

---

### Register 0x03 - Status (Read-Only)

**Purpose**: Reports the current state of the sensor and interrupt status

**Typical Values**:
```
0x00 - No event occurred
0x01 - Lightning strike detected (far field)
0x04 - Lightning strike detected (mid field)
0x08 - Lightning strike detected (close field)
0x10 - Interrupt triggered
```

**When to Check**:
- After detecting lightning event
- To verify sensor is responding
- For event classification (distance estimation)

---

### Register 0x04 - Lightning Energy MSB (Read-Only)

**Purpose**: Most significant byte of lightning energy measurement

**Meaning**: 
- Higher value = stronger lightning strike energy
- 3-byte value: combine 0x04 (MSB), 0x05 (MID), 0x06 (LSB)
- Typical range: 0x000000 - 0xFFFFFF

**Calculation**:
```
Energy = (R0x04 << 16) | (R0x05 << 8) | R0x06
```

**Use Case**: Relative lightning intensity (not actual energy in Joules)

---

### Register 0x05 - Lightning Energy Middle (Read-Only)

**Purpose**: Middle byte of energy measurement

**See Register 0x04** for details on the 3-byte energy value

---

### Register 0x06 - Lightning Energy LSB (Read-Only)

**Purpose**: Least significant byte of energy measurement

**See Register 0x04** for details on the 3-byte energy value

---

### Register 0x07 - Calibration (Read/Write)

**Purpose**: Tunes the RLC oscillator frequency for accurate operation

**Default Value**: Depends on crystal frequency (typically 0x00-0x0F)

**Adjustment Range**: 0x00 - 0x0F (4-bit value)

**When to Use**:
- Automatic calibration is usually done during initialization
- Manual adjustment rarely needed
- Only modify if sensor is not detecting correctly after trying 0x00 and 0x01

**Typical Values**:
```
0x00 - No calibration offset
0x01 - +1 step
0x0F - +15 steps (maximum)
```

**Warning**: Incorrect calibration can worsen detection. Most installations use default.

---

### Register 0x08 - Distance (Read-Only)

**Purpose**: Estimated distance to latest lightning strike in kilometers

**Typical Range**: 0 - 63 km

**Accuracy**:
- Only valid after lightning event detected
- Rough estimate based on return signal timing
- Accuracy: ±5-10 km typical

**Interpretation**:
```
0x00 - Lightning within 5 km (overhead)
0x05 - Lightning ~5 km away
0x20 - Lightning ~32 km away (edge of detection range)
0x3F - Lightning at maximum distance (~63 km)
0xFF - No distance data available
```

---

## Quick Configuration Scenarios

### Scenario 1: Too Many False Triggers

**Problem**: Sensor detecting "lightning" when none is occurring (false positives)

**Solution**:
1. Increase Register 0x01 (Threshold) from 0x22 to 0x30
2. If still triggering, try 0x40
3. Verify Register 0x00 (AFE Gain) is 0x24 (default) or lower

**Steps**:
1. Load registers
2. Find Threshold (0x01)
3. Change value to 0x30
4. Click Write
5. Test for 5 minutes
6. Adjust further if needed

---

### Scenario 2: Missing Real Lightning Strikes

**Problem**: Real lightning occurring but sensor not detecting

**Solution**:
1. Decrease Register 0x01 (Threshold) from 0x22 to 0x10
2. Verify antenna connection is secure
3. Try increasing Register 0x00 (AFE Gain) from 0x24 to 0x40

**Steps**:
1. Load registers
2. Try adjusting Threshold (0x01) downward to 0x10-0x14
3. If still missing strikes, increase AFE Gain (0x00) to 0x30-0x40
4. Test during next storm

---

### Scenario 3: Wrong Distance Readings

**Problem**: Lightning distance shows unrealistic values (0 km or 63 km every time)

**Solution**:
1. Verify sensor is responding: Check Register 0x08 updates after events
2. Run self-test via "View Details" button
3. Try recalibrating: Use /api/as3935/calibrate endpoint
4. Check antenna for damage

**Note**: Distance is approximate; focus on whether strike occurred, not exact distance

---

### Scenario 4: Optimal Sensitivity for Outdoor Monitoring

**Problem**: Need balanced detection for outdoor storm monitoring

**Recommended Settings**:
```
Register 0x00 (AFE Gain):     0x24 (default)
Register 0x01 (Threshold):    0x22 (default)
Register 0x07 (Calibration):  0x00 (default)
```

If detecting too many weak signals (false positives):
```
Register 0x01 (Threshold):    0x30 (increase to 0x3C for very noisy environments)
```

---

## API Endpoints for Register Configuration

### Read a Single Register

```bash
GET /api/as3935/register/read?reg=0x00
```

**Response**:
```json
{
  "reg": "0x00",
  "value": "0x24",
  "value_dec": 36
}
```

---

### Write a Single Register

```bash
POST /api/as3935/register/write
Content-Type: application/json

{
  "reg": 0,
  "value": 0x30
}
```

**Response**:
```json
{
  "ok": true,
  "reg": "0x00",
  "value": "0x30",
  "value_dec": 48
}
```

**Note**: Changes apply immediately to sensor; no restart required

---

### Read All Registers

```bash
GET /api/as3935/registers/all
```

**Response**:
```json
{
  "status": "ok",
  "registers": {
    "0x00": "0x24",
    "0x01": "0x22",
    "0x02": "0x00",
    "0x03": "0x00",
    "0x04": "0x00",
    "0x05": "0x00",
    "0x06": "0x00",
    "0x07": "0x00",
    "0x08": "0xFF"
  }
}
```

---

### Save Configuration to NVS

```bash
POST /api/as3935/config/save
Content-Type: application/json

{
  "config": "{\"0x00\":\"0x24\",\"0x01\":\"0x22\",...}"
}
```

This persists the current register values so they load on device restart.

---

## Troubleshooting Register Configuration

### Issue: Changes Don't Persist After Restart

**Solution**: Click "Save Current Register Values" button to persist to NVS

### Issue: Can't Write to Read-Only Registers

**Expected Behavior**: Registers 0x03-0x06 and 0x08 are read-only per AS3935 design

**Alternative**: These values update automatically after lightning events

### Issue: "Failed to write register" Error

**Causes**:
1. I2C communication error - check sensor connection
2. Invalid register address (must be 0x00-0x08)
3. Sensor not responding - verify I2C address in pin configuration

**Fix**:
1. Run "/api/as3935/post" diagnostic test
2. Verify I2C address and pins are correct
3. Check physical wiring and power

### Issue: Sensor Completely Unresponsive

**Diagnostic Steps**:
1. Check Register 0x00 value - if 0x00, sensor not responding
2. Verify I2C address (try 0x01, 0x02, 0x03)
3. Look at GPIO 4 (SCL) and GPIO 7 (SDA) connections
4. Check 3.3V power supply
5. Reboot device

---

## Advanced: Batch Register Configuration via JSON

Use the `/api/as3935/params` endpoint to apply multiple register values at once:

```bash
POST /api/as3935/params
Content-Type: application/json

{
  "config": "{
    \"0x00\": 48,
    \"0x01\": 32,
    \"0x07\": 0
  }"
}
```

This writes all three registers in a single request and is useful for switching profiles.

---

## Best Practices

1. **Start with defaults**: 0x00=0x24, 0x01=0x22 work for most situations
2. **Adjust one at a time**: Change one register, test for 5-10 minutes
3. **Save working configurations**: Use "Save Current Register Values" for profiles
4. **Document your settings**: Note what values worked for your environment
5. **Monitor the distance register**: Register 0x08 should update with each strike
6. **Check status regularly**: Use "Load All Registers" to verify sensor health

---

## Reference: Typical Operating Configurations

### Configuration A: Balanced (Default)
```
0x00: 0x24  (Medium gain)
0x01: 0x22  (Medium threshold)
0x07: 0x00  (No calibration offset)
Result: Detects most strikes without excessive false positives
```

### Configuration B: High Sensitivity (Outdoor/Remote)
```
0x00: 0x40  (High gain)
0x01: 0x10  (Low threshold)
0x07: 0x00  (No calibration offset)
Result: Detects distant and weak lightning
```

### Configuration C: Low Sensitivity (Urban/Indoor)
```
0x00: 0x18  (Low gain)
0x01: 0x40  (High threshold)
0x07: 0x00  (No calibration offset)
Result: Ignores most noise, only detects strong nearby strikes
```

### Configuration D: Auto-Calibrated (Best)
```
Run calibration via /api/as3935/calibrate endpoint
System automatically finds optimal 0x00, 0x01, 0x07 values
Persisted to NVS for future boots
Result: Optimal balance for your specific installation
```

---

## Support & Next Steps

- **Read Register Values**: Click "📥 Load All Registers" to see current sensor state
- **View Guide**: Click "View Register Guide" for quick reference
- **API Documentation**: See `AS3935_API_ENDPOINTS.md` for REST endpoints
- **Troubleshooting**: See `URGENT_I2C_NACK_ERROR.md` if sensor not responding

---

*Last Updated: November 2025*
*AS3935 Hardware Reference: AMS AS3935 Datasheet Rev 3.0*
