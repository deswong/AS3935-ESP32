# OpenHAB Storm Warning Integration Guide

This guide explains how to integrate the AS3935 lightning detector with OpenHAB using JavaScript rules to create intelligent storm warnings that track storm direction and proximity.

## Table of Contents

1. [Overview](#overview)
2. [MQTT Topics and Payloads](#mqtt-topics-and-payloads)
3. [OpenHAB Items Setup](#openhab-items-setup)
4. [JavaScript Rules for Storm Detection](#javascript-rules-for-storm-detection)
5. [Storm Direction Analysis](#storm-direction-analysis)
6. [Example Dashboard](#example-dashboard)

---

## Overview

The AS3935 device publishes lightning detection events to MQTT with the following information:
- **Lightning events**: Distance (1-40 km), energy levels
- **Availability status**: "online" or "offline" for device tracking
- **Event types**: Lightning, disturber, or noise detection

This integration uses OpenHAB JavaScript rules to:
- Track storm distance trends
- Determine if storm is approaching or receding
- Calculate estimated time to storm arrival
- Trigger alerts based on distance thresholds
- Provide storm direction estimation

---

## MQTT Topics and Payloads

### Lightning Detection Topic: `as3935/lightning`

**Payload Example:**
```json
{
  "event": "lightning",
  "description": "Lightning Strike Detected",
  "distance_km": 12,
  "distance_description": "Close (5-10km)",
  "energy": 450,
  "energy_description": "Moderate (200-500)",
  "r0": "0x05",
  "r1": "0x02",
  "r3": "0x44",
  "r8": "0x3f",
  "timestamp": 12345678
}
```

### Availability Topic: `as3935/availability`

**Possible Values:**
- `online` - Device is connected to MQTT broker
- `offline` - Device has disconnected (Last Will Testament)

### Disturber Detection Topic: `as3935/lightning`

**Payload Example:**
```json
{
  "event": "disturber",
  "description": "Disturber Detected (non-lightning noise)",
  "r0": "0x05",
  "r1": "0x02",
  "r3": "0x44",
  "r8": "0x3f",
  "timestamp": 12345678
}
```

---

## OpenHAB Items Setup

Create these items in your `items/lightning.items` file:

```
// MQTT Binding Items
String AS3935_LastEvent "Last Event [%s]" <lightning> { channel="mqtt:topic:mybroker:as3935_lightning:event" }
Number AS3935_Distance "Storm Distance [%.0f km]" <distance> { channel="mqtt:topic:mybroker:as3935_lightning:distance" }
Number AS3935_Energy "Energy Level [%.0f]" <energy> { channel="mqtt:topic:mybroker:as3935_lightning:energy" }
String AS3935_Status "Device Status [%s]" <status> { channel="mqtt:topic:mybroker:as3935_availability:state" }
DateTime AS3935_LastDetection "Last Detection [%1$tY-%1$tm-%1$td %1$tH:%1$tM:%1$tS]"

// Calculated Items
Number AS3935_DistanceTrend "Distance Trend [%d km/min]" <trend>
Number AS3935_EstimatedArrival "Time to Arrival [%d minutes]" <clock>
String AS3935_StormStatus "Storm Status [%s]" <alert>
String AS3935_StormAlert "⚠️ Storm Alert [%s]" <warning>
Number AS3935_WarningLevel "Warning Level [%d]" <attention>

// Counters
Number AS3935_StrikesLast5Min "Strikes (Last 5min) [%d]" <counter>
Number AS3935_StrikesLast15Min "Strikes (Last 15min) [%d]" <counter>

// Direction Estimation (if you have multiple detectors)
String AS3935_Direction "Estimated Direction [%s]" <wind>
Number AS3935_Bearing "Bearing [%.0f°]" <compass>
```

### MQTT Binding Configuration

Add to your `things/mqtt.things`:

```
Thing mqtt:topic:mybroker:as3935_lightning "AS3935 Lightning Detector" (mqtt:broker:mybroker) {
    Channels:
        Type string : event     [ stateTopic="as3935/lightning", transformationPattern="JSONPATH:$.event" ]
        Type number : distance  [ stateTopic="as3935/lightning", transformationPattern="JSONPATH:$.distance_km" ]
        Type number : energy    [ stateTopic="as3935/lightning", transformationPattern="JSONPATH:$.energy" ]
        Type string : description [ stateTopic="as3935/lightning", transformationPattern="JSONPATH:$.description" ]
}

Thing mqtt:topic:mybroker:as3935_availability "AS3935 Availability" (mqtt:broker:mybroker) {
    Channels:
        Type string : state [ stateTopic="as3935/availability", retained=true ]
}
```

---

## JavaScript Rules for Storm Detection

### Rule 1: Track Distance Trend and Alert on Lightning

Create a new rule file: `automation/lightning_detection.js`

```javascript
const logger = Java.type("org.slf4j.LoggerFactory").getLogger("lightning.detection");

// When a lightning strike is detected
rules.JSRule({
    name: "Lightning Strike Detected",
    description: "Process lightning detection and update storm status",
    triggers: [
        triggers.ItemStateChangedTrigger("AS3935_Distance")
    ],
    execute: function(module, input) {
        const currentDistance = parseFloat(items.getItem("AS3935_Distance").state);
        const previousDistance = parseFloat(items.getItem("AS3935_Distance").previousState);
        const now = new Date();

        logger.info("Lightning detected! Distance: {} km (Previous: {} km)", 
                    currentDistance, previousDistance);

        // Update last detection time
        events.postUpdate("AS3935_LastDetection", now.toISOString());

        // Calculate distance trend (km/minute)
        let distanceTrend = 0;
        if (previousDistance !== null && previousDistance !== NaN) {
            // Time between updates (assume ~30 seconds for close strikes, longer for distant)
            const timeDeltaMinutes = 0.5; // Adjust based on your strike frequency
            distanceTrend = (currentDistance - previousDistance) / timeDeltaMinutes;
        }

        events.postUpdate("AS3935_DistanceTrend", Math.round(distanceTrend));

        // Determine storm status
        let stormStatus = "Far";
        let warningLevel = 0;

        if (currentDistance <= 5) {
            stormStatus = "EXTREME - Very Close!";
            warningLevel = 5;
        } else if (currentDistance <= 10) {
            stormStatus = "SEVERE - Close";
            warningLevel = 4;
        } else if (currentDistance <= 20) {
            stormStatus = "WARNING - Approaching";
            warningLevel = 3;
        } else if (currentDistance <= 40) {
            stormStatus = "CAUTION - Distant";
            warningLevel = 2;
        } else {
            stormStatus = "Safe - Very Distant";
            warningLevel = 1;
        }

        // Check if storm is approaching
        let stormDirection = "Stationary";
        if (distanceTrend < -2) {
            stormDirection = "APPROACHING RAPIDLY";
            warningLevel += 2;
        } else if (distanceTrend < -0.5) {
            stormDirection = "Approaching";
            warningLevel += 1;
        } else if (distanceTrend > 2) {
            stormDirection = "Receding rapidly";
        } else if (distanceTrend > 0.5) {
            stormDirection = "Receding";
        }

        // Estimate arrival time
        let estimatedArrival = "N/A";
        if (distanceTrend < -0.1 && currentDistance > 0) {
            const minutesToArrival = Math.ceil(currentDistance / Math.abs(distanceTrend));
            if (minutesToArrival > 0 && minutesToArrival < 120) {
                estimatedArrival = minutesToArrival;
                events.postUpdate("AS3935_EstimatedArrival", minutesToArrival);
            }
        }

        // Create alert message
        let alertMessage = `${stormStatus} | Distance: ${currentDistance}km | Trend: ${stormDirection}`;
        if (estimatedArrival !== "N/A") {
            alertMessage += ` | ETA: ${estimatedArrival} min`;
        }

        events.postUpdate("AS3935_StormStatus", alertMessage);
        events.postUpdate("AS3935_StormAlert", alertMessage);
        events.postUpdate("AS3935_WarningLevel", warningLevel);

        logger.info("Storm Status Updated: {}", alertMessage);

        // Trigger notifications based on warning level
        if (warningLevel >= 4) {
            // SEVERE - Send immediate notification
            actions.NotificationAction.sendNotification("user@example.com",
                `🚨 SEVERE STORM ALERT: ${alertMessage}`);
            logger.warn("SEVERE STORM ALERT: {}", alertMessage);
        } else if (warningLevel >= 3) {
            // WARNING - Send notification
            actions.NotificationAction.sendNotification("user@example.com",
                `⚠️ STORM WARNING: ${alertMessage}`);
            logger.warn("STORM WARNING: {}", alertMessage);
        }
    }
});
```

### Rule 2: Track Strike Count and Rate

Create: `automation/strike_counter.js`

```javascript
const logger = Java.type("org.slf4j.LoggerFactory").getLogger("lightning.counter");

// Store strike history with timestamps
let strikeHistory = [];

rules.JSRule({
    name: "Lightning Strike Counter",
    description: "Count lightning strikes and calculate strike rate",
    triggers: [
        triggers.ItemStateChangedTrigger("AS3935_LastEvent", "lightning")
    ],
    execute: function(module, input) {
        const now = Date.now();
        
        // Add current strike to history
        strikeHistory.push(now);

        // Remove strikes older than 15 minutes
        strikeHistory = strikeHistory.filter(timestamp => (now - timestamp) < 15 * 60 * 1000);

        // Count strikes in different time windows
        const strikes5min = strikeHistory.filter(ts => (now - ts) < 5 * 60 * 1000).length;
        const strikes15min = strikeHistory.length;

        // Update items
        events.postUpdate("AS3935_StrikesLast5Min", strikes5min);
        events.postUpdate("AS3935_StrikesLast15Min", strikes15min);

        logger.info("Strike count - Last 5min: {}, Last 15min: {}", strikes5min, strikes15min);

        // Severe activity alert (multiple strikes in short period)
        if (strikes5min >= 10) {
            logger.warn("SEVERE ACTIVITY: {} strikes in last 5 minutes!", strikes5min);
        } else if (strikes5min >= 5) {
            logger.info("Active storm: {} strikes in last 5 minutes", strikes5min);
        }
    }
});
```

### Rule 3: Track Device Availability

Create: `automation/device_availability.js`

```javascript
const logger = Java.type("org.slf4j.LoggerFactory").getLogger("lightning.availability");

rules.JSRule({
    name: "AS3935 Availability Monitor",
    description: "Monitor AS3935 device connection status",
    triggers: [
        triggers.ItemStateChangedTrigger("AS3935_Status")
    ],
    execute: function(module, input) {
        const status = items.getItem("AS3935_Status").state;
        
        if (status === "online") {
            logger.info("✓ AS3935 Device is ONLINE");
            // Clear any offline alerts
        } else if (status === "offline") {
            logger.warn("✗ AS3935 Device is OFFLINE");
            actions.NotificationAction.sendNotification("user@example.com",
                "⚠️ Lightning detector device is offline!");
        }
    }
});
```

### Rule 4: Reset Storm Tracking

Create: `automation/storm_reset.js`

```javascript
const logger = Java.type("org.slf4j.LoggerFactory").getLogger("lightning.reset");

// Reset storm tracking every 30 minutes of no activity
rules.JSRule({
    name: "Storm Tracking Reset",
    description: "Reset storm alerts if no activity for extended period",
    triggers: [
        triggers.GenericCronTrigger("0 */30 * * * ?")  // Every 30 minutes
    ],
    execute: function(module, input) {
        const lastDetection = items.getItem("AS3935_LastDetection").state;
        const now = new Date();
        
        // Check if last detection was more than 30 minutes ago
        if (lastDetection) {
            const lastTime = new Date(lastDetection);
            const minutesSinceLastStrike = (now - lastTime) / (1000 * 60);
            
            if (minutesSinceLastStrike > 30) {
                logger.info("No storm activity for {} minutes. Resetting alerts.", 
                           Math.round(minutesSinceLastStrike));
                
                events.postUpdate("AS3935_StormStatus", "No recent activity");
                events.postUpdate("AS3935_WarningLevel", 0);
                events.postUpdate("AS3935_DistanceTrend", 0);
                events.postUpdate("AS3935_EstimatedArrival", "N/A");
            }
        }
    }
});
```

---

## Storm Direction Analysis

If you have multiple lightning detectors at different locations, you can estimate the direction of the storm. Here's an advanced example:

### Rule 5: Multi-Device Direction Estimation

Create: `automation/storm_direction.js`

```javascript
const logger = Java.type("org.slf4j.LoggerFactory").getLogger("lightning.direction");

rules.JSRule({
    name: "Storm Direction Estimation",
    description: "Estimate storm direction based on distance trends",
    triggers: [
        triggers.ItemStateChangedTrigger("AS3935_Distance")
    ],
    execute: function(module, input) {
        const currentDistance = parseFloat(items.getItem("AS3935_Distance").state);
        const trend = parseFloat(items.getItem("AS3935_DistanceTrend").state);

        // Simple direction estimation based on trend
        // In reality, you'd want triangulation if you have multiple detectors
        let direction = "N/A";
        let bearing = 0;

        // If you have your detector's location and need triangulation:
        // You would need GPS coordinates and time-synchronized readings
        // For now, use a simple heuristic based on strike history patterns

        // This is a placeholder - implement based on your setup
        // Options:
        // 1. If you have 2+ detectors: use triangulation
        // 2. If you have 1 detector: use historical pattern matching
        // 3. Use weather data correlation

        // Example: Simple assumption-based direction
        if (currentDistance < 10 && trend < 0) {
            // Close and approaching - likely heading toward detector
            direction = "Toward";
            bearing = 0; // Would be calculated from detector orientation
        } else if (trend > 0) {
            direction = "Away";
            bearing = 180;
        }

        events.postUpdate("AS3935_Direction", direction);
        if (bearing !== null) {
            events.postUpdate("AS3935_Bearing", bearing);
        }
    }
});
```

---

## Example Dashboard

### Basic Sitemap (HTML)

```
sitemap lightning label="⚡ Storm Monitor" {
    Frame label="Storm Status" {
        Text item=AS3935_StormAlert
        Number item=AS3935_WarningLevel
        Text item=AS3935_Distance
        Number item=AS3935_DistanceTrend
        Number item=AS3935_EstimatedArrival
    }
    
    Frame label="Activity" {
        Number item=AS3935_StrikesLast5Min
        Number item=AS3935_StrikesLast15Min
        Text item=AS3935_LastDetection
    }
    
    Frame label="Device Status" {
        Text item=AS3935_Status
        Text item=AS3935_LastEvent
    }
}
```

### Advanced Dashboard Widget (HABPanel/OH3+)

```html
<!-- Add to your dashboard -->
<div class="widget" ng-controller="ItemController">
    <div class="storm-alert" ng-class="{'warning-level-' + itemValue('AS3935_WarningLevel')}">
        <h1>{{ itemValue('AS3935_StormAlert') }}</h1>
        
        <div class="storm-metrics">
            <div class="metric">
                <span class="label">Distance:</span>
                <span class="value">{{ itemValue('AS3935_Distance') }} km</span>
            </div>
            
            <div class="metric">
                <span class="label">Trend:</span>
                <span class="value">{{ itemValue('AS3935_DistanceTrend') }} km/min</span>
            </div>
            
            <div class="metric">
                <span class="label">ETA:</span>
                <span class="value">{{ itemValue('AS3935_EstimatedArrival') }} min</span>
            </div>
            
            <div class="metric">
                <span class="label">Strikes (5min):</span>
                <span class="value">{{ itemValue('AS3935_StrikesLast5Min') }}</span>
            </div>
        </div>
        
        <div class="device-status">
            Device: <span class="status" ng-class="{'online': itemValue('AS3935_Status') === 'online', 'offline': itemValue('AS3935_Status') !== 'online'}">
                {{ itemValue('AS3935_Status') }}
            </span>
        </div>
    </div>
</div>

<style>
.storm-alert {
    padding: 20px;
    border-radius: 8px;
    border: 3px solid #ccc;
}

.storm-alert.warning-level-1 { background: #e8f5e9; border-color: #4caf50; }
.storm-alert.warning-level-2 { background: #fff3e0; border-color: #ff9800; }
.storm-alert.warning-level-3 { background: #ffe0b2; border-color: #ff6f00; }
.storm-alert.warning-level-4 { background: #ffebee; border-color: #f44336; }
.storm-alert.warning-level-5 { background: #f3e5f5; border-color: #9c27b0; animation: pulse 1s infinite; }

.storm-metrics {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 15px;
    margin: 15px 0;
}

.metric {
    background: rgba(255,255,255,0.7);
    padding: 10px;
    border-radius: 4px;
}

.metric .label { font-weight: bold; }
.metric .value { margin-left: 10px; }

.device-status {
    margin-top: 15px;
    text-align: center;
}

.device-status .status.online { color: #4caf50; }
.device-status .status.offline { color: #f44336; }

@keyframes pulse {
    0%, 100% { opacity: 1; }
    50% { opacity: 0.7; }
}
</style>
```

---

## Advanced Features

### 1. Machine Learning Strike Prediction

Store historical data and identify patterns:
- Time of day storms occur
- Seasonal variations
- Leading indicators (increase in disturber/noise events)

### 2. Multi-Detector Triangulation

If you have 2-3 AS3935 detectors:
- Calculate bearing from time differences between detections
- Triangulate storm center
- Better direction and speed estimation

### 3. Weather API Integration

Combine with weather services:
- Correlate with radar data
- Cross-verify with severe weather warnings
- Improve trend prediction

### 4. Custom Notifications

```javascript
// Send to different channels based on severity
if (warningLevel >= 4) {
    // SMS for severe
    actions.Telegram.sendTelegram("🚨 SEVERE STORM ALERT!");
} else if (warningLevel >= 3) {
    // Push notification
    actions.PushNotification.sendNotification("Storm Warning", alertMessage);
}
```

---

## Troubleshooting

### No strikes detected
- Check MQTT broker connection: Verify `AS3935_Status` is "online"
- Verify MQTT topic: Should be `as3935/lightning`
- Check device settings in web UI (AFE, noise level)

### Distance not updating
- Ensure MQTT binding is correctly parsing JSON payload
- Verify JSON path transformation: `$.distance_km`
- Check broker logs for incoming messages

### Rules not triggering
- Enable debug logging: Set logger level to DEBUG
- Verify trigger item names match exactly
- Check rule syntax in OpenHAB logs

### Missed direction information
- Direction estimation requires pattern analysis or multiple detectors
- Consider adding compass/GPS data if available
- Use weather radar correlation for better results

---

## References

- [AS3935 Library Documentation](../AS3935_REGISTER_API.md)
- [MQTT Integration Guide](../AS3935_ADVANCED_SETTINGS_QUICK.md)
- [OpenHAB JavaScript Rules](https://www.openhab.org/docs/configuration/rules-dsl.html)
- [MQTT Binding](https://www.openhab.org/addons/bindings/mqtt/)

---

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Enable DEBUG logging on the AS3935 device
3. Verify MQTT messages are being published with `mqtt_sub` tool
4. Check OpenHAB rule execution logs

