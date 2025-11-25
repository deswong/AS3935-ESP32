# AS3935 Lightning Monitor — Quick Reference

## Setup (5 min)

```bash
idf.py set-target esp32c3
idf.py fullclean build
idf.py -p /dev/ttyUSB0 flash monitor
```

Connect to `AS3935-Setup` AP → `192.168.4.1` → Enter Wi-Fi & MQTT → Done.

## Hardware Pinout (ESP32-C3)

| AS3935 | ESP32-C3 GPIO |
|--------|---------------|
| VCC    | 3.3V          |
| GND    | GND           |
| SDA    | GPIO 21       |
| SCL    | GPIO 22       |
| IRQ    | GPIO 23       |

**Add 10kΩ pull-ups** to SDA/SCL if not on module.

## Web UI Endpoints

| Page | URL |
|------|-----|
| Setup | `http://192.168.4.1` (first boot) |
| Home | `http://<device-ip>/` (after setup) |
| Config | Click menu in web UI |

## REST API Quick Hits

```bash
# Status
curl http://<ip>/api/as3935/status
curl http://<ip>/api/wifi/status
curl http://<ip>/api/mqtt/status

# Configure Wi-Fi
curl -X POST http://<ip>/api/wifi/save \
  -H "Content-Type: application/json" \
  -d '{"ssid":"MyNet","password":"MyPass"}'

# Configure MQTT
curl -X POST http://<ip>/api/mqtt/save \
  -H "Content-Type: application/json" \
  -d '{"broker":"192.168.1.100","port":1883}'

# Read all registers
curl http://<ip>/api/as3935/registers/all

# Calibrate
curl -X POST http://<ip>/api/as3935/calibrate
curl http://<ip>/api/as3935/calibrate/status
curl -X POST http://<ip>/api/as3935/calibrate/apply

# Real-time events
curl http://<ip>/api/events/stream
```

## MQTT Events

**Topic:** `as3935/lightning` (default, configurable)

**Payload:**
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

## Troubleshooting

| Issue | Fix |
|-------|-----|
| No AP | Power cycle, wait 10s |
| Sensor not found | Try I2C addr 0x01, 0x02 via web UI |
| No events | Check wiring, verify IRQ pin, adjust sensitivity |
| No MQTT | Test with `/api/mqtt/test` endpoint |
| Time wrong | Check NTP connection, verify Wi-Fi |

## Key Files

- **Build**: `idf.py build`
- **Flash**: `idf.py -p /dev/ttyUSB0 flash`
- **Monitor**: `idf.py monitor`
- **Tests**: `idf.py -DTEST=true build`
- **Web UI**: `components/main/web/index.html`
- **REST API**: `components/main/app_main.c`
- **Sensor Driver**: `components/main/as3935_adapter.c`

## Default Configuration

- **I2C Port**: 0
- **I2C Address**: 0x03
- **SDA Pin**: GPIO 21
- **SCL Pin**: GPIO 22
- **IRQ Pin**: GPIO 23
- **MQTT Topic**: `as3935/lightning`
- **NTP Server**: `pool.ntp.org`

## Documentation

- [GETTING_STARTED.md](GETTING_STARTED.md) — Full setup guide
- [README.md](README.md) — Features & overview
- [API_REFERENCE.md](API_REFERENCE.md) — Complete REST API
- [HARDWARE.md](HARDWARE.md) — Schematic & BOM
- [RELEASE.md](RELEASE.md) — Deployment checklist
