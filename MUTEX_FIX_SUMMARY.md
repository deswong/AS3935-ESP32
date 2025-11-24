# I2C Mutex Synchronization Fix

## Problem Analysis

### Root Cause
The browser's `loadSettings()` function makes multiple concurrent HTTP requests:
- AFE, Noise, Spike, Strikes, Watchdog, Disturber, Status, etc.

These requests may be processed by the HTTP server in rapid succession or concurrently, causing multiple handlers to execute at nearly the same time. All handlers use the same persistent I2C device handle (`g_i2c_device`) to read sensor registers.

**Race Condition:**
```
Thread A (AFE handler):     reads 0x00 -> sends HTTP response
Thread B (Noise handler):   reads 0x01 (while A is sending)
                             Both access g_i2c_device simultaneously!
I2C bus state corrupted -> HTTP socket corrupted -> error 23 (EBADF)
```

### Initial Symptoms
- Browser console: `ERR_CONNECTION_RESET` on disturber endpoint
- ESP logs: `httpd_accept_conn: error in accept (23)` 
- Error 23 = EBADF = "Bad file descriptor" (socket corruption)

## Solution: I2C Mutex

### Implementation
Added FreeRTOS binary semaphore mutex to serialize all I2C access:

**1. Added mutex global:**
```c
static SemaphoreHandle_t g_i2c_mutex = NULL;
```

**2. Initialize in bus_init:**
```c
if (!g_i2c_mutex) {
    g_i2c_mutex = xSemaphoreCreateMutex();
    if (!g_i2c_mutex) {
        ESP_LOGE(TAG, "[INIT] FAILED to create I2C mutex");
        return false;
    }
    ESP_LOGI(TAG, "[INIT] I2C mutex created for thread-safe I2C operations");
}
```

**3. Wrap I2C operations:**
```c
// Acquire mutex (5s timeout)
if (xSemaphoreTake(g_i2c_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    return ESP_ERR_TIMEOUT;
}

// Single atomic I2C transaction
esp_err_t ret = i2c_master_transmit_receive(g_i2c_device, &tx_buf, 1, &rx_buf, 1, 500);

// Release mutex immediately
xSemaphoreGive(g_i2c_mutex);

// Log AFTER releasing to minimize hold time
```

### Key Optimization
Logging was moved **outside** the mutex-protected section to minimize the time the mutex is held. This prevents HTTP send buffers from timing out while waiting for the I2C mutex.

## Build & Flash

```bash
# Build
cd d:\AS3935-ESP32
python -m idf.py build

# Flash
python -m idf.py -p COM3 flash

# Monitor
python -m idf.py monitor
```

## Expected Behavior After Fix

### Browser Logs
Should see all endpoints return 200 OK:
```
AFE response status: 200 true
Noise response status: 200 true  
Spike response status: 200 true
Strikes response status: 200 true
Watchdog response status: 200 true
Disturber response status: 200 true  ← This was failing before
Status response status: 200 true
```

### Serial Logs
Should see mutex acquire/release around I2C operations:
```
I (16391) as3935_adapter: [I2C-NB] ENTER: reg=0x03
I (16391) as3935_adapter: [I2C-NB] Starting transmit_receive
I (16391) as3935_adapter: [I2C-NB] transmit_receive returned: ESP_OK
I (16391) as3935_adapter: [I2C-NB] SUCCESS: reg=0x03 val=0x00
I (16391) as3935_adapter: [I2C-NB] EXIT: returning ESP_OK
```

NO more:
- `httpd_accept_conn: error in accept (23)` ✓
- `ERR_CONNECTION_RESET` ✓
- `Failed to load resource` ✓

## If Issues Persist

### Error 11 (EAGAIN) on send
Means HTTP send buffer timeout. If you see:
```
W (26931) httpd_txrx: httpd_sock_err: error in send : 11
```

**Solution:** The logging deferral should help. If not, can increase HTTP server send timeout:
- Check `sdkconfig` for `CONFIG_HTTPD_MAX_REQ_HDR_LEN` and `CONFIG_HTTPD_RECV_BUF_SIZE`
- Increase recv buffer size if handlers take longer to complete

### Still Getting Socket Errors
If socket errors persist:
1. Check mutex is being created (log: "[INIT] I2C mutex created")
2. Verify no timeout on mutex acquire (would log "[I2C-NB] ERROR: Failed to acquire I2C mutex")
3. Check if other I2C users exist outside the adapter (search for `i2c_master_transmit` in codebase)

## Performance Notes
- Mutex adds negligible overhead (microseconds per lock/unlock)
- I2C transactions are fast (~1-2ms), so other tasks won't block long
- 5-second timeout is very conservative; typical mutex hold <10ms
- Logging deferral ensures HTTP thread isn't blocked by ESP_LOGI()

## Files Modified
- `components/main/as3935_adapter.c`
  - Added `#include "freertos/semphr.h"`
  - Added `static SemaphoreHandle_t g_i2c_mutex`
  - Modified `as3935_adapter_bus_init()` to create mutex
  - Modified `as3935_i2c_read_byte_nb()` to use mutex
  - Moved logging outside mutex critical section
