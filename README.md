# AS3935 Lightning Monitor for ESP32

Lightweight ESP32 firmware to read an AS3935 lightning detector via I2C, provide a web UI for provisioning and sensor configuration, and publish lightning events to an MQTT broker.

## Overview

- **Built with ESP-IDF v6.0** 
- **Single-file embedded web UI** for provisioning, configuration, and sensor tuning
- **I2C communication** to AS3935 lightning detector (configurable address: 0x03, 0x02, 0x01, or custom)
- **NVS persistence** for all settings
- **MQTT publishing** for lightning events
- **REST API** for remote sensor control

## Quick Prerequisites

- **ESP-IDF 5.x or 6.x** installed and environment exported (`source $IDF_PATH/export.sh`)
- **Target device**: ESP32-C3 (configurable via `idf.py set-target`)
- **AS3935 module** with I2C interface

## Build and Flash

```bash
# Set target (if not already ESP32-C3)
idf.py set-target esp32c3

# Clean build
idf.py fullclean build

# Flash and monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Wiring & Pins

The firmware uses I2C for communication. Pin assignments are configurable via the web UI.

### Default I2C Pins (ESP32-C3)

| Function | GPIO Pin | Notes |
|----------|----------|-------|
| I2C SDA (Data) | GPIO 7 | I2C data line (configurable) |
| I2C SCL (Clock) | GPIO 4 | I2C clock line (configurable) |
| IRQ (Interrupt) | GPIO 0 | Lightning detection interrupt (configurable) |
| VCC (Power) | 3.3V | AS3935 requires 3.3V |
| GND (Ground) | GND | Common ground |

### I2C Address

- **Default Address**: `0x03` (typical for most AS3935 modules)
- **Alternative Addresses**: `0x02`, `0x01` (check your module documentation)
- **Custom Addresses**: Any value 0x00-0xFF (configurable via web UI)

### Hardware Connection Example

```
AS3935 Module        ESP32-C3
─────────────       ──────────
VCC (3.3V)    ──→   3.3V
GND           ──→   GND
SDA           ──→   GPIO 7 (I2C1_SDA)
SCL           ──→   GPIO 4 (I2C1_SCL)
IRQ           ──→   GPIO 0
```

**Note**: I2C pull-ups (typically 10kΩ to 47kΩ) may be needed if not included on the AS3935 module.

## Configuration

All configuration is done via the web UI. **No code changes needed!**

### Via Web UI

1. Build and flash the firmware
2. Connect to the device's provisioning AP
3. Enter Wi-Fi and MQTT credentials
4. Configure I2C address and pins (if needed)
5. Adjust sensor sensitivity via the **Advanced Settings** page

### Via REST API

See **[AS3935_REGISTER_API.md](AS3935_REGISTER_API.md)** for complete API documentation.

```bash
# Read all registers
curl http://192.168.x.x/api/as3935/registers/all

# Write a register (e.g., increase sensitivity)
curl -X POST http://192.168.x.x/api/as3935/register/write \
  -H "Content-Type: application/json" \
  -d '{"reg":"0x00","value":"0x40"}'
```

## AS3935 Register Configuration

The sensor's behavior is controlled via 9 registers. You can adjust:

- **Register 0x00** - AFE Gain (sensitivity)
- **Register 0x01** - Threshold (detection level)
- **Register 0x02** - Configuration
- **Register 0x07** - Calibration

See **[AS3935_ADVANCED_SETTINGS_QUICK.md](AS3935_ADVANCED_SETTINGS_QUICK.md)** for quick tuning tips or **[AS3935_REGISTER_CONFIGURATION.md](AS3935_REGISTER_CONFIGURATION.md)** for the complete guide.

## Time Synchronization

The firmware synchronizes its system clock using SNTP once it connects to Wi-Fi. 

- After the device obtains an IP address (`IP_EVENT_STA_GOT_IP`), the SNTP client initializes and fetches wall-clock time from `pool.ntp.org` by default
- A background task restarts SNTP every 12 hours to reduce clock drift. For tighter accuracy, configure a local NTP server on your LAN
- Events include both monotonic `uptime_ms` (milliseconds since boot) and an optional wall-clock `ts_ms` with a `time_ok` flag

## MQTT Payload Format

When lightning is detected, the device publishes a compact JSON event to the broker (default topic: `as3935/lightning`).

**Example with synchronized time:**

```json
{
	"e": "lightning",
	"distance_km": 12.3,
	"energy": 5,
	"ts_ms": 1700000000123,
	"uptime_ms": 1234567,
	"seq": 42,
	"time_ok": true,
	"err_ms": 35
}
```

**Example without synchronized time:**

```json
{
	"e": "lightning",
	"distance_km": 12.3,
	"energy": 5,
	"uptime_ms": 23456,
	"seq": 7,
	"time_ok": false
}
```

**Field Notes:**

- `uptime_ms`: Monotonic milliseconds since boot (always present, microsecond resolution)
- `ts_ms`: Wall-clock epoch milliseconds (UTC, present only if `time_ok == true`)
- `err_ms`: Estimated time uncertainty (optional, approximately half the SNTP round-trip time)

## Examples & Helper Scripts

```bash
# Install Python dependencies
pip install -r requirements.txt

# Listen with mosquitto_sub
mosquitto_sub -h <broker_ip> -t 'as3935/lightning' -v

# Python MQTT subscriber
python scripts/mqtt_sub_example.py --broker 192.168.1.10 --port 1883 --topic as3935/lightning

# SSE subscriber (real-time events from device)
python scripts/sse_sub_example.py --url http://192.168.4.1/api/events/stream
```

## Web UI Development

The embedded single-page app (SPA) is located at:

```
components/main/web/index.html
```

During the CMake configure phase, the HTML is automatically converted into a C header so the firmware can serve it from flash storage. The converter is `tools/embed_web.py`, and the generated header is written to:

```
build/esp-idf/main/generated/web_index.h
```

### Development Workflow

Edit the SPA in `components/main/web/index.html`, then rebuild:

```bash
idf.py build
```

### Run Tests

```bash
idf.py -DTEST=true build
```

### Flash and Monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Manual Header Regeneration

To regenerate the web header without a full CMake rebuild:

```bash
python3 tools/embed_web.py components/main/web/index.html build/esp-idf/main/generated/web_index.h
```

**Important Notes:**

- Do not commit generated header files to the repository
- The build tree automatically generates the C header during configure/build
- For CI validation, run: `idf.py -DTEST=true build` to generate the header and run tests

## Security Notes

- Credentials are stored in NVS in plain text
- For production use, consider stronger key management or encryption at rest

## Contributing

- Open an issue for bugs or feature requests
- Send pull requests with clear descriptions and focused diffs

## Quick Links

- [I2C Address Configuration](I2C_ADDRESS_CONFIGURATION.md)
- [I2C Pinout Guide](I2C_PINOUT_GUIDE.md)
- [AS3935 Register Configuration](AS3935_REGISTER_CONFIGURATION.md)
- [Advanced Settings](AS3935_ADVANCED_SETTINGS_QUICK.md)
- [Register API Documentation](AS3935_REGISTER_API.md)
- [Live Status Display](LIVE_STATUS_DISPLAY.md)
- [Complete Documentation Index](DOCUMENTATION_INDEX.md)

