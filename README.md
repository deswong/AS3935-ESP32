# AS3935 Lightning Monitor for ESP32

Lightweight ESP32 firmware to read an AS3935 lightning detector, provide a tiny web UI for provisioning and device settings, and publish lightning events to an MQTT broker.

Overview
- Small firmware built with ESP-IDF. Provides a single-file embedded web UI for provisioning and configuration, NVS persistence for settings, MQTT publishing and a simple AS3935 driver interface.

Quick prerequisites
- ESP-IDF 5.x installed and your environment exported (source $IDF_PATH/export.sh)
- Target device: ESP32 (this repo defaults to `esp32c3` in sdkconfig)

Build and flash (short)
```bash
idf.py set-target esp32c3
idf.py fullclean build
idf.py -p /dev/ttyUSB0 flash monitor
```

Wiring & pins (suggested defaults)
These are example pins used by the default UI and NVS settings. Adjust in the UI or NVS as required by your board.

 - I2C port: 0
 - SDA pin: 8
 - SCL pin: 9
 - IRQ pin: 10 (AS3935 interrupt output)
 - VCC: 3.3V
 - GND: GND

SPI wiring (recommended)

 - SPI host: 1 (or other available host)
 - SCLK pin: 14
 - MOSI pin: 13
 - MISO pin: 12 (optional depending on adapter)
 - CS pin: 15
 - IRQ pin: 10 (AS3935 interrupt output)
 - VCC: 3.3V
 - GND: GND

Note: Use SPI where possible for reliability. The firmware accepts both legacy I2C-style pin saves and the newer SPI mapping via the `AS3935 -> Pins` page. Use SPI JSON fields: `spi_host`, `sclk`, `mosi`, `miso`, `cs`, `irq`.

Note: Your AS3935 breakout may use different pinouts or require an I2C/SPI adapter. Use the `AS3935 -> Pins` UI page to set the exact pins and save them to NVS.

What the JSON AS3935 register config is for
- The web UI and `/api/as3935/save` endpoint accept a JSON object that maps register addresses to byte values. The purpose is to allow manual configuration of the AS3935 internal registers (gain, thresholds, masks, etc.) without rebuilding firmware.
- Format: a JSON object where keys are register addresses (hex like `"0x03"` or decimal like `"3"`) and values are integers 0–255.
	Example:
	```json
	{"0x00":36, "0x01":34, "0x02":2}
	```
- When saved the firmware will write the provided values to the sensor over I2C and persist the JSON in NVS so it is re-applied on reboot.

REST endpoints (short)
- GET  /api/wifi/status
- POST /api/wifi/save
- POST /api/mqtt/save
- GET  /api/mqtt/status
- POST /api/as3935/save
- GET  /api/as3935/status
- GET  /api/events/stream (SSE)

MQTT payload
- Published JSON looks like: `{ "distance_km": <number>, "energy": <number>, "ts": <unix_ms_optional> }`.

Examples & helper scripts
- Install Python helpers:
	```bash
	pip install -r requirements.txt
	```
- Listen with mosquitto_sub:
	```bash
	mosquitto_sub -h <broker_ip> -t 'as3935/lightning' -v
	```
- Python MQTT subscriber:
	```bash
	python scripts/mqtt_sub_example.py --broker 192.168.1.10 --port 1883 --topic as3935/lightning
	```
- SSE subscriber (connect to the device's event stream):
	```bash
	python scripts/sse_sub_example.py --url http://192.168.4.1/api/events/stream
	```

Tidying notes
- Removed old/duplicate tasks from README. The repo TODOs are tracked in the project issue list.

Security
- Credentials are stored in NVS in plain text. For production use, consider stronger key management.

# AS3935 Lightning Monitor for ESP32

This project provides firmware for the ESP32 to interface with the AS3935 lightning detector, host a web UI for configuration, and publish lightning events over MQTT.

Current status
- Project scaffold with basic HTTP server and AS3935 driver stub.

Provisioning and configuration
- Web provisioning portal available on the device (AP mode on first boot).
- Use the web UI to configure Wi‑Fi, MQTT broker (URI + TLS flag), and AS3935 register map (JSON).

AS3935 configuration via UI
- Sensor ID dropdown: the UI now provides a simple Sensor ID dropdown (Sensor 1/2/3) instead of requiring a JSON upload for the common case. Choosing a sensor and clicking Save will POST {"sensor_id": <1|2|3>} to `/api/as3935/save` and the value is persisted to NVS under the key `sensor_id`.

- Advanced: Register JSON (unchanged): the UI still supports uploading a register map JSON for advanced configuration. That format is a JSON object where keys are register addresses (hex like `"0x03"` or decimal like `"3"`) and values are integers 0–255. Example:
	```json
	{"0x00":36, "0x01":34, "0x02":2}
	```
	When saved the firmware will write the provided values to the sensor and persist the JSON in NVS so it is re-applied on reboot.

Suggested wiring (ESP32-C3)
- AS3935 <---> ESP32-C3 (I2C)
	- SDA -> GPIO (example) 8
	- SCL -> GPIO (example) 9
	- IRQ -> GPIO (example) 10 (configure in the UI)
	- VCC -> 3.3V
	- GND -> GND

Notes
- The AS3935 module variants may use different register maps or an I2C bridge address; use the UI to load and test register writes before relying on them.
- After configuring MQTT in the UI, the device will attempt to connect and publish lightning events to topic `as3935/lightning` with JSON payload {"distance_km":...,"energy":...}.

Build & Flash (ESP-IDF required)

1. Install ESP-IDF (see Espressif docs).
2. Set up environment variables per ESP-IDF.
3. From project root:

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Next steps
- Implement I2C driver for AS3935 using esp-idf I2C APIs
- Implement Wi-Fi provisioning (AP/captive portal)
- Implement full Material Design web UI and embed assets
- Implement MQTT publishing via esp-mqtt
# AS3935-ESP32
 AS3935 ESP32 to MQTT
