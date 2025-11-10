# AS3935 Lightning Monitor for ESP32

Lightweight ESP32 firmware to read an AS3935 lightning detector, provide a tiny web UI for provisioning and device settings, and publish lightning events to an MQTT broker.

Overview
- Built with ESP-IDF. Provides a single-file embedded web UI for provisioning and configuration, NVS persistence for settings, MQTT publishing and a compact AS3935 driver.

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

SPI wiring (recommended)

 - SPI host: 1 (or other available host)
 - SCLK pin: 14
 - MOSI pin: 13
 - MISO pin: 12 (optional depending on adapter)
 - CS pin: 15
 - IRQ pin: 10 (AS3935 interrupt output)
 - VCC: 3.3V
 - GND: GND

Note: Use SPI where possible for reliability. The firmware accepts both legacy I2C-style pin saves and the newer SPI mapping via the `AS3935 -> Pins` page.

What the JSON AS3935 register config is for
- The web UI and `/api/as3935/save` endpoint accept a JSON object that maps register addresses to byte values. This allows manual configuration of the AS3935 internal registers (gain, thresholds, masks, etc.) without rebuilding firmware.
- Format: a JSON object where keys are register addresses (hex like `"0x03"` or decimal like `"3"`) and values are integers 0–255.

REST endpoints (short)
- GET  /api/wifi/status
- POST /api/wifi/save
- POST /api/mqtt/save
- GET  /api/mqtt/status
 - POST /api/as3935/save
 - GET  /api/as3935/status
 - GET  /api/events/stream (SSE)

Time synchronization (SNTP / NTP)
- The firmware synchronizes its system clock using SNTP once it obtains a Wi‑Fi connection and an IP address. After the device gets an IP (`IP_EVENT_STA_GOT_IP`) the SNTP client is initialized and will obtain wall-clock time from `pool.ntp.org` by default.
- The device runs a background resync task that restarts SNTP every 12 hours to reduce clock drift. For tighter accuracy, configure a local NTP server on your LAN.
- If events occur before time is synced, event payloads always include a monotonic `uptime_ms` (milliseconds since boot) and a `time_ok` flag indicating whether wall-clock timestamps are valid.

MQTT payload (recommended schema)
Use compact JSON to keep payloads small. Send both a monotonic uptime and an optional wall-clock timestamp when available. Include a sequence number for ordering and a small error estimate when applicable.

Example payload (when time is synced):

```
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

Example payload (offline / no SNTP sync yet):

```
{
	"e": "lightning",
	"distance_km": 12.3,
	"energy": 5,
	"uptime_ms": 23456,
	"seq": 7,
	"time_ok": false
}
```

Notes on accuracy
- `uptime_ms`: obtained from the ESP-IDF high-resolution monotonic timer (`esp_timer_get_time()`), reliable for ordering and relative timings (microsecond resolution; typically reported in ms).
- `ts_ms`: obtained from system time after SNTP sync (`gettimeofday()`); typical achievable accuracy vs a public NTP pool is tens of milliseconds, depending on network latency.
- `err_ms`: optional; you can approximate it as half the SNTP round-trip time or use SNTP offset information if your SNTP client provides it.

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

Security
- Credentials are stored in NVS in plain text. For production use, consider stronger key management or encrypting secrets at rest.

Release notes / first-deploy checklist
- Build and flash the firmware to a test device.
- Connect a mobile device to the provisioning AP and confirm the captive-portal SPA appears.
- Provision Wi‑Fi and MQTT settings via the SPA.
- Confirm the device connects to Wi‑Fi and performs an SNTP sync (check serial logs for "Initializing SNTP").
- Verify MQTT events arrive at your broker with `time_ok` true when SNTP is successful, and `uptime_ms` always present.

Contributing
- Please open an issue for bugs or feature requests. For code changes, send a pull request with a clear description and a small focused diff.

- POST /api/as3935/save
- GET  /api/as3935/status
- GET  /api/events/stream (SSE)

MQTT payload
- Published JSON looks like: `{ "distance_km": <number>, "energy": <number>, "ts": <unix_ms_optional> }`.

Time synchronization (SNTP / NTP)
- The firmware can synchronize its system clock using SNTP once it obtains a Wi‑Fi connection and an IP address. This is enabled by the firmware: after the device gets an IP (`IP_EVENT_STA_GOT_IP`) the SNTP client is initialized and will obtain wall-clock time from `pool.ntp.org` by default.
- The device also runs a small background resync task that restarts SNTP every 12 hours to reduce clock drift. If your deployment requires tighter accuracy, point SNTP to a local NTP server on the same LAN.
- While the device may send events before time is synced, event payloads always include a monotonic `uptime_ms` (milliseconds since boot) and a `time_ok` flag indicating whether wall-clock timestamps are valid.

MQTT payload (recommended schema)
- Use compact JSON to keep payloads small. Send both a monotonic uptime and an optional wall-clock timestamp when available. Include a sequence number for ordering and a small error estimate when applicable.

Example payload (when time is synced):

```
{
	"e": "lightning",
	"distance_km": 12.3,
	"energy": 5,
	"ts_ms": 1700000000123,    // wall-clock epoch ms (UTC), present only if time_ok==true
	"uptime_ms": 1234567,      // monotonic ms since boot (always present)
	"seq": 42,
	"time_ok": true,
	"err_ms": 35               // optional estimate of time uncertainty
}
```

Example payload (offline / no SNTP sync yet):

```
{
	"e": "lightning",
	"distance_km": 12.3,
	"energy": 5,
	"uptime_ms": 23456,
	"seq": 7,
	"time_ok": false
}
```

Notes on accuracy
- `uptime_ms`: obtained from the ESP-IDF high-resolution monotonic timer (`esp_timer_get_time()`), reliable for ordering and relative timings (microsecond resolution; typically reported in ms).
- `ts_ms`: obtained from system time after SNTP sync (`gettimeofday()`); typical achievable accuracy vs a public NTP pool is tens of milliseconds, depending on network latency. For best results use a local LAN NTP server.
- `err_ms`: optional; you can approximate it as half the SNTP round-trip time or use SNTP offset information if your SNTP client provides it.

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

## Editing the embedded web UI (SPA)

The single-page app that the firmware serves lives in plain HTML at:

- `components/main/web/index.html`

During CMake configure the project automatically converts this HTML into a C
header (so the firmware can serve it from flash). The converter is
`tools/embed_web.py` and the generated header is written into the build tree
under the component's generated directory, for example:

```
build/esp-idf/main/generated/web_index.h
```

Quick workflow

- Edit the SPA in `components/main/web/index.html`.
- Re-generate the header and build the firmware (the build runs the generator
	automatically):

```bash
idf.py build
```

- Run the tests (unit tests use the generated header too):

```bash
idf.py -DTEST=true build
```

- Flash + monitor (device required):

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Manual regeneration

If you prefer to regenerate the header manually (without a full CMake run),
you can run the converter script directly:

```bash
python3 tools/embed_web.py components/main/web/index.html build/esp-idf/main/generated/web_index.h
```

Notes

- Do not commit generated header files into the repository. The project
	intentionally keeps the editable `index.html` under `components/main/web/`
	and generates the C header into the build tree during configure/build.
- If you want CI to validate changes to the SPA, have your CI run:
	`idf.py -DTEST=true build` so the generated header is created and unit tests run.

