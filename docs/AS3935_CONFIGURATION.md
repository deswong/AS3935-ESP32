# AS3935 Sensor Configuration Guide

This guide explains how to configure the AS3935 lightning detector sensor via the REST API or web UI.

## Quick Start

The device is running a web provisioning server. Connect to it and configure your sensor.

### Option 1: Using PowerShell (Windows - Recommended)

```powershell
# Navigate to the scripts directory
cd scripts

# Configure with default pins (SPI host 1, SCLK 14, MOSI 13, MISO 12, CS 15, IRQ 10)
.\configure_as3935.ps1 -Url http://192.168.4.1

# Check current configuration
.\configure_as3935.ps1 -Url http://192.168.4.1 -StatusOnly

# Configure with custom pins
.\configure_as3935.ps1 -Url http://192.168.4.1 -SpiHost 1 -Sclk 14 -Mosi 13 -Miso 12 -Cs 15 -Irq 10
```

### Option 2: Using Batch (Windows)

```batch
cd scripts
configure_as3935.bat http://192.168.4.1
```

### Option 3: Using Python

```bash
pip install requests
python scripts/configure_as3935.py --url http://192.168.4.1
python scripts/configure_as3935.py --url http://192.168.4.1 --status-only
python scripts/configure_as3935.py --url http://192.168.4.1 --spi-host 1 --sclk 14 --mosi 13 --miso 12 --cs 15 --irq 10
```

### Option 4: Using cURL

```bash
# Configure pins
curl -X POST http://192.168.4.1/api/as3935/pins/save \
  -H "Content-Type: application/json" \
  -d '{"spi_host": 1, "sclk": 14, "mosi": 13, "miso": 12, "cs": 15, "irq": 10}'

# Check current configuration
curl http://192.168.4.1/api/as3935/pins/status
curl http://192.168.4.1/api/as3935/status
```

## Default Pin Configuration (ESP32-C3 / ESP32)

These are the recommended default pins for SPI operation:

| Pin | Function | GPIO |
|-----|----------|------|
| SCLK | Serial Clock | 14 |
| MOSI | Master Out / Slave In | 13 |
| MISO | Master In / Slave Out | 12 |
| CS | Chip Select | 15 |
| IRQ | Interrupt Output | 10 |
| VCC | Power (3.3V) | 3.3V |
| GND | Ground | GND |

## Hardware Wiring

### SPI Connection (Recommended)

```
AS3935    ESP32-C3/ESP32
========  ==============
SCLK  --> GPIO 14 (SCLK)
MOSI  --> GPIO 13 (MOSI)
MISO  --> GPIO 12 (MISO)
CS    --> GPIO 15 (CS)
IRQ   --> GPIO 10 (IRQ)
VCC   --> 3.3V
GND   --> GND
```

## API Endpoints

### Configure Pins
**POST** `/api/as3935/pins/save`

Request body (JSON):
```json
{
  "spi_host": 1,
  "sclk": 14,
  "mosi": 13,
  "miso": 12,
  "cs": 15,
  "irq": 10
}
```

Response:
```json
{"ok": true}
```

### Get Pin Configuration
**GET** `/api/as3935/pins/status`

Response:
```json
{
  "i2c_port": 1,
  "sda": 14,
  "scl": 13,
  "irq": 10
}
```

### Get Sensor Status
**GET** `/api/as3935/status`

Response:
```json
{
  "sensor_id": 1,
  "config": {...}
}
```

### Configure Register Map (Advanced)
**POST** `/api/as3935/save`

Request body (JSON) - register addresses to values:
```json
{
  "0x00": 14,
  "0x01": 34,
  "0x02": 2
}
```

## Troubleshooting

### Device Not Responding
- Ensure the device is connected to WiFi and has an IP address
- Check the serial monitor for connection status
- The default AP mode IP is `192.168.4.1`
- When connected to WiFi, use the device's IP address from your router

### Pins Not Saved
- Check that the JSON payload is properly formatted
- Verify all required fields are present: `spi_host`, `sclk`, `mosi`, `irq`
- `miso` and `cs` are optional

### Sensor Not Initializing
- Ensure the hardware is properly wired
- Check that GPIO pins are not in use by other components
- Verify power supply (3.3V) to the AS3935 module
- Check the serial monitor for error messages

## Next Steps

After configuring the sensor:
1. Check the web UI for sensor status
2. Configure MQTT to publish lightning events
3. Monitor for incoming lightning detections via the REST API or MQTT broker

For more details, see the main [README.md](../README.md) in the project root.
