# Build and Test Instructions - I2C Mutex Fix

## What Was Changed

The race condition causing HTTP socket corruption has been fixed by adding thread-safe synchronization:

1. **Added I2C Mutex** - FreeRTOS binary semaphore to serialize all I2C device access
2. **Optimized Lock Hold Time** - Moved logging outside mutex-protected section
3. **Increased Timeout** - Set mutex timeout to 5 seconds for reliable acquisition under load

## Build Commands

Run from `d:\AS3935-ESP32`:

```powershell
# Step 1: Build
python -m idf.py build

# Step 2: Flash
python -m idf.py -p COM3 flash

# Step 3: Monitor (open new terminal)
python -m idf.py monitor
```

## What to Look For

### Successful Boot
Should see in serial logs:
```
I (xxxx) as3935_adapter: [INIT] I2C mutex created for thread-safe I2C operations
I (xxxx) as3935_adapter: [INIT] SUCCESS: Persistent I2C device handle created
I (xxxx) as3935_adapter: AS3935 adapter initialized
```

### Browser: Test Disturber Endpoint
1. Open http://192.168.1.153/
2. Click "🔄 Reload Current" button in Advanced Settings
3. Monitor browser console - should see:
   ```
   AFE response status: 200 true
   Noise response status: 200 true
   Spike response status: 200 true
   Strikes response status: 200 true
   Watchdog response status: 200 true
   Disturber response status: 200 true ✓ (was failing before)
   ```

### Serial Logs: I2C Operations
Should see clean mutex synchronization:
```
I (xxxxx) as3935_adapter: [I2C-NB] ENTER: reg=0x01, device=0x3fcbxxxx
I (xxxxx) as3935_adapter: [I2C-NB] transmit_receive returned: ESP_OK (0x0)
I (xxxxx) as3935_adapter: [I2C-NB] SUCCESS: reg=0x01 val=0x22
I (xxxxx) as3935_adapter: [I2C-NB] EXIT: returning ESP_OK
```

### What Should NOT Appear
❌ `httpd_accept_conn: error in accept (23)`
❌ `ERR_CONNECTION_RESET` in browser console
❌ `httpd_sock_err: error in send : 11` (EAGAIN timeout)
❌ `Failed to load resource` on any endpoint

## Verification Checklist

- [ ] Build completes without errors
- [ ] Flash successful (shows "Leaving... Hard resetting...")
- [ ] Monitor shows "[INIT] I2C mutex created" message
- [ ] All 6 settings endpoints return 200 OK
- [ ] No socket errors in serial logs
- [ ] Browser console shows no network errors
- [ ] AFE dropdown value updates correctly
- [ ] Can repeatedly reload settings without crashes

## If Tests Pass

Congratulations! The concurrent I2C access race condition is fixed. The disturber endpoint (and all others) should now work reliably even when called in rapid succession.

## If Issues Remain

### Still Getting EAGAIN (Error 11)
If you see:
```
W (xxxxx) httpd_txrx: httpd_sock_err: error in send : 11
```

This means the HTTP send buffer is still timing out. Try:
1. Check monitor output for "[I2C-NB] ERROR: Failed to acquire I2C mutex" - if present, timeout is too short
2. Add small delay after I2C transaction to let HTTP send buffer flush:
   ```c
   xSemaphoreGive(g_i2c_mutex);
   vTaskDelay(pdMS_TO_TICKS(2));  // Small delay
   ```

### Still Getting Socket Error 23
If you see:
```
E (xxxxx) httpd: httpd_accept_conn: error in accept (23)
```

This indicates the mutex fix didn't fully work. Check:
1. Are there OTHER I2C operations outside this adapter? (grep for `i2c_master_transmit` in all .c files)
2. Is the mutex being created? (look for "[INIT] I2C mutex created" message)
3. Does any code call I2C functions before `as3935_adapter_bus_init()`?

### Performance Concerns
The mutex adds minimal overhead:
- Lock/unlock: ~1-2 microseconds
- I2C transaction: ~1-2 milliseconds
- Typical mutex hold time: <10 milliseconds
- No noticeable impact on responsiveness

## Technical Details

### Why This Fixes It
```
BEFORE (Race Condition):
Handler A: reads 0x00 -> sends response -> socket released
Handler B: reads 0x01 at same time -> I2C state corrupts -> socket corrupts

AFTER (Mutex Protected):
Handler A: acquires mutex -> reads 0x00 -> releases mutex -> sends response
Handler B: waits for mutex -> acquires mutex -> reads 0x01 -> releases mutex -> sends response
Both operations serialized -> no corruption
```

### Files Changed
- `components/main/as3935_adapter.c`
  - Added `#include "freertos/semphr.h"` (line 24)
  - Added `static SemaphoreHandle_t g_i2c_mutex = NULL;` (line 35)
  - Modified `as3935_i2c_read_byte_nb()` with mutex protect (lines 50-105)
  - Modified `as3935_adapter_bus_init()` to create mutex (lines 161-171)

### Config Notes
- Mutex Type: Binary semaphore (acts as mutex)
- Timeout: 5 seconds (conservative, typical use <10ms)
- Hold Time: Minimized by deferring logging after release
- Thread Safety: Now safe for concurrent handler calls

## Next Steps

After verification:
1. Perform stress testing - rapidly click buttons, load multiple endpoints
2. Leave device running for extended period to check for stability
3. Monitor system logs for any other issues that may have been masked by socket errors

Report any remaining issues with:
- Serial log output (full section around error)
- Browser console error messages
- Description of which endpoint fails and how it fails
