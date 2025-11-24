# AS3935 Disturber Endpoint & AFE Setting - Debug Summary

## Issues Fixed

### 1. AFE (Environment Mode) Setting Not Updating on UI
**Problem:** The AFE value was being fetched from `/api/as3935/settings/afe` and appeared in console logs, but the select element on the UI was not being updated.

**Root Cause:** In `loadSettings()` function, the code retrieved the AFE value but never actually set it on the select element.

**Fix Applied:** 
- Modified `components/main/web/index.html` line ~964
- Changed from just logging the value to actually setting `afeSelect.value = afeVal`
- Converted value to string to ensure proper select matching
- Regenerated web header via `embed_web.py`

**Files Modified:**
- `components/main/web/index.html` - loadSettings() function
- `components/main/include/web_index.h` - regenerated with updated HTML

---

### 2. Disturber Endpoint Still Crashing (ONGOING DEBUG)
**Problem:** GET request to `/api/as3935/settings/disturber` returns `ERR_CONNECTION_RESET` in browser while other endpoints (AFE, noise, spike, strikes, watchdog) work fine.

**Root Cause:** Under investigation. Initial fixes for vTaskDelay blocking calls applied but issue persists.

**Hypothesis:** 
- Might be specific to register 0x03 reads
- Possible I2C device handle creation/destruction issue
- May be a race condition or stack corruption

**Debug Logging Added:**
Extensive logging added to trace execution flow:

1. **[INIT]** - I2C persistent device handle creation during startup
2. **[STARTUP]** - Disturber handler registration 
3. **[I2C-NB]** - Non-blocking I2C read entry/exit with values and error codes
4. **[AFE]** / **[DISTURBER]** / etc - Handler entry points
5. **[*-GET]** / **[*-POST]** - Request type detection

**Files Modified:**
- `components/main/as3935_adapter.c`:
  - Added persistent I2C device handle `g_i2c_device`
  - Modified `as3935_i2c_read_byte_nb()` with detailed logging
  - Modified `as3935_disturber_handler()` with entry/exit logging
  - Modified `as3935_afe_handler()` with entry/exit logging
  - Modified bus init to create persistent device handle
  
- `components/main/app_main.c`:
  - Added startup logging for disturber handler registration

---

## Next Steps

1. **Build and Flash Firmware**
   ```bash
   python -m idf.py build
   python -m idf.py -p COM3 flash
   ```

2. **Test and Collect Logs**
   - Monitor serial output via `python -m idf.py monitor`
   - Reload web UI: `/`
   - Click "Reload Current" settings button
   - Watch for detailed debug output

3. **Interpret Log Output**
   Look for these patterns in the output:
   - `[INIT]` logs should show persistent device handle created successfully
   - `[STARTUP]` logs should show disturber handlers registered
   - `[DISTURBER] ENTER handler` should appear when GET request is made
   - `[I2C-NB]` logs will show if I2C transaction succeeds/fails
   - `httpd_accept_conn: error in accept (23)` indicates socket corruption

4. **Possible Issues to Investigate**
   - Check if `[I2C-NB] ENTER: reg=0x03` logs appear
   - Check if I2C transmit_receive returns successfully for reg 0x03
   - Compare timing/behavior between working (0x00, 0x01) vs failing (0x03) registers
   - Monitor for stack overflow or memory corruption

---

## Code Changes Summary

### HTTP Handler Pattern (All GET endpoints now use)
```c
// Non-blocking I2C read - safe for HTTP context
uint8_t reg_val = 0;
esp_err_t err = as3935_i2c_read_byte_nb(REG_ADDR, &reg_val);

if (err != ESP_OK) {
    // Handle error gracefully
    return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"read_failed\"}");
}

// Parse bits from register value
// ... extract specific bits as needed ...

// Return JSON response
return http_reply_json(req, json_response);
```

### I2C Read Helper Function
- Uses persistent device handle (no create/destroy per call)
- Single I2C transaction without retries or delays
- Returns immediately with 500ms I2C-level timeout
- Safe for HTTP handler context (non-blocking)

---

## Testing Checklist

- [ ] AFE setting appears on reload
- [ ] Other settings (noise, spike, strikes, watchdog) still work
- [ ] Disturber GET endpoint returns valid JSON
- [ ] No `ERR_CONNECTION_RESET` errors
- [ ] Serial logs show all debug markers
- [ ] HTTP server remains stable throughout
- [ ] Settings can be changed via POST endpoints
- [ ] Device can be rebooted successfully

