# AS3935 Lightning Monitor for ESP32

Professional-grade ESP32 firmware for detecting and monitoring lightning strikes using the AS3935 sensor. Features a responsive web UI for configuration, real-time MQTT publishing, and comprehensive REST API for integration.

## Features

- **Lightning Detection**: High-sensitivity AS3935 lightning detector with configurable I2C interface
- **Web UI**: Embedded single-page app for Wi-Fi provisioning, sensor configuration, and real-time monitoring
- **MQTT Integration**: Automatic event publishing with timestamps and sensor data
- **REST API**: 20+ endpoints for programmatic sensor control and diagnostics
- **Automatic NTP Sync**: System time synchronization for accurate event logging
- **Calibration Support**: Automated sensor calibration with register tuning and rollback
- **NVS Persistence**: All settings stored in non-volatile storage, survives reboots
- **Captive Portal**: Automatic provisioning portal on first boot

## Quick Start

### Prerequisites

- ESP32-C3 (or compatible ESP32 variant)
- ESP-IDF 5.x or 6.x (with environment configured: `source $IDF_PATH/export.sh`)
- AS3935 lightning detector module with I2C interface

### Build and Flash

```bash
# Set target (defaults to esp32c3, change if needed)
idf.py set-target esp32c3

# Clean build
idf.py fullclean build

# Flash to device
idf.py -p /dev/ttyUSB0 flash monitor
```

### Initial Setup

1. Device boots and starts provisioning AP: `AS3935-Setup`
2. Connect mobile device or laptop to the AP
3. Captive portal opens automatically (if supported) or navigate to `http://192.168.4.1`
4. Configure Wi-Fi credentials and MQTT broker details
5. Device connects to Wi-Fi, syncs time, and begins monitoring

## Hardware Wiring

### I2C Configuration (ESP32-C3)

| Signal | Default GPIO | Notes |
|--------|--------------|-------|
| I2C SDA | GPIO 21 | Data line (configurable) |
| I2C SCL | GPIO 22 | Clock line (configurable) |
| IRQ | GPIO 23 | Lightning interrupt (configurable) |
| VCC | 3.3V | Power supply |
| GND | GND | Ground |

### Connection Example

```
AS3935 Module    ESP32-C3
─────────────    ────────
VCC (3.3V)  ──→  3.3V
GND         ──→  GND
SDA         ──→  GPIO 21 (I2C SDA)
SCL         ──→  GPIO 22 (I2C SCL)
IRQ         ──→  GPIO 23
```

**Pull-ups**: If not included on the AS3935 module, add 10kΩ-47kΩ pull-up resistors to SDA and SCL lines.

### Changing Pins

All pin assignments are configurable via the web UI:

1. Navigate to `http://<device-ip>/` (on same network)
2. Go to **AS3935 → Pins**
3. Enter desired GPIO numbers and save

Pins persist in NVS and apply automatically on reboot.

## Configuration

### Web UI

Access the device configuration interface:

- **First boot**: Provisioning AP `AS3935-Setup` at `192.168.4.1`
- **After provisioning**: Device IP (check your router or serial logs)

**Configuration Pages:**

- **Home**: Real-time lightning detection status and statistics
- **Wi-Fi Settings**: Provisioning and network configuration
- **MQTT Settings**: Broker connection and topic configuration
- **AS3935 Pins**: I2C and interrupt pin assignment
- **AS3935 Sensor**: I2C address and sensor configuration
- **Advanced Settings**: Register-level tuning and calibration

### REST API

Complete REST API with 20+ endpoints for programmatic control:

- **Wi-Fi**: `/api/wifi/status`, `/api/wifi/save`, `/api/wifi/scan`
- **MQTT**: `/api/mqtt/status`, `/api/mqtt/save`, `/api/mqtt/test`, `/api/mqtt/clear_credentials`
- **Sensor**: `/api/as3935/status`, `/api/as3935/save`
- **Pins**: `/api/as3935/pins/status`, `/api/as3935/pins/save`
- **Address**: `/api/as3935/address/status`, `/api/as3935/address/save`
- **Calibration**: `/api/as3935/calibrate`, `/api/as3935/calibrate/status`, `/api/as3935/calibrate/apply`, `/api/as3935/calibrate/cancel`
- **Registers**: `/api/as3935/registers/all`, `/api/as3935/register/read`, `/api/as3935/register/write`
- **Parameters**: `/api/as3935/params`
- **Events**: `/api/events/stream` (Server-Sent Events)

See [API_REFERENCE.md](API_REFERENCE.md) for detailed documentation.

## MQTT Events

### Payload Format

When lightning is detected, the device publishes a JSON event to the configured MQTT topic (default: `as3935/lightning`).

**With Time Sync:**

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

**Without Time Sync:**

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

**Field Descriptions:**

- `e`: Event type (`lightning`, `noise`, `disturber`)
- `distance_km`: Estimated distance to lightning strike
- `energy`: Raw energy value from sensor
- `ts_ms`: Wall-clock timestamp (UTC epoch milliseconds, only when `time_ok=true`)
- `uptime_ms`: Device uptime in milliseconds (monotonic, always present)
- `seq`: Sequence number for event ordering
- `time_ok`: Whether wall-clock timestamp is synchronized and valid
- `err_ms`: Estimated time error in milliseconds (optional)

## Sensor Configuration

### I2C Address

Default address is `0x03`. Common alternatives: `0x02`, `0x01`. Configure via web UI or REST API:

```bash
curl -X POST http://<device-ip>/api/as3935/address/save \
  -H "Content-Type: application/json" \
  -d '{"i2c_addr": 2}'
```

### Register Tuning

For advanced sensor tuning, modify individual AS3935 registers via REST API:

```bash
# Read a register
curl "http://<device-ip>/api/as3935/register/read?reg=0x00"

# Write a register
curl -X POST http://<device-ip>/api/as3935/register/write \
  -H "Content-Type: application/json" \
  -d '{"reg": "0x00", "value": "0x40"}'

# Read all registers
curl "http://<device-ip>/api/as3935/registers/all"
```

### Calibration

The firmware supports automated calibration with register tuning and rollback:

```bash
# Start calibration
curl -X POST http://<device-ip>/api/as3935/calibrate

# Check status
curl http://<device-ip>/api/as3935/calibrate/status

# Apply results
curl -X POST http://<device-ip>/api/as3935/calibrate/apply

# Cancel and rollback
curl -X POST http://<device-ip>/api/as3935/calibrate/cancel
```

## Time Synchronization

The firmware automatically synchronizes system time via SNTP when Wi-Fi connects:

- **Initialization**: SNTP client starts after obtaining an IP address
- **Server**: Defaults to `pool.ntp.org` (configurable)
- **Sync Interval**: Background task restarts SNTP every 12 hours to limit drift
- **Accuracy**: Typical accuracy is 10-100ms depending on network latency
- **Events**: Include `time_ok` flag and `ts_ms` field when synchronized

For higher accuracy, configure a local NTP server on your LAN.

## Web UI Development

The embedded single-page app (SPA) is located at `components/main/web/index.html`.

### Development Workflow

1. Edit the SPA: `components/main/web/index.html`
2. Rebuild: `idf.py build` (automatically converts HTML to C header)
3. Run tests: `idf.py -DTEST=true build`
4. Flash: `idf.py -p /dev/ttyUSB0 flash monitor`

### Build Process

During CMake configure, the HTML file is converted to a C header by `tools/embed_web.py` and written to the build tree:

```
build/esp-idf/main/generated/web_index.h
```

**Important**: Do not commit generated headers. The repository keeps only the source `index.html`.

## Security Considerations

- **Credentials**: Wi-Fi and MQTT credentials are stored in NVS in plain text. For production deployments, consider:
  - Encrypting sensitive data at rest
  - Using secure provisioning methods
  - Limiting network access to the configuration interface
- **API Access**: REST endpoints have no authentication. Restrict network access if needed.

## Troubleshooting

### Device Won't Boot

- Check serial logs: `idf.py monitor`
- Verify power supply (3.3V required)
- Ensure I2C connections are secure

### Sensor Not Responding

- Verify I2C address via web UI or REST API
- Check wiring and pull-up resistors
- Try alternative I2C addresses: `0x01`, `0x02`, `0x03`
- Monitor serial logs for I2C errors

### No Lightning Events

- Check sensor orientation (antenna should point upward)
- Verify IRQ pin is connected correctly
- Adjust sensitivity via Advanced Settings in web UI
- Check MQTT broker connection status

### Time Sync Issues

- Verify Wi-Fi connectivity
- Check that device can reach `pool.ntp.org` (or configured NTP server)
- Monitor serial logs for "Initializing SNTP" messages

## Examples

### Python MQTT Subscriber

```bash
# Install dependencies
pip install paho-mqtt

# Listen for events
python scripts/mqtt_sub_example.py --broker 192.168.1.10 --port 1883 --topic as3935/lightning
```

### Real-Time Event Stream (SSE)

```bash
# Connect to device's Server-Sent Events stream
curl http://<device-ip>/api/events/stream
```

### Query Sensor Status

```bash
# Get current sensor state
curl http://<device-ip>/api/as3935/status

# Check Wi-Fi connection
curl http://<device-ip>/api/wifi/status

# Check MQTT status
curl http://<device-ip>/api/mqtt/status
```

## Documentation

### Getting Started
- [GETTING_STARTED.md](GETTING_STARTED.md) - Step-by-step first-time setup guide
- [CHEAT_SHEET.md](CHEAT_SHEET.md) - Quick reference card (1-page, printable)

### Configuration & API
- [API_REFERENCE.md](API_REFERENCE.md) - Complete REST API documentation with examples
- [HARDWARE.md](HARDWARE.md) - Hardware setup, schematic, and Bill of Materials

### Deployment
- [RELEASE.md](RELEASE.md) - Release notes and production deployment checklist

## Contributing

- Report bugs via GitHub issues
- Submit pull requests with clear descriptions and focused diffs
- Follow ESP-IDF coding conventions

## License

See LICENSE file for details.

## Support

For support:
- Check the [Troubleshooting](#troubleshooting) section
- Review [GETTING_STARTED.md](GETTING_STARTED.md)
- Consult [API_REFERENCE.md](API_REFERENCE.md) for REST endpoint details
- Open a GitHub issue for bugs or feature requests
