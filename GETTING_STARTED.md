# Getting Started Guide

This guide walks you through the initial setup of your AS3935 Lightning Monitor.

## What You'll Need

1. **Hardware**
   - ESP32-C3 development board
   - AS3935 lightning detector module (I2C interface)
   - USB cable for programming
   - Jumper wires
   - Optional: 10kΩ-47kΩ resistors for I2C pull-ups

2. **Software**
   - ESP-IDF 5.x or 6.x installed
   - Python 3.7+ for build tools
   - `mosquitto_sub` or MQTT client (optional, for testing)

## Step 1: Build and Flash

### On Linux/macOS

```bash
# Clone this repository
git clone https://github.com/yourusername/AS3935-ESP32.git
cd AS3935-ESP32

# Set target (if not already esp32c3)
idf.py set-target esp32c3

# Clean build
idf.py fullclean build

# Flash to device (adjust port as needed)
idf.py -p /dev/ttyUSB0 flash monitor
```

### Expected Serial Output

```
...
I (1000) app_main: Starting AS3935 Lightning Monitor
I (2000) wifi_prov: Initializing provisioning AP
I (2000) wifi_prov: Started AP 'AS3935-Setup'
I (2000) web_server: HTTP server started on port 80
```

## Step 2: Wire the Hardware

Connect your AS3935 module to the ESP32-C3 following this diagram:

```
AS3935 Module    ESP32-C3
─────────────    ────────
VCC (3.3V)  ──→  3.3V pin
GND         ──→  GND pin
SDA         ──→  GPIO 21
SCL         ──→  GPIO 22
IRQ         ──→  GPIO 23
```

**Pull-ups**: If your AS3935 module doesn't have built-in pull-ups, add 10kΩ resistors from SDA and SCL to 3.3V.

Verify connections are secure and there are no cold solder joints.

## Step 3: Connect to Provisioning AP

1. **Power on the device** (via USB)
2. **Look for Wi-Fi network** named `AS3935-Setup`
3. **Connect to it** (no password required)
4. **Open a web browser** to `http://192.168.4.1`

You should see the configuration UI. If not:
- Check your phone/computer is connected to the `AS3935-Setup` network
- Try opening `http://192.168.4.1` directly in your browser
- Check serial logs for errors

## Step 4: Configure Wi-Fi and MQTT

### In the Web UI

1. **Go to Wi-Fi Settings page**
   - Select your home Wi-Fi network from the list
   - Enter your Wi-Fi password
   - Click **Save**

2. **Go to MQTT Settings page**
   - Enter MQTT broker IP or hostname (e.g., `192.168.1.100`)
   - Enter MQTT port (default: `1883`)
   - Optional: username and password
   - Click **Save and Test**

3. **Wait for connection**
   - Device will restart and attempt to connect
   - Provisioning AP will disappear
   - Device will sync time from NTP
   - Serial logs will show "Initializing SNTP"

**Expected result**: Device connects to your Wi-Fi and MQTT broker. You can now access it on your home network at its IP address (check your router).

## Step 5: Verify Operation

### Option A: Via Web UI

1. Navigate to `http://<device-ip>/` on your home network
2. Check **Home** page for sensor status
3. You should see "Sensor initialized" or "Ready"

### Option B: Via REST API

```bash
# Get device status
curl http://<device-ip>/api/as3935/status

# Should return something like:
# {"sensor_id":3,"i2c_addr":"0x03","irq_pin":23,...}

# Get Wi-Fi status
curl http://<device-ip>/api/wifi/status

# Get MQTT status
curl http://<device-ip>/api/mqtt/status
```

### Option C: Monitor MQTT Events

```bash
# Install MQTT tools (Ubuntu/Debian)
sudo apt-get install mosquitto-clients

# Subscribe to lightning events (replace 192.168.1.100 with your broker IP)
mosquitto_sub -h 192.168.1.100 -t "as3935/lightning" -v
```

When lightning strikes nearby, you'll see JSON events appear in your terminal.

## Troubleshooting

### Problem: Can't connect to provisioning AP

**Solution:**
- Reboot the device
- Check that Wi-Fi is enabled on your phone/computer
- Try connecting from a different device
- Check serial logs for errors

### Problem: Sensor not responding

**Solution:**
1. Verify wiring (especially I2C and IRQ pins)
2. Check I2C address in web UI (try 0x01, 0x02, 0x03)
3. Verify power supply voltage (should be 3.3V)
4. Add pull-up resistors if missing
5. Check serial logs for I2C errors

### Problem: No lightning events detected

**Solution:**
1. Verify sensor is oriented correctly (antenna up)
2. Check that MQTT connection is active (`/api/mqtt/status`)
3. Trigger a test MQTT message via web UI button "Test MQTT"
4. Verify MQTT broker is receiving messages (`mosquitto_sub`)
5. Try adjusting sensitivity in Advanced Settings

### Problem: Events stop after a while

**Solution:**
- Check MQTT broker is still running
- Check Wi-Fi connection via `/api/wifi/status`
- Restart device and monitor serial logs
- Check for I2C communication errors in logs

## Next Steps

- **Web UI Development**: Edit `components/main/web/index.html` to customize the interface
- **Register Tuning**: Use the Advanced Settings page to optimize sensor sensitivity
- **Calibration**: Run automatic calibration via the Calibration page
- **Automation**: Use REST API endpoints to integrate with home automation systems

## API Quick Reference

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/wifi/status` | GET | Get Wi-Fi connection status |
| `/api/mqtt/status` | GET | Get MQTT broker status |
| `/api/as3935/status` | GET | Get sensor status |
| `/api/as3935/registers/all` | GET | Read all sensor registers |
| `/api/events/stream` | GET | Real-time event stream (SSE) |

For complete API documentation, see [API_REFERENCE.md](API_REFERENCE.md).

## Getting Help

- Check [API_REFERENCE.md](API_REFERENCE.md) for detailed endpoint documentation
- Review [README.md](README.md) for feature overview
- Open a GitHub issue for bugs or questions
