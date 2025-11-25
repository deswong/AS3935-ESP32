# REST API Reference

Complete documentation of all REST endpoints provided by the AS3935 Lightning Monitor firmware.

## Base URL

```
http://<device-ip>/
```

Replace `<device-ip>` with the device's IP address on your network (or `192.168.4.1` for the provisioning AP).

---

## Wi-Fi Endpoints

### GET /api/wifi/status

Get current Wi-Fi connection status.

**Response:**

```json
{
  "connected": true,
  "ssid": "MyNetwork",
  "ip": "192.168.1.42",
  "password_set": true
}
```

**Fields:**
- `connected`: Boolean. True if connected to a Wi-Fi network.
- `ssid`: Network name. Empty string if not connected.
- `ip`: Device IP address. Empty if not connected.
- `password_set`: Boolean. True if a password is configured in NVS.

**Example:**

```bash
curl http://192.168.1.42/api/wifi/status
```

---

### POST /api/wifi/save

Save Wi-Fi credentials and initiate connection.

**Request Body:**

```json
{
  "ssid": "MyNetwork",
  "password": "MyPassword"
}
```

**Fields:**
- `ssid`: (Required) Wi-Fi network name.
- `password`: (Optional) Wi-Fi password. Omit to connect to open network.

**Response:**

```json
{
  "ok": true
}
```

**Note:** Device will restart connection process. Takes 5-10 seconds to complete.

**Example:**

```bash
curl -X POST http://192.168.4.1/api/wifi/save \
  -H "Content-Type: application/json" \
  -d '{
    "ssid": "MyNetwork",
    "password": "MyPassword"
  }'
```

---

### GET /api/wifi/scan

Scan for available Wi-Fi networks.

**Response:**

```json
[
  {"ssid": "Network1", "rssi": -45},
  {"ssid": "Network2", "rssi": -65},
  {"ssid": "Network3", "rssi": -75}
]
```

**Fields:**
- `ssid`: Network name.
- `rssi`: Signal strength in dBm (closer to 0 is stronger).

**Note:** Scan takes 5-10 seconds. Response includes only visible networks.

**Example:**

```bash
curl http://192.168.4.1/api/wifi/scan
```

---

## MQTT Endpoints

### GET /api/mqtt/status

Get MQTT broker connection status.

**Response:**

```json
{
  "connected": true,
  "broker": "192.168.1.100",
  "port": 1883,
  "topic": "as3935/lightning",
  "last_event": 1700000000,
  "events_published": 42
}
```

**Fields:**
- `connected`: Boolean. True if connected to MQTT broker.
- `broker`: Broker hostname or IP.
- `port`: Broker port.
- `topic`: MQTT topic where events are published.
- `last_event`: Timestamp of last event (Unix seconds, 0 if none).
- `events_published`: Total events published since boot.

**Example:**

```bash
curl http://192.168.1.42/api/mqtt/status
```

---

### POST /api/mqtt/save

Save MQTT broker configuration and initiate connection.

**Request Body:**

```json
{
  "broker": "192.168.1.100",
  "port": 1883,
  "username": "mqtt_user",
  "password": "mqtt_pass",
  "topic": "as3935/lightning",
  "use_tls": false
}
```

**Fields:**
- `broker`: (Required) Broker hostname or IP address.
- `port`: (Optional) Broker port. Default: 1883.
- `username`: (Optional) MQTT username.
- `password`: (Optional) MQTT password.
- `topic`: (Optional) MQTT topic for events. Default: `as3935/lightning`.
- `use_tls`: (Optional) Boolean. Use TLS for connection. Default: false.

**Response:**

```json
{
  "ok": true
}
```

**Note:** Device will attempt connection immediately. Check status via `/api/mqtt/status`.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/mqtt/save \
  -H "Content-Type: application/json" \
  -d '{
    "broker": "192.168.1.100",
    "port": 1883,
    "topic": "as3935/lightning"
  }'
```

---

### POST /api/mqtt/test

Publish a test message to MQTT broker.

**Response:**

```json
{
  "ok": true,
  "message": "Test message published"
}
```

**Note:** Verifies MQTT connection is active and broker is reachable.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/mqtt/test
```

---

### POST /api/mqtt/clear_credentials

Clear all MQTT credentials from NVS storage.

**Response:**

```json
{
  "ok": true,
  "message": "Credentials cleared"
}
```

**Note:** MQTT connection will be terminated. Credentials must be re-saved via `/api/mqtt/save`.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/mqtt/clear_credentials
```

---

## AS3935 Sensor Endpoints

### GET /api/as3935/status

Get current sensor status and configuration.

**Response:**

```json
{
  "initialized": true,
  "sensor_id": 3,
  "i2c_addr": "0x03",
  "i2c_port": 0,
  "sda_pin": 21,
  "scl_pin": 22,
  "irq_pin": 23,
  "r0": "0x24",
  "r1": "0x22",
  "r3": "0x02",
  "r8": "0x04"
}
```

**Fields:**
- `initialized`: Boolean. Sensor communication OK.
- `sensor_id`: I2C address (decimal).
- `i2c_addr`: I2C address (hex string).
- `i2c_port`: ESP-IDF I2C port number (0 or 1).
- `sda_pin`, `scl_pin`, `irq_pin`: GPIO pin assignments.
- `r0`, `r1`, `r3`, `r8`: Key register values (hex strings).

**Example:**

```bash
curl http://192.168.1.42/api/as3935/status
```

---

### POST /api/as3935/save

Save sensor configuration (sensor ID).

**Request Body:**

```json
{
  "sensor_id": 3
}
```

**Fields:**
- `sensor_id`: Sensor ID (1-3, typically corresponds to I2C address bits).

**Response:**

```json
{
  "ok": true,
  "sensor_id": 3
}
```

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/save \
  -H "Content-Type: application/json" \
  -d '{"sensor_id": 3}'
```

---

## Pin Configuration Endpoints

### GET /api/as3935/pins/status

Get current I2C and interrupt pin configuration.

**Response:**

```json
{
  "i2c_port": 0,
  "sda_pin": 21,
  "scl_pin": 22,
  "irq_pin": 23
}
```

**Example:**

```bash
curl http://192.168.1.42/api/as3935/pins/status
```

---

### POST /api/as3935/pins/save

Save I2C and interrupt pin configuration.

**Request Body:**

```json
{
  "i2c_port": 0,
  "sda_pin": 8,
  "scl_pin": 9,
  "irq_pin": 10
}
```

**Fields:**
- `i2c_port`: ESP-IDF I2C port (0 or 1).
- `sda_pin`: GPIO pin for I2C SDA.
- `scl_pin`: GPIO pin for I2C SCL.
- `irq_pin`: GPIO pin for interrupt (lightning detection).

**Response:**

```json
{
  "ok": true
}
```

**Note:** Device will reinitialize I2C bus with new pins. Takes a few seconds.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/pins/save \
  -H "Content-Type: application/json" \
  -d '{
    "i2c_port": 0,
    "sda_pin": 8,
    "scl_pin": 9,
    "irq_pin": 10
  }'
```

---

## I2C Address Endpoints

### GET /api/as3935/address/status

Get current I2C slave address.

**Response:**

```json
{
  "i2c_addr": "0x03",
  "i2c_addr_dec": 3
}
```

**Fields:**
- `i2c_addr`: I2C address (hex string).
- `i2c_addr_dec`: I2C address (decimal).

**Example:**

```bash
curl http://192.168.1.42/api/as3935/address/status
```

---

### POST /api/as3935/address/save

Save I2C slave address.

**Request Body:**

```json
{
  "i2c_addr": 2
}
```

**Fields:**
- `i2c_addr`: I2C address as decimal (0-255). Common values: 0x01, 0x02, 0x03.

**Response:**

```json
{
  "ok": true,
  "i2c_addr": "0x02"
}
```

**Note:** Device will attempt to reinitialize sensor at new address.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/address/save \
  -H "Content-Type: application/json" \
  -d '{"i2c_addr": 2}'
```

---

## Register Access Endpoints

### GET /api/as3935/registers/all

Read all 9 AS3935 control registers.

**Response:**

```json
{
  "r0": "0x24",
  "r1": "0x22",
  "r2": "0x00",
  "r3": "0x02",
  "r4": "0x14",
  "r5": "0x24",
  "r6": "0x01",
  "r7": "0x40",
  "r8": "0x04"
}
```

**Fields:** Hex-formatted register values.

**Example:**

```bash
curl http://192.168.1.42/api/as3935/registers/all
```

---

### GET /api/as3935/register/read

Read a specific register.

**Query Parameters:**
- `reg`: Register address (hex or decimal). Examples: `0x00`, `0`, `0x08`.

**Response:**

```json
{
  "reg": "0x00",
  "value": "0x24"
}
```

**Example:**

```bash
curl "http://192.168.1.42/api/as3935/register/read?reg=0x00"
curl "http://192.168.1.42/api/as3935/register/read?reg=0"
```

---

### POST /api/as3935/register/write

Write a specific register.

**Request Body:**

```json
{
  "reg": "0x00",
  "value": "0x40"
}
```

**Fields:**
- `reg`: Register address (hex or decimal).
- `value`: Value to write (hex or decimal).

**Response:**

```json
{
  "ok": true,
  "reg": "0x00",
  "value": "0x40"
}
```

**Warning:** Incorrect register values can disable sensor or cause incorrect readings. Refer to AS3935 datasheet.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/register/write \
  -H "Content-Type: application/json" \
  -d '{
    "reg": "0x00",
    "value": "0x40"
  }'
```

---

## Calibration Endpoints

### POST /api/as3935/calibrate

Start automated sensor calibration.

**Response:**

```json
{
  "ok": true,
  "message": "Calibration started"
}
```

**Note:** Calibration may take 30-60 seconds. Do not send lightning or noise during this period.

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/calibrate
```

---

### GET /api/as3935/calibrate/status

Get calibration progress.

**Response (in progress):**

```json
{
  "in_progress": true,
  "step": 3,
  "total_steps": 5,
  "message": "Tuning sensitivity..."
}
```

**Response (complete):**

```json
{
  "in_progress": false,
  "success": true,
  "message": "Calibration complete. Apply or cancel."
}
```

**Example:**

```bash
curl http://192.168.1.42/api/as3935/calibrate/status
```

---

### POST /api/as3935/calibrate/apply

Apply calibration results and save to NVS.

**Response:**

```json
{
  "ok": true,
  "message": "Calibration applied"
}
```

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/calibrate/apply
```

---

### POST /api/as3935/calibrate/cancel

Cancel calibration and rollback to previous settings.

**Response:**

```json
{
  "ok": true,
  "message": "Calibration cancelled"
}
```

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/calibrate/cancel
```

---

## Sensor Parameters Endpoint

### POST /api/as3935/params

Set advanced sensor parameters (AFE gain, noise level, spike rejection, etc.).

**Request Body:**

```json
{
  "afe": 1,
  "noise_level": 2,
  "spike_rejection": 3,
  "min_strikes": 1,
  "disturber_enabled": true,
  "watchdog": 0
}
```

**Fields:**
- `afe`: AFE gain (0-1, higher = more sensitive).
- `noise_level`: Noise level threshold (0-3).
- `spike_rejection`: Spike rejection (0-3, higher = more filtering).
- `min_strikes`: Minimum strikes before reporting (0-3).
- `disturber_enabled`: Boolean. Report non-lightning noise.
- `watchdog`: Watchdog timer mode (typically 0).

**Response:**

```json
{
  "ok": true,
  "message": "Parameters applied"
}
```

**Example:**

```bash
curl -X POST http://192.168.1.42/api/as3935/params \
  -H "Content-Type: application/json" \
  -d '{
    "afe": 1,
    "noise_level": 2,
    "spike_rejection": 2,
    "min_strikes": 1,
    "disturber_enabled": true,
    "watchdog": 0
  }'
```

---

## Event Stream Endpoint

### GET /api/events/stream

Real-time event stream using Server-Sent Events (SSE).

**Response (streaming):**

```
data: {"event":"lightning","distance_km":12.3,"energy":5,"timestamp":1700000000}
data: {"event":"lightning","distance_km":8.5,"energy":12,"timestamp":1700000010}
```

**Usage:** Connect with a browser or SSE client library to receive real-time event notifications.

**Example (curl):**

```bash
curl http://192.168.1.42/api/events/stream
```

**Example (JavaScript):**

```javascript
const eventSource = new EventSource('http://192.168.1.42/api/events/stream');

eventSource.onmessage = function(event) {
  const data = JSON.parse(event.data);
  console.log('Event:', data);
};

eventSource.onerror = function(error) {
  console.error('SSE Error:', error);
  eventSource.close();
};
```

---

## Error Responses

All endpoints return error responses in the following format:

```json
{
  "error": "Description of the error",
  "code": 400
}
```

**Common HTTP Status Codes:**
- `200 OK`: Request successful.
- `400 Bad Request`: Invalid parameters or malformed JSON.
- `404 Not Found`: Endpoint not found.
- `500 Internal Server Error`: Device error (e.g., I2C communication failure).

---

## Rate Limiting

No explicit rate limiting is enforced. However:
- I2C operations are serialized and may take 10-100ms per request.
- Wi-Fi and MQTT operations are asynchronous; responses indicate initiation, not completion.
- Rapid successive requests to the same resource may return stale data.

---

## Authentication

**No authentication is required.** Restrict network access if this is a concern.

---

## CORS

The API does not explicitly enable CORS. Cross-origin requests from browsers may be blocked. For web-based integration, ensure the client and device are on the same domain or configure CORS proxies as needed.

---

## Examples

### Complete Workflow

```bash
# 1. Configure Wi-Fi
curl -X POST http://192.168.4.1/api/wifi/save \
  -H "Content-Type: application/json" \
  -d '{"ssid":"MyNetwork","password":"MyPass"}'

# Wait 5-10 seconds for connection

# 2. Configure MQTT
curl -X POST http://<device-ip>/api/mqtt/save \
  -H "Content-Type: application/json" \
  -d '{"broker":"192.168.1.100","port":1883,"topic":"as3935/lightning"}'

# 3. Verify sensor
curl http://<device-ip>/api/as3935/status

# 4. Test MQTT
curl -X POST http://<device-ip>/api/mqtt/test

# 5. Start monitoring
curl http://<device-ip>/api/events/stream
```

### Sensor Tuning

```bash
# Read all registers
curl http://<device-ip>/api/as3935/registers/all

# Increase sensitivity (AFE gain)
curl -X POST http://<device-ip>/api/as3935/params \
  -H "Content-Type: application/json" \
  -d '{"afe":1,"noise_level":2,"spike_rejection":2}'

# Run calibration
curl -X POST http://<device-ip>/api/as3935/calibrate
curl http://<device-ip>/api/as3935/calibrate/status
curl -X POST http://<device-ip>/api/as3935/calibrate/apply
```

---

## Support

For API issues or questions:
- Check this reference for endpoint details
- Review [GETTING_STARTED.md](GETTING_STARTED.md) for basic setup
- Consult the [README.md](README.md) for feature overview
- Open a GitHub issue for bugs or feature requests
