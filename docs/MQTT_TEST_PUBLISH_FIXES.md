# MQTT Test Publish - Fixes and Improvements

## Issues Addressed

1. **Test publish was silently failing** - No feedback to user about why the test didn't work
2. **MQTT connection state unknown** - Users couldn't tell if MQTT was actually connected
3. **No clear error messages** - Generic errors didn't help users troubleshoot
4. **Missing validation** - No checks if broker URI or topic were configured

## Changes Made

### 1. MQTT Connection Status Checking
- **Added `mqtt_is_connected()` function** - Checks actual MQTT client connection state
- **Updated `/api/mqtt/status` endpoint** - Now returns `"connected": true/false` flag
- **Added connection status display in web UI** - Shows "Connected ✓", "Connecting ⟳", or "Disconnected ✗"

### 2. Improved Test Publish Handler
- **Added broker URI validation** - Checks if MQTT is configured before attempting publish
- **Added topic validation** - Ensures topic is configured
- **Better error responses** - Returns specific error codes and messages:
  - `no_mqtt_broker_configured` - No broker URI set
  - `no_topic_configured` - No topic configured
  - `mqtt_not_connected` - Broker URI and topic set but not connected yet

### 3. Enhanced Web UI
- **MQTT Status Indicator** - Visual status box showing connection state with color coding:
  - 🟢 Green = Connected and ready
  - 🟠 Orange = Connecting (normal after save)
  - 🔴 Red = Not configured or disconnected
  
- **New "Refresh Status" Button** - Manually check connection status
- **Better error messages** - User-friendly explanations when test publish fails
- **Clearer button labels** - Distinguishes between "Save MQTT" and "Test Publish"

### 4. Improved Error Logging
- **Enhanced mqtt_publish() function** - Logs more details about why publish failed
- **Better logging messages** - Indicates if client isn't initialized or connection isn't ready

## How to Use

### Typical Workflow:
1. **Configure MQTT** - Enter broker URI (e.g., `mqtt://broker.com:1883`) and topic
2. **Click "Save MQTT"** - Settings are saved to NVS
3. **Wait for connection** - Status will change to "Connecting ⟳" (takes 5-10 seconds typically)
4. **Click "Refresh Status"** - Once status shows "Connected ✓", you're ready
5. **Click "Test Publish"** - Will publish a test message to your broker

### Troubleshooting:
- **Status shows "Disconnected ✗" after saving** - Wait a bit longer, network connection may be slow
- **Test Publish shows "Not Configured"** - Make sure you've saved both broker URI and topic
- **Test Publish shows "Not Connected"** - MQTT is trying to connect but hasn't yet. Wait and retry.
- **Check mosquitto_sub** - Verify messages arrive:
  ```bash
  mosquitto_sub -h your-broker -t 'as3935/lightning'
  ```

## API Changes

### GET /api/mqtt/status
**New field**: `"connected": true/false`

Response example:
```json
{
  "configured": true,
  "uri": "mqtt://broker:1883",
  "use_tls": false,
  "topic": "as3935/lightning",
  "username": "user",
  "password_set": true,
  "password_masked": "********",
  "connected": true
}
```

### POST /api/mqtt/test
**Success response**:
```json
{
  "ok": true,
  "message": "Test message published successfully"
}
```

**Error responses**:
```json
{
  "ok": false,
  "error": "no_topic_configured",
  "message": ""
}
```

or

```json
{
  "ok": false,
  "error": "mqtt_not_connected",
  "message": "MQTT client is not connected yet. Please wait a moment and try again."
}
```

## Next Steps

- Monitor your MQTT broker to see test messages arriving
- Configure AS3935 sensor to detect lightning events
- Messages will be published automatically when lightning is detected
