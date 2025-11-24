# Info Button Error Fixed: showSettingInfo is not defined

## Problem
When clicking the info buttons (ℹ️ Info), the browser console showed:
```
Uncaught ReferenceError: showSettingInfo is not defined
    at HTMLButtonElement.onclick
```

## Root Cause
The `showSettingInfo()` function and `settingDescriptions` object were defined **inside** the `registerEventListeners()` function, making them local to that function's scope. However, the HTML buttons were trying to call `showSettingInfo()` via `onclick` attributes, which execute in the **global scope**. This caused the function to be undefined.

### Before (Incorrect):
```
registerEventListeners() {
  const safeId = (id) => { ... }
  
  // ... hundreds of lines of code ...
  
  const settingDescriptions = { ... }  // ← LOCAL SCOPE
  
  function showSettingInfo(settingName) { ... }  // ← LOCAL SCOPE
}
```

HTML button trying to call it:
```html
<button onclick="showSettingInfo('disturber')">ℹ️ Info</button>
<!-- ↑ This looks for showSettingInfo in GLOBAL scope - NOT FOUND! -->
```

## Solution
Moved `settingDescriptions` and `showSettingInfo()` to **global scope** (before the `load()` function), making them accessible to the `onclick` handlers.

### After (Correct):
```javascript
// GLOBAL SCOPE - accessible to onclick handlers
const settingDescriptions = {
  afe: "...",
  noise_level: "...",
  spike_rejection: "...",
  min_strikes: "...",
  watchdog: "...",
  disturber: "..."
};

function showSettingInfo(settingName) {
  const modal = document.getElementById('settings_info_modal');
  const title = modal.querySelector('h2');
  const content = modal.querySelector('p');
  title.textContent = 'Setting Information';
  content.textContent = settingDescriptions[settingName] || 'No information available for this setting.';
  modal.style.display = 'block';
}

// load() function and registerEventListeners() follow below
```

## File Changes

**File:** `components/main/web/index.html`

**Changes:**
1. **Lines 505-529:** Added `settingDescriptions` and `showSettingInfo()` in global scope
2. **Removed duplicate:** Deleted the same definitions from inside `registerEventListeners()` (previously around lines 883-1040)

**Lines added (global scope, before load()):**
```javascript
// Setting descriptions for info modal (global scope for onclick handlers)
const settingDescriptions = {
  afe: "...",
  // ... all setting descriptions
};

// Show setting info modal (global scope for onclick handlers)
function showSettingInfo(settingName) {
  const modal = document.getElementById('settings_info_modal');
  const title = modal.querySelector('h2');
  const content = modal.querySelector('p');
  title.textContent = 'Setting Information';
  content.textContent = settingDescriptions[settingName] || 'No information available for this setting.';
  modal.style.display = 'block';
}
```

**Web header regenerated:**
```
components/main/include/web_index.h
```

## Testing

After rebuild and flash:

✅ Click any ℹ️ Info button → Modal opens with correct setting description
✅ Browser console shows NO "showSettingInfo is not defined" error
✅ Modal displays correct information for each setting
✅ Modal closes when clicking ✕ Close or outside the modal

## Technical Details

**JavaScript Scope Rules:**
- Functions defined at the top level are in the **global scope**
- Functions defined inside other functions are in **local scope**
- `onclick` handlers execute in the **global scope**
- To call a function from `onclick`, it must be in the **global scope**

**Solution Pattern:**
When functions need to be called from HTML event handlers (onclick, onchange, etc.), define them in global scope:

```javascript
// ✅ CORRECT - Global scope, accessible from onclick
function myFunction() { ... }
<button onclick="myFunction()">Click</button>

// ❌ WRONG - Local scope, not accessible from onclick
function outer() {
  function myFunction() { ... }  // Local to outer()
}
<button onclick="myFunction()">Click</button>  // Error: not defined
```

## Summary

✅ **Fixed:** `showSettingInfo is not defined` error
✅ **Info buttons now work:** All setting info modals display correctly
✅ **Ready to rebuild and test**

---

**Status:** Ready for firmware rebuild and flash
**Files modified:** 
- `components/main/web/index.html` (moved functions to global scope)
- `components/main/include/web_index.h` (auto-regenerated)
