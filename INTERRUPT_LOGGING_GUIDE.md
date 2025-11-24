# Interrupt & MQTT Publishing Logging Guide

## Overview
This document describes the logging infrastructure added to trace the entire flow from GPIO interrupt trigger through MQTT publication.

## Logging Flow Path

### 1. **GPIO Interrupt Triggered** (as3935.c, line 73-78)
```
[IRQ] GPIO interrupt triggered on pin XXX at YYY ms
```
- **When**: Every time the AS3935 sensor triggers an interrupt (lightning detected)
- **Location**: `gpio_isr_handler()` ISR function
- **Information**: Pin number and timestamp when interrupt occurred

### 2. **Background Task Woken Up** (as3935.c, line 157)
```
[TASK] gpio_task woken up by interrupt on pin XXX at YYY ms
```
- **When**: Background task receives queue event from ISR
- **Location**: `gpio_task()` function
- **Information**: Pin number and timestamp

### 3. **Sensor Data Reading** (as3935.c, line 161)
```
[TASK] Sensor data: energy=EEEE, distance=DDD km, has_data=H
```
- **When**: Task reads energy and distance from sensor via non-blocking I2C
- **Location**: After calling `as3935_get_sensor_energy()` and `as3935_get_sensor_distance()`
- **Information**: 
  - `energy`: Storm intensity (0-15)
  - `distance`: Distance to lightning in km
  - `has_data`: Whether both I2C reads succeeded

### 4. **Event Publishing Started** (as3935.c, line 191)
```
[MQTT] as3935_publish_event: publishing event
```
- **When**: Task calls publish function with sensor data
- **Location**: `as3935_publish_event()` function
- **Information**: Confirms publish was called

### 5. **MQTT Topic Publishing** (as3935.c, line 318)
```
[MQTT] Publishing to topic 'as3935/event': {"distance_km":DDD,"energy":EEEE,"timestamp":TTTT}
```
- **When**: Publishes formatted JSON to MQTT broker
- **Location**: `as3935_publish_event_json()` function
- **Information**: Topic name and full JSON payload

### 6. **Low-Level MQTT Publish** (mqtt_client.c, line 108-109)
```
[MQTT-PUB] Attempting publish: connected=C, topic='TOPIC', payload='PAYLOAD'
```
- **When**: Low-level MQTT library call is made
- **Location**: `mqtt_publish()` function
- **Information**: 
  - `connected`: MQTT connection status (1=connected, 0=not connected)
  - Topic and payload being sent

### 7. **MQTT Publish Result** (mqtt_client.c, line 115 or 118)

**Success:**
```
[MQTT-PUB] Success: msg_id NNN published to topic 'TOPIC'
```

**Failure:**
```
[MQTT-PUB] Failed: msg_id=-1 (client may not be connected yet, connected=C)
```

- **When**: Result of MQTT publish operation
- **Location**: `mqtt_publish()` function
- **Information**:
  - `msg_id`: Message ID from MQTT library (-1 = error)
  - `connected`: MQTT connection state at time of publish

### 8. **Publish Call Completed** (as3935.c, line 320)
```
[MQTT] Publish call completed
```
- **When**: After MQTT publish attempt
- **Location**: `as3935_publish_event_json()` function
- **Information**: Marks completion of publish call

## Expected Log Sequence for a Lightning Strike

When lightning is detected, you should see this sequence:

```
I (12345) as3935_adapter: [IRQ] GPIO interrupt triggered on pin 23 at 12345 ms
I (12346) as3935: [TASK] gpio_task woken up by interrupt on pin 23 at 12346 ms
I (12347) as3935_adapter: [I2C-NB] ENTER: reg=0x04, device=0x3fcb5040  ← Reading energy
I (12348) as3935_adapter: [I2C-NB] SUCCESS: reg=0x04 val=0xA0
I (12348) as3935_adapter: [I2C-NB] EXIT: returning ESP_OK
I (12349) as3935_adapter: [I2C-NB] ENTER: reg=0x03, device=0x3fcb5040  ← Reading distance
I (12349) as3935_adapter: [I2C-NB] SUCCESS: reg=0x03 val=0x05
I (12349) as3935_adapter: [I2C-NB] EXIT: returning ESP_OK
I (12350) as3935: [TASK] Sensor data: energy=10, distance=5 km, has_data=1
I (12351) as3935: [MQTT] as3935_publish_event: publishing event
I (12351) as3935: [MQTT] Publishing to topic 'as3935/event': {"distance_km":5,"energy":10,"timestamp":12350}
I (12352) mqtt_client: [MQTT-PUB] Attempting publish: connected=1, topic='as3935/event', payload='...'
I (12353) mqtt_client: [MQTT-PUB] Success: msg_id 123 published to topic 'as3935/event'
I (12353) as3935: [MQTT] Publish call completed
```

## Troubleshooting

### No `[IRQ]` logs appearing
- **Cause**: GPIO interrupt not being triggered by sensor
- **Check**: 
  1. Verify IRQ pin (GPIO 23) is correctly connected to AS3935 INT pin
  2. Check if `as3935_setup_irq()` was called successfully during initialization
  3. Look for "[GPIO-IRQ] Setup completed successfully" log during boot

### `[TASK]` appears but no `[MQTT]` logs
- **Cause**: Sensor data read failing or sensor not initialized
- **Check**:
  1. Look for `[I2C-NB]` logs showing I2C failures
  2. Verify sensor is properly initialized

### `[MQTT]` logs show but `[MQTT-PUB]` shows `connected=0`
- **Cause**: MQTT client not connected to broker
- **Check**:
  1. Verify WiFi is connected (look for wifi connection logs)
  2. Verify MQTT broker address and credentials in config
  3. Check for MQTT connection error logs

### `[MQTT-PUB]` shows success but message not received by broker
- **Cause**: Message queued but not yet transmitted, or broker issue
- **Check**:
  1. Wait a few seconds for network transmission
  2. Verify broker is receiving messages from other sources
  3. Check MQTT topic subscription on client side

## Log Tag Organization

All logs use these tags for easy filtering:

- `[IRQ]` - GPIO interrupt event
- `[TASK]` - Background task processing
- `[I2C-NB]` - I2C read/write operations
- `[MQTT]` - High-level MQTT event publishing
- `[MQTT-PUB]` - Low-level MQTT client publish call

## Viewing Logs

To filter for interrupt/MQTT logs only:
```bash
# View all interrupt and MQTT logs
idf.py monitor | grep -E "\[IRQ\]|\[TASK\]|\[MQTT\]"

# View only MQTT publish attempts
idf.py monitor | grep "\[MQTT-PUB\]"

# View complete interrupt-to-publish flow
idf.py monitor | grep -E "\[IRQ\]|\[TASK\]|\[MQTT\]"
```
