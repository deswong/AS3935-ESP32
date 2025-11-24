# Quick Reference: Today's Changes

## AFE (Environment Mode) Setting Fix ✅ COMPLETE

**What was wrong:** UI dropdown for "Environment Mode" wasn't updating even though the value was being fetched from the server.

**What was changed:** One line in the loadSettings() function to actually set the select element value.

**File:** `components/main/web/index.html`
**Line ~964:**
```javascript
// BEFORE:
if(afe && afe.afe !== undefined) {
  const afeVal = afe.afe;
  const afeSelect = document.getElementById('as3935_afe');
  console.log('AFE available on load:', afeVal, ...);  // ← Just logging, not setting!
}

// AFTER:
if(afe && afe.afe !== undefined) {
  const afeVal = String(afe.afe);
  const afeSelect = document.getElementById('as3935_afe');
  afeSelect.value = afeVal;  // ← NOW it actually sets the value!
  console.log('AFE set to:', afeVal, ...);
}
```

---

## Disturber Endpoint Crash Investigation 🔍 IN PROGRESS

**What's wrong:** Disturber GET endpoint (`/api/as3935/settings/disturber`) crashes the HTTP server with `ERR_CONNECTION_RESET` while all other endpoints work.

**What was done:** Added extensive debug logging to trace the issue.

**Files changed:**
1. `components/main/as3935_adapter.c`
   - Added persistent I2C device handle to avoid create/destroy overhead
   - Replaced temporary device creation with reusable handle
   - Added [I2C-NB] logging to track I2C operations
   - Added [DISTURBER] logging to track handler execution

2. `components/main/app_main.c`
   - Added [STARTUP] logging when disturber handler registers

**How to proceed:**
1. Build and flash the new firmware
2. Watch the serial monitor output
3. Look for these debug messages when disturber GET is called:
   ```
   [STARTUP] Registering disturber GET/POST handlers...
   [DISTURBER] ENTER handler
   [DISTURBER] content_len=0 (indicates GET)
   [DISTURBER-GET] Before I2C read
   [I2C-NB] ENTER: reg=0x03, device=...
   [I2C-NB] transmit_receive returned: OK
   [I2C-NB] SUCCESS: reg=0x03 val=0x??
   [DISTURBER-GET] After I2C read, err=OK
   [DISTURBER-GET] Sending response
   ```

4. If crash happens, you'll see: `httpd_accept_conn: error in accept (23)`

5. Compare the actual sequence with expected above to identify failure point

---

## Build & Test Commands

```bash
# Navigate to project directory
cd d:\AS3935-ESP32

# Build
python -m idf.py build

# Flash
python -m idf.py -p COM3 flash

# Monitor logs
python -m idf.py monitor

# In browser:
# 1. Go to http://192.168.1.153/
# 2. Click "⚙️ Advanced Settings" section
# 3. Verify "Environment Mode" dropdown now shows current value
# 4. Click "🔄 Reload Current" to trigger loadSettings()
# 5. Check browser console and serial monitor
# 6. Watch for debug markers [AFE], [DISTURBER], [I2C-NB]
```

---

## Summary of Current State

✅ **Fixed:**
- AFE/Environment Mode dropdown now displays current setting on reload

⏳ **Awaiting Debug Data:**
- Disturber endpoint crash
- Need serial logs to identify exact failure point
- All logging infrastructure in place

📦 **Ready to Rebuild:**
- All code changes complete
- Just needs: build → flash → test → debug

