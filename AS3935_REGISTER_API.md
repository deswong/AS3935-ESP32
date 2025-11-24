# AS3935 Register Configuration API Endpoints

## Overview

These endpoints allow programmatic access to read and modify AS3935 sensor register values. All responses are JSON format. Changes to registers apply immediately to the sensor.

---

## Endpoints

### 1. Read Single Register

**Endpoint**: `GET /api/as3935/register/read`

**Parameters**:
- `reg` (required): Register address in hex or decimal
  - Example: `?reg=0x00` or `?reg=0` or `?reg=1`

**Response**:
```json
{
  "reg": "0x00",
  "value": "0x24",
  "value_dec": 36
}
```

**Examples**:
```bash
# Read AFE Gain register (0x00)
curl "http://192.168.1.100/api/as3935/register/read?reg=0x00"

# Read Threshold register (0x01)
curl "http://192.168.1.100/api/as3935/register/read?reg=1"

# Read Distance register (0x08)
curl "http://192.168.1.100/api/as3935/register/read?reg=8"
```

**Error Response**:
```json
{"error": "Failed to read register"}
```

**HTTP Status**: `200 OK` (on success), `400 Bad Request` (on failure)

---

### 2. Write Single Register

**Endpoint**: `POST /api/as3935/register/write`

**Method**: POST

**Content-Type**: `application/json`

**Request Body**:
```json
{
  "reg": 0,
  "value": 48
}
```

**Parameters**:
- `reg` (required): Register address (0-255 or 0x00-0xFF)
- `value` (required): Value to write (0-255 or 0x00-0xFF)

**Response** (Success):
```json
{
  "ok": true,
  "reg": "0x00",
  "value": "0x30",
  "value_dec": 48
}
```

**Response** (Failure):
```json
{"error": "Failed to write register"}
```

**Examples**:
```bash
# Write 0x30 to AFE Gain register (0x00)
curl -X POST http://192.168.1.100/api/as3935/register/write \
  -H "Content-Type: application/json" \
  -d '{"reg":"0x00","value":"0x30"}'

# Write decimal value to Threshold register (0x01)
curl -X POST http://192.168.1.100/api/as3935/register/write \
  -H "Content-Type: application/json" \
  -d '{"reg":1,"value":48}'

# Write maximum gain for testing (0x00 = 0xFF)
curl -X POST http://192.168.1.100/api/as3935/register/write \
  -H "Content-Type: application/json" \
  -d '{"reg":"0x00","value":"0xFF"}'
```

**HTTP Status**: `200 OK` (on success), `400 Bad Request` (on invalid input)

**Format Support**:
- Register: Accepts decimal (1) or hex (0x01)
- Value: Accepts decimal (48) or hex (0x30)
- Mixed formats work: `{"reg":"0x00","value":48}`

---

### 3. Read All Registers

**Endpoint**: `GET /api/as3935/registers/all`

**Parameters**: None

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

**Error Response**:
```json
{"error": "Failed to read registers"}
```

**Examples**:
```bash
# Fetch all registers at once
curl http://192.168.1.100/api/as3935/registers/all

# Save to file for analysis
curl http://192.168.1.100/api/as3935/registers/all > sensor_state.json

# Pretty print with jq
curl -s http://192.168.1.100/api/as3935/registers/all | jq .
```

**Use Cases**:
- Periodic monitoring of all sensor values
- Before/after comparison during troubleshooting
- Backup current settings before making changes
- Verify sensor is responding (all registers should NOT be 0x00)

**HTTP Status**: `200 OK` (on success), `500 Internal Server Error` (on I2C failure)

---

### 4. Save Configuration

**Endpoint**: `POST /api/as3935/config/save`

**Method**: POST

**Content-Type**: `application/json`

**Request Body**:
```json
{
  "config": "{\"0x00\":\"0x24\",\"0x01\":\"0x22\",\"0x07\":\"0x00\"}"
}
```

**Purpose**: Persist current register configuration to NVS (non-volatile storage). Configuration will be automatically loaded on device restart.

**Parameters**:
- `config` (required): JSON string containing register values

**Response** (Success):
```json
{"ok": true, "message": "Configuration saved"}
```

**Response** (Failure):
```json
{"error": "Failed to save configuration"}
```

**Examples**:
```bash
# Save current sensor configuration
curl -X POST http://192.168.1.100/api/as3935/config/save \
  -H "Content-Type: application/json" \
  -d '{"config":"{\"0x00\":\"0x24\",\"0x01\":\"0x22\"}"}'

# Two-step: Read all, then save
curl -s http://192.168.1.100/api/as3935/registers/all | \
  jq -r '.registers | @json' | \
  xargs -I {} curl -X POST http://192.168.1.100/api/as3935/config/save \
  -H "Content-Type: application/json" \
  -d '{"config":"{}"}'
```

**Important**: Without saving, register changes are lost on device restart.

**HTTP Status**: `200 OK` (on success), `400 Bad Request` (invalid config format)

---

## Complete Workflow Examples

### Example 1: Increase Sensitivity

```bash
#!/bin/bash

DEVICE="192.168.1.100"

# Step 1: Read current AFE Gain
echo "Reading current AFE Gain (0x00)..."
curl -s "http://$DEVICE/api/as3935/register/read?reg=0x00" | jq '.value'

# Step 2: Write new higher sensitivity value
echo "Writing new AFE Gain value (0x40)..."
curl -X POST "http://$DEVICE/api/as3935/register/write" \
  -H "Content-Type: application/json" \
  -d '{"reg":"0x00","value":"0x40"}'

# Step 3: Read all registers to verify
echo "Verifying all registers..."
curl -s "http://$DEVICE/api/as3935/registers/all" | jq '.registers'

# Step 4: Save configuration to NVS
echo "Saving configuration..."
curl -X POST "http://$DEVICE/api/as3935/config/save" \
  -H "Content-Type: application/json" \
  -d '{"config":"{\"0x00\":\"0x40\",\"0x01\":\"0x22\"}"}'

echo "Done!"
```

---

### Example 2: Automatic Tuning Script

```bash
#!/bin/bash

DEVICE="192.168.1.100"

echo "=== AS3935 Automatic Configuration ==="

# Configuration profiles
declare -A profiles=(
  [balanced]='{"0x00":"0x24","0x01":"0x22"}'
  [high_sensitivity]='{"0x00":"0x40","0x01":"0x10"}'
  [low_sensitivity]='{"0x00":"0x18","0x01":"0x40"}'
)

PROFILE="${1:-balanced}"

if [[ ! -v profiles[$PROFILE] ]]; then
  echo "Unknown profile: $PROFILE"
  echo "Available: balanced, high_sensitivity, low_sensitivity"
  exit 1
fi

CONFIG="${profiles[$PROFILE]}"

# Apply each register
echo "Applying $PROFILE profile..."
jq -r 'to_entries[] | "\(.key) \(.value)"' <<< "$CONFIG" | while read reg val; do
  echo "  Setting register $reg to $val"
  curl -s -X POST "http://$DEVICE/api/as3935/register/write" \
    -H "Content-Type: application/json" \
    -d "{\"reg\":\"$reg\",\"value\":\"$val\"}" > /dev/null
done

# Save to NVS
curl -s -X POST "http://$DEVICE/api/as3935/config/save" \
  -H "Content-Type: application/json" \
  -d "{\"config\":\"$CONFIG\"}" > /dev/null

echo "✓ $PROFILE profile applied and saved"
```

---

### Example 3: Monitor Sensor Health

```bash
#!/bin/bash

DEVICE="192.168.1.100"

echo "=== AS3935 Sensor Health Monitor ==="

REGS=$(curl -s "http://$DEVICE/api/as3935/registers/all")

echo "Register Status:"
echo "$REGS" | jq '.registers | to_entries[] | "  \(.key): \(.value)"' -r

# Check if sensor is responding
AFE=$(echo "$REGS" | jq -r '.registers["0x00"]')
if [ "$AFE" == "0x00" ]; then
  echo "⚠️  WARNING: Sensor not responding (0x00 = no I2C communication)"
else
  echo "✓ Sensor responding: AFE Gain = $AFE"
fi

# Check distance register
DIST=$(echo "$REGS" | jq -r '.registers["0x08"]')
if [ "$DIST" == "0xFF" ]; then
  echo "• No recent lightning detected (0xFF = no event)"
elif [ "$DIST" == "0x00" ]; then
  echo "⚡ Recent close lightning detected (< 5 km)"
else
  KM=$(echo "obase=16; $DIST" | bc)
  echo "⚡ Recent lightning ~$DIST km away"
fi
```

---

## Error Handling

### Common Errors and Solutions

| Error | Cause | Solution |
|-------|-------|----------|
| `Failed to read register` | I2C communication error | Verify sensor I2C address and wiring |
| `Failed to write register` | Register address out of range | Check register is 0x00-0x08 |
| `400 Bad Request` | Invalid JSON format | Check JSON syntax in request body |
| `500 Internal Server Error` | Device not responding on I2C | Check sensor power and connection |

### HTTP Status Codes

- `200 OK`: Request succeeded
- `400 Bad Request`: Invalid parameters or malformed JSON
- `500 Internal Server Error`: I2C communication failure or sensor not responding

---

## Integration Examples

### Python Integration

```python
import requests
import json

class AS3935:
    def __init__(self, device_ip, timeout=5):
        self.base_url = f"http://{device_ip}"
        self.timeout = timeout
    
    def read_register(self, reg):
        """Read a single register"""
        url = f"{self.base_url}/api/as3935/register/read?reg={reg:02x}"
        r = requests.get(url, timeout=self.timeout)
        return r.json()
    
    def write_register(self, reg, value):
        """Write a single register"""
        url = f"{self.base_url}/api/as3935/register/write"
        data = {"reg": f"0x{reg:02x}", "value": f"0x{value:02x}"}
        r = requests.post(url, json=data, timeout=self.timeout)
        return r.json()
    
    def read_all_registers(self):
        """Read all registers at once"""
        url = f"{self.base_url}/api/as3935/registers/all"
        r = requests.get(url, timeout=self.timeout)
        return r.json()
    
    def increase_sensitivity(self):
        """Increase AFE gain to 0x40"""
        return self.write_register(0x00, 0x40)

# Usage
sensor = AS3935("192.168.1.100")
all_regs = sensor.read_all_registers()
print(f"AFE Gain: {all_regs['registers']['0x00']}")
sensor.increase_sensitivity()
```

### Node.js Integration

```javascript
const axios = require('axios');

class AS3935 {
  constructor(deviceIP) {
    this.baseURL = `http://${deviceIP}`;
  }
  
  async readRegister(reg) {
    const url = `${this.baseURL}/api/as3935/register/read?reg=0x${reg.toString(16).padStart(2, '0')}`;
    const response = await axios.get(url);
    return response.data;
  }
  
  async writeRegister(reg, value) {
    const url = `${this.baseURL}/api/as3935/register/write`;
    const response = await axios.post(url, {
      reg: `0x${reg.toString(16).padStart(2, '0')}`,
      value: `0x${value.toString(16).padStart(2, '0')}`
    });
    return response.data;
  }
  
  async readAllRegisters() {
    const url = `${this.baseURL}/api/as3935/registers/all`;
    const response = await axios.get(url);
    return response.data;
  }
}

// Usage
const sensor = new AS3935('192.168.1.100');
const regs = await sensor.readAllRegisters();
console.log('AFE Gain:', regs.registers['0x00']);
```

---

## Rate Limiting & Performance

- **No built-in rate limiting** on these endpoints
- Each write operation requires I2C communication (~1-2ms)
- Bulk reads (all registers) are more efficient than individual reads
- Recommended: Max 10 writes per second to avoid I2C bus saturation

---

## Best Practices

1. **Always read before write**: Verify current value before changing
2. **Save after tuning**: Use config/save endpoint to persist changes
3. **Monitor distance register**: 0x08 should update after strikes (not stuck at 0xFF)
4. **Test in same conditions**: Don't compare outdoor vs indoor readings
5. **Document working values**: Keep notes of configurations that worked well
6. **Use bulk reads**: `/registers/all` is more efficient than 9 individual reads

---

## Related Resources

- **Quick Reference**: `AS3935_ADVANCED_SETTINGS_QUICK.md`
- **Full Register Guide**: `AS3935_REGISTER_CONFIGURATION.md`
- **Troubleshooting**: `URGENT_I2C_NACK_ERROR.md`
- **I2C Configuration**: `I2C_ADDRESS_CONFIGURATION.md`

---

*Last Updated: November 2025*
*API Version: 1.0*
*AS3935 Hardware: AMS AS3935*
