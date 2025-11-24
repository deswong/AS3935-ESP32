# AS3935 Storm Event Monitoring Fix

## Problem
The register status was NOT being posted when a storm was active because:

1. **No Event Monitor Initialized**: The code only called `as3935_init()` which initializes the sensor but does NOT start the interrupt monitoring task
2. **No Event Handler Registered**: Even if events were generated, there was no handler registered to receive and process them
3. **Status Only Polled**: The `/api/as3935/status` endpoint only read register values when requested via HTTP GET - it didn't push updates when lightning events occurred

## Root Cause Analysis

The AS3935 library has two initialization modes:

- **`as3935_init()`**: Basic initialization - sets up I2C communication but does NOT monitor interrupts
- **`as3935_monitor_init()`**: Full initialization - creates an interrupt monitoring task, event loop, and processes GPIO interrupts to detect lightning

The code was using `as3935_init()`, so while the sensor was configured, **no events were being generated or processed** when lightning was detected.

## Solution Implemented

### 1. Added Event Monitoring Infrastructure

**File**: `components/main/as3935_adapter.c`

- Added `g_monitor_handle` to track the AS3935 monitor instance
- Added includes for MQTT and SSE event broadcasting:
  ```c
  #include "app_mqtt.h"
  #include "events.h"
  #include "settings.h"
  ```

### 2. Created Event Handler Function

Added `as3935_event_handler()` function that:
- Receives events from the AS3935 monitor task
- Processes three event types:
  - `AS3935_INT_LIGHTNING` - Lightning detected
  - `AS3935_INT_DISTURBER` - Electrical disturber detected
  - `AS3935_INT_NOISE` - Noise level too high
- Reads current register values (R0, R1, R3, R8) using non-blocking I2C
- Builds JSON payload with event data and register status
- Publishes to MQTT topic (configurable, default: `as3935/lightning`)
- Broadcasts via SSE for real-time web UI updates
- Calls legacy event callback if registered

**Example JSON payload published:**
```json
{
  "event": "lightning",
  "distance_km": 12,
  "energy": 5000,
  "r0": "0x00",
  "r1": "0x22",
  "r3": "0x08",
  "r8": "0x0C",
  "timestamp": 1234567
}
```

### 3. Replaced Sensor Init with Monitor Init

Changed `as3935_init_sensor_handle()` to:
- Use `as3935_monitor_init()` instead of `as3935_init()`
- This creates the interrupt monitoring task and event loop
- Extracts the sensor handle from monitor context for compatibility
- Registers the event handler via `as3935_monitor_add_handler()`

**Before:**
```c
esp_err_t ret = as3935_init(g_i2c_bus, &lib_config, &g_sensor_handle);
```

**After:**
```c
esp_err_t ret = as3935_monitor_init(g_i2c_bus, &lib_config, &g_monitor_handle);
// Extract sensor handle from monitor
as3935_monitor_context_t *monitor_ctx = (as3935_monitor_context_t *)g_monitor_handle;
g_sensor_handle = monitor_ctx->as3935_handle;
// Register event handler
ret = as3935_monitor_add_handler(g_monitor_handle, as3935_event_handler, NULL);
```

### 4. Updated Status Check

Modified `as3935_status_handler()` to check for both sensor AND monitor handles:
```c
bool sensor_ok = g_initialized && (g_i2c_bus != NULL) && 
                 (g_sensor_handle != NULL) && (g_monitor_handle != NULL);
```

## How It Works Now

### Event Flow When Lightning is Detected:

1. **GPIO Interrupt** → AS3935 IRQ pin triggers interrupt
2. **Monitor Task** → Wakes up, reads interrupt cause register
3. **Event Posted** → Posts event to ESP event loop with event type and data
4. **Event Handler** → `as3935_event_handler()` receives the event
5. **Read Registers** → Handler reads R0, R1, R3, R8 via non-blocking I2C
6. **Build Payload** → Creates JSON with event data and register values
7. **MQTT Publish** → Publishes to configured MQTT topic
8. **SSE Broadcast** → Sends to web clients via Server-Sent Events
9. **Legacy Callback** → Calls old event callback if set

### Real-Time Updates:

- **MQTT subscribers** receive lightning events immediately
- **Web UI** gets live updates via SSE stream
- **Register values** included in every event for complete status

## Benefits

✅ **Automatic Event Publishing**: No polling needed - events pushed immediately when detected  
✅ **Complete Register Status**: All relevant registers (R0, R1, R3, R8) included in event data  
✅ **Multiple Outputs**: Both MQTT and SSE broadcasting for flexibility  
✅ **Non-Blocking I2C**: Safe to call from event handler context  
✅ **Backward Compatible**: Existing HTTP endpoints and legacy callbacks still work  

## Testing

To verify the fix works:

1. **Build and flash** the updated firmware
2. **Configure MQTT** broker in web UI
3. **Subscribe** to the MQTT topic:
   ```bash
   mosquitto_sub -h <broker> -t 'as3935/lightning' -v
   ```
4. **Trigger lightning** detection (or disturber event)
5. **Observe** real-time events published with register status

## Files Modified

- `components/main/as3935_adapter.c` - Added event monitoring system

## Related Documentation

- `INTERRUPT_LOGGING_GUIDE.md` - How to monitor event flow via logs
- `AS3935_REGISTER_API.md` - Register meanings and values
- `README.md` - MQTT payload format

---

**Status**: ✅ Complete - Register status now publishes automatically when storms are active  
**Build**: Ready for testing  
**Impact**: Enables real-time lightning detection with full register status reporting
