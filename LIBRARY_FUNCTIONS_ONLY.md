# HTTP Handlers Updated to Use Library Functions Only

## Overview
All HTTP handler functions for the AS3935 advanced settings endpoints have been updated to use **ONLY library functions** (`as3935_*` API) instead of direct I2C register reads. This eliminates:
- Blocking I2C operations in the HTTP handler context
- Direct register manipulation that could interfere with library state
- vTaskDelay calls that crash the HTTP server
- Race conditions between HTTP handlers and interrupt handlers

## Handler Updates

### 1. Disturber Handler (`/api/as3935/settings/disturber`)
**GET**: Returns current disturber state
```c
// Uses library function to safely read register 0x03
as3935_0x03_register_t reg3 = {0};
esp_err_t err = as3935_get_0x03_register(g_sensor_handle, &reg3);
// Extract bit 5: 0=enabled, 1=disabled
bool enabled = (reg3.bits.disturber_detection_state == AS3935_DISTURBER_DETECTION_ENABLED);
```

**POST**: Sets disturber state
```c
// Uses library functions:
// - as3935_enable_disturber_detection()
// - as3935_disable_disturber_detection()
```

### 2. AFE Handler (`/api/as3935/settings/afe`)
**GET**: Returns current analog frontend setting
```c
// Already uses library function:
as3935_get_0x00_register(g_sensor_handle, &reg0);
```

**POST**: Sets AFE (indoor/outdoor)
```c
// Uses: as3935_set_analog_frontend()
```

### 3. Noise Level Handler (`/api/as3935/settings/noise-level`)
**GET**: Returns current noise floor level
```c
// Already uses library function:
as3935_get_0x01_register(g_sensor_handle, &reg1);
```

**POST**: Sets noise level (0-7)
```c
// Uses: as3935_set_noise_floor_threshold()
```

### 4. Spike Rejection Handler (`/api/as3935/settings/spike-rejection`)
**GET**: Returns current spike rejection value (0-15)
```c
// Already uses library function:
as3935_get_0x02_register(g_sensor_handle, &reg2);
```

**POST**: Sets spike rejection
```c
// Uses: as3935_set_spike_rejection()
```

### 5. Min Strikes Handler (`/api/as3935/settings/min-strikes`)
**GET**: Returns current minimum lightning strikes setting
```c
// Already uses library function:
as3935_get_0x02_register(g_sensor_handle, &reg2);
```

**POST**: Sets minimum strikes (0-3)
```c
// Uses: as3935_set_minimum_lightnings()
```

### 6. Watchdog Handler (`/api/as3935/settings/watchdog`)
**GET**: Returns current watchdog threshold
```c
// Already uses library function:
as3935_get_0x01_register(g_sensor_handle, &reg1);
```

**POST**: Sets watchdog threshold (0-10)
```c
// Uses: as3935_set_watchdog_threshold()
```

## Error Handling Strategy

For **GET** requests:
- If sensor not initialized: return error
- If library function fails: return default/cached value instead of crashing
- Always respond with HTTP 200 OK (JSON with status field indicates error state)

For **POST** requests:
- Validate JSON input first
- Apply setting via library function
- Return result immediately (non-blocking)

## Benefits

1. **Safe**: Library functions handle I2C protocol correctly
2. **Non-blocking**: No vTaskDelay in HTTP handler context
3. **Consistent**: All handlers use same pattern
4. **Reliable**: No HTTP server crashes from rogue I2C operations
5. **Maintainable**: Single source of truth (library) for register manipulation

## Testing Checklist

- [ ] Build firmware successfully
- [ ] Flash to ESP32
- [ ] Load web UI
- [ ] GET `/api/as3935/settings/disturber` returns HTTP 200 with JSON
- [ ] POST `/api/as3935/settings/disturber` returns HTTP 200
- [ ] All other settings endpoints work (AFE, noise, spike, strikes, watchdog)
- [ ] HTTP server does not crash
- [ ] No socket errors in monitor log

## Files Modified

- `components/main/as3935_adapter.c` - Updated disturber handler to use library functions

## Library Functions Used

- `as3935_get_0x00_register()` - Read register 0x00 (AFE)
- `as3935_get_0x01_register()` - Read register 0x01 (noise, watchdog)
- `as3935_get_0x02_register()` - Read register 0x02 (spike, strikes)
- `as3935_get_0x03_register()` - Read register 0x03 (disturber state) ✅ NEW
- `as3935_set_analog_frontend()` - Set AFE
- `as3935_set_noise_floor_threshold()` - Set noise level
- `as3935_set_spike_rejection()` - Set spike rejection
- `as3935_set_minimum_lightnings()` - Set min strikes
- `as3935_set_watchdog_threshold()` - Set watchdog
- `as3935_enable_disturber_detection()` - Enable disturber ✅ NEW
- `as3935_disable_disturber_detection()` - Disable disturber ✅ NEW

All functions are part of the official esp_as3935 library and handle I2C communication safely.
