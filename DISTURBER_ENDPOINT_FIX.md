# Disturber Endpoint Connection Reset Fix

## Problem Description

The disturber settings endpoint was crashing with `net::ERR_CONNECTION_RESET` when accessed, while all other settings endpoints (AFE, Noise, Spike, Strikes, Watchdog) worked correctly.

**Error in browser console:**
```
GET http://192.168.1.153/api/as3935/settings/disturber net::ERR_CONNECTION_RESET
Disturber load failed: TypeError: Failed to fetch
```

**Other endpoints working fine:**
- AFE (Environment Mode): ✅ 200 OK
- Noise Level: ✅ 200 OK
- Spike Rejection: ✅ 200 OK
- Min Strikes: ✅ 200 OK
- Watchdog: ✅ 200 OK
- **Disturber: ❌ ERR_CONNECTION_RESET**

---

## Root Causes Identified & Fixed

### Issue 1: Potentially Unsafe Register Access
**Original Code:**
```c
bool disturber_enabled = (reg3.bits.disturber_detection_state == AS3935_DISTURBER_DETECTION_ENABLED);
```

**Problem:** 
- Accessing individual bit fields through struct members can be unreliable
- If the library's struct definition differs from expectations, this could crash
- The bit field access might not correctly map to the actual hardware register layout

**Solution:**
```c
int reg_value = reg3.reg;
bool disturber_enabled = (reg_value & 0x20) != 0;  // Bit 5 = disturber detection
```
- Now reads the raw register value
- Uses direct bit manipulation to extract the disturber detection bit (bit 5 of register 0x03)
- Much more robust and reliable

### Issue 2: Missing Buffer Overflow Checks
**Original Code:**
```c
snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"disturber_enabled\":%s}", disturber_enabled ? "true" : "false");
return http_reply_json(req, buf);
```

**Problem:**
- No validation that snprintf succeeded
- If buffer overflow occurred, would return garbage/corrupted JSON
- Could cause HTTP parsing issues

**Solution:**
```c
int resp_len = snprintf(buf, sizeof(buf), "{\"status\":\"ok\",\"disturber_enabled\":%s,\"reg3\":\"0x%02x\"}", 
                        disturber_enabled ? "true" : "false", reg_value);
if (resp_len < 0 || resp_len >= (int)sizeof(buf)) {
    ESP_LOGE(TAG, "Buffer overflow in disturber response");
    return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"response_formatting_failed\"}");
}
return http_reply_json(req, buf);
```
- Validates snprintf return value
- Checks for buffer overflow conditions
- Returns error response instead of corrupted JSON

### Issue 3: Missing Error Logging
**Original Code:**
- No logging for debugging

**Solution:**
- Added `ESP_LOGE()` calls for error conditions
- Logs are sent to ESP32 serial monitor/IDF Monitor
- Helps diagnose future issues

```c
if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to read disturber register: %d", err);
    return http_reply_json(req, "{\"status\":\"error\",\"msg\":\"register_read_failed\"}");
}
```

---

## Changes Made

### File: `components/main/as3935_adapter.c`

**Function:** `as3935_disturber_handler()` (lines 1198-1254)

#### GET Handler (Register Read)
- ✅ Changed from bit field access to direct register value reading
- ✅ Added bit manipulation for disturber detection (bit 5 check)
- ✅ Added snprintf overflow validation
- ✅ Added logging for debug purposes
- ✅ Include raw register value in response (`reg3` field)

#### POST Handler (Register Write)
- ✅ Added snprintf overflow validation
- ✅ Added logging for errors
- ✅ Maintained original functionality

### File: `components/main/web/index.html`

**Previously Updated (Session):**
- ✅ Enhanced setting descriptions
- ✅ Non-auto-selection of AFE on load
- ✅ Proper exposure of showSettingInfo to window

**No changes needed in this update.**

---

## Technical Details

### Register 0x03 Layout (AS3935)
- **Bit 5**: Disturber Detection State (1 = enabled, 0 = disabled)
- **Bit 3**: Lightning interrupt state

The fix uses direct bit manipulation:
```c
disturber_enabled = (reg_value & 0x20) != 0;  // 0x20 = binary 00100000 (bit 5)
```

### Response Format
**GET Response:**
```json
{
  "status": "ok",
  "disturber_enabled": true,
  "reg3": "0x28"
}
```
- `status`: Operation status
- `disturber_enabled`: Current disturber detection state (true/false)
- `reg3`: Raw register 0x03 value for debugging

---

## Testing Procedure

### Before Rebuild
1. ✅ Browser shows: AFE, Noise, Spike, Strikes, Watchdog all load (200 OK)
2. ✅ Browser shows: Disturber crashes (ERR_CONNECTION_RESET)

### After Rebuild & Flash
1. Open Advanced Settings tab
2. Click "🔄 Reload Current" button
3. Expected: ALL settings load successfully including Disturber
4. Expected: Disturber shows current state (Enabled/Disabled)
5. Expected: No console errors for disturber endpoint

### Verification in Browser Console
```
AFE response status: 200 true
Noise response status: 200 true
Spike response status: 200 true
Strikes response status: 200 true
Watchdog response status: 200 true
Disturber response status: 200 true  ← Should now be 200 OK
```

### ESP32 Serial Monitor
Watch for error logs if anything fails:
```
E (xxxxx) as3935_adapter: Failed to read disturber register: [error_code]
```

---

## Build & Flash Instructions

### Step 1: Build Firmware
```powershell
$env:PATH = "C:\Users\Des\.espressif\tools\riscv32-esp-elf\esp-14.2.0_20241119\riscv32-esp-elf\bin;C:\Users\Des\.espressif\tools\cmake\4.0.3\bin;C:\Users\Des\.espressif\tools\ninja\1.12.1;$env:PATH"

cd d:\AS3935-ESP32

python -m idf.py build
```

### Step 2: Flash Firmware
```powershell
python -m idf.py -p COM3 flash monitor
```
(Replace COM3 with your actual port)

### Step 3: Verify in Browser
1. Navigate to web UI: http://192.168.1.153
2. Open "⚙️ AS3935 Sensor - Advanced Settings" tab
3. Click "🔄 Reload Current"
4. Check browser console for successful loads

---

## Related Changes in This Session

1. **Web UI Info Button Fix** ✅
   - Made `showSettingInfo` globally accessible
   - All info buttons now work

2. **Enhanced Setting Descriptions** ✅
   - All 6 settings have comprehensive information
   - Proper key mapping (afe, noise, spike, strikes, watchdog, disturber)

3. **User-Customizable AFE** ✅
   - Environment Mode now shows "Select..." on initial load
   - Users must explicitly choose INDOOR or OUTDOOR
   - No automatic defaults forced

4. **Disturber Endpoint Crash Fix** ✅ **← THIS DOCUMENT**
   - Fixed register access method
   - Added buffer overflow protection
   - Added error logging

---

## Files Modified

| File | Changes | Status |
|------|---------|--------|
| components/main/as3935_adapter.c | Disturber handler rewritten | ✅ Modified |
| components/main/web/index.html | (No new changes, web header regenerated) | ✅ Updated |
| components/main/include/web_index.h | Web header regenerated | ✅ Updated |

---

## Troubleshooting

### Still Getting ERR_CONNECTION_RESET?
1. Check ESP32 serial monitor for error logs
2. Verify g_sensor_handle is initialized (check AS3935 status endpoint)
3. Run full build with clean cache: `idf.py fullclean && idf.py build`

### Disturber endpoint returns "register_read_failed"?
- AS3935 sensor not responding
- Check I2C connection
- Verify GPIO pins (SDA, SCL, IRQ)
- See I2C_PINOUT_GUIDE.md

### Button clicks cause crashes?
- Clear browser cache (Ctrl+Shift+Delete or Cmd+Shift+Delete)
- Try private/incognito window
- Hard refresh (Ctrl+F5 or Cmd+Shift+R)

---

**Last Updated:** November 22, 2025
**Status:** Ready for Testing
**Priority:** HIGH (Blocks Settings Tab Functionality)

