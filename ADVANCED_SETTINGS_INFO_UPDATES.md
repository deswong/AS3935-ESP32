# Advanced Settings Information Updates

## Summary of Changes

This document outlines the improvements made to the Advanced Settings tab for the AS3935 Lightning Detector.

---

## 1. Enhanced Setting Descriptions ✅

All settings now have comprehensive, user-friendly information accessible via the **ℹ️ Info** buttons.

### **Environment Mode (AFE)**
- **Full explanation** of INDOOR vs OUTDOOR modes
- Sensitivity differences explained with practical guidance
- Tips for choosing the right mode based on location and electrical environment
- Values: INDOOR (18), OUTDOOR (14)

### **Noise Level Threshold**
- **Complete sensitivity scale** from 0-7 with actual voltage thresholds
- Explanation of what noise immunity means
- When to increase/decrease values
- Recommendations for different environments

### **Spike Rejection (0-15)**
- **Detailed filtering explanation** - what it actually filters
- Examples of electrical transients that get rejected
- Recommended values for different environments
- Clear indication of industrial vs clean areas

### **Minimum Lightning Strikes**
- **Practical explanation** of why requiring multiple strikes matters
- Clear value mapping (1, 5, 9, 16 strikes)
- When to use higher thresholds
- Tradeoffs between sensitivity and false positives

### **Watchdog Threshold (0-10)**
- **Timing scale explanation** with actual timing values
- What "settling" means for the sensor
- Recommended values for different use cases
- Relationship to event detection speed

### **Disturber Detection**
- **Algorithm explanation** - how the sensor distinguishes lightning from disturbances
- Clear enabled/disabled behavior
- When to use each mode
- Rise time and energy signature explanation

---

## 2. User-Customizable Environment Mode ✅

### **Previous Behavior:**
- Environment Mode (AFE) was automatically set to INDOOR on device initialization
- Users couldn't easily customize this without modifying code

### **New Behavior:**
- **Environment Mode now defaults to "Select..."** on initial load
- Users **must explicitly choose** INDOOR or OUTDOOR
- Select box remains blank until user makes a choice
- When users click "Reload Current", it shows the currently active mode but they can change it anytime

### **Implementation Details:**
- HTML select has three options:
  1. "Select..." (value: 0) - Initial/blank state
  2. "🏠 Indoor (more sensitive)" (value: 18)
  3. "🏞️ Outdoor (less sensitive)" (value: 14)
- JavaScript loading code no longer auto-selects the AFE value
- Sensor still initializes with a default internally, but UI doesn't force it on users

---

## 3. UI/UX Improvements

### Info Buttons
- ✅ All 6 settings have info buttons with emojis for quick identification
- ✅ Comprehensive descriptions with formatting (bullet points, headers)
- ✅ Practical recommendations and use cases
- ✅ Technical details for advanced users

### Settings Layout
- Grid layout (3 columns on desktop, responsive on mobile)
- Clear labels for each setting
- Input validation built-in (min/max values)
- Visual indicators (🏠, 🏞️, emojis for clarity)

---

## 4. Technical Changes

### Files Modified:

1. **components/main/web/index.html**
   - Enhanced `settingDescriptions` object with detailed information
   - Changed AFE loading logic to NOT auto-select values
   - All keys now match the onclick handlers exactly

2. **components/main/include/web_index.h**
   - Regenerated web header with all updates
   - Embeds the updated HTML with enhanced descriptions

### Configuration

- Sensor initialization still sets AFE=0 (INDOOR) as default in memory
- Users can override this through the web UI
- Settings persist when user clicks "Apply Settings"
- "Reload Current" button shows currently active settings

---

## 5. User Workflow

### First-Time Setup:
1. User opens Advanced Settings tab
2. Sees "Select..." for Environment Mode (not auto-selected)
3. Reads the ℹ️ Info description for guidance
4. Chooses INDOOR or OUTDOOR based on their location
5. Adjusts other settings as needed
6. Clicks "💾 Apply Settings"

### Later Use:
- Users can click "🔄 Reload Current" to see all current settings
- AFE will show the currently active mode
- Can be changed and "Apply Settings" to update

---

## 6. Emoji Reference in Advanced Settings

| Setting | Emoji | Values |
|---------|-------|--------|
| Environment Mode | 🌍 | 🏠 Indoor, 🏞️ Outdoor |
| Noise Level | 🔊 | 0-7 (levels with µV thresholds) |
| Spike Rejection | ⚡ | 0-15 (numeric) |
| Min Strikes | ⚡ | 1, 5, 9, 16 strikes |
| Watchdog | ⏱️ | 0-10 (timing levels) |
| Disturber | 🚫 | ✓ Enabled, ✗ Disabled |

---

## 7. Testing Checklist

Before deployment, verify:

- [ ] All info buttons display complete, properly formatted text
- [ ] AFE select shows "Select..." on initial page load
- [ ] AFE select correctly maps values (18=INDOOR, 14=OUTDOOR)
- [ ] "Reload Current" shows active settings
- [ ] "Apply Settings" saves and applies changes
- [ ] Mobile layout displays descriptions properly
- [ ] Modal closes when clicking outside or on close button

---

## 8. Future Enhancements

Potential improvements:
- [ ] Save user's Environment Mode preference in NVS
- [ ] Add "Restore Defaults" button
- [ ] Create preset configurations (e.g., "Weather Station", "Urban Area")
- [ ] Add graphical guides for threshold values
- [ ] Create a quick-start guide video for first-time users

---

**Last Updated:** November 22, 2025
**Version:** 1.0
