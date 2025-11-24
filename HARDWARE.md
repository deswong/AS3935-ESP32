# Hardware Setup & Bill of Materials

## System Overview

The AS3935 Lightning Monitor uses an ESP32-C3 microcontroller to interface with an AS3935 lightning detector via I2C. The system includes Wi-Fi connectivity for MQTT publishing and a built-in web UI for configuration.

## Bill of Materials (BOM)

### Core Components

| Qty | Component | Part Number | Purpose | Notes |
|-----|-----------|-------------|---------|-------|
| 1 | ESP32-C3 Dev Board | ESP32-C3-DevKitM-1 | Main microcontroller | Any ESP32-C3 variant works |
| 1 | AS3935 Module | AS3935-DIY Module | Lightning detector | I2C interface required |
| 2 | Pull-up Resistor | 10kΩ 0.25W | I2C bus termination | Only if not on AS3935 module |
| 1 | USB Cable | USB 2.0 A→Micro-B | Programming & power | For initial flashing |

### Optional Components (for permanent installation)

| Qty | Component | Part Number | Purpose | Notes |
|-----|-----------|-------------|---------|-------|
| 1 | Power Supply | 5V/1A USB | Permanent power | Can use USB adapter |
| 1 | Enclosure | IP65 Plastic Box | Protection | Keep antenna exposed |
| 1 | Antenna | 5dBi 2.4GHz | Wi-Fi range | Optional, improves range |
| - | Jumper Wires | 22 AWG | Connections | Various lengths needed |

### Tools Required

| Tool | Purpose |
|------|---------|
| USB Serial Adapter | Alternative to USB cable (if not included) |
| Multimeter | Verify I2C pull-ups and power |
| Soldering Iron | Optional, for permanent connections |

## Wiring Schematic

### I2C Connection (Standard)

```
ESP32-C3                    AS3935 Module
───────────                 ──────────────
GPIO 21 (SDA) ────────┬──→  SDA
GPIO 22 (SCL) ────────┼──→  SCL
GPIO 23 (IRQ) ────────────→  IRQ
3.3V          ────────┼──→  VCC
GND           ────┬───┼──→  GND
              │   │   │
         [10kΩ] [10kΩ]  (Pull-ups, if needed)
              │   │
         3.3V ┴───┴
```

**Pull-up Resistors (if not on module):**
- SDA to 3.3V: 10kΩ resistor
- SCL to 3.3V: 10kΩ resistor

### Power Connection

```
USB 5V ──→  [USB→3.3V Regulator] ──→ 3.3V Pin
USB GND ──→  GND Pin
```

**Or direct 3.3V supply:**
```
3.3V Supply (1A capable) ──→ 3.3V Pin
GND ──→ GND Pin
```

## Hardware Assembly Instructions

### Step 1: Identify Components

- **ESP32-C3 Board**: Small board with GPIO labels on edges
- **AS3935 Module**: Sensor board with SDA, SCL, IRQ, VCC, GND labeled
- **Pull-up Resistors** (if purchased separately): 2× 10kΩ resistors

### Step 2: Check AS3935 Module

**Inspect the AS3935 module for:**
- ✓ Power pins labeled (VCC, GND)
- ✓ I2C pins labeled (SDA, SCL)
- ✓ IRQ pin labeled
- ✓ Optional on-board pull-ups (check datasheet; if not present, add external)

### Step 3: Connect I2C Bus

**Connect AS3935 to ESP32-C3:**

1. **SDA (Data)**: AS3935 SDA → ESP32-C3 GPIO 21
2. **SCL (Clock)**: AS3935 SCL → ESP32-C3 GPIO 22
3. **IRQ (Interrupt)**: AS3935 IRQ → ESP32-C3 GPIO 23
4. **VCC (Power)**: AS3935 VCC → ESP32-C3 3.3V Pin
5. **GND (Ground)**: AS3935 GND → ESP32-C3 GND Pin

**Use jumper wires or solder connections for durability.**

### Step 4: Add Pull-up Resistors (if needed)

If the AS3935 module doesn't include pull-ups, add external resistors:

1. **10kΩ resistor** between SDA and 3.3V
2. **10kΩ resistor** between SCL and 3.3V

Use resistor legs or solder to pin headers.

### Step 5: Verify Connections

**Check each connection with a multimeter:**

| Connection | Expected | Measurement |
|------------|----------|-------------|
| VCC to GND | 3.3V | 3.3V ±0.1V |
| SDA to GND (no load) | ~3.3V | 3.0-3.3V |
| SCL to GND (no load) | ~3.3V | 3.0-3.3V |
| IRQ to GND (idle) | ~3.3V | 3.0-3.3V |

### Step 6: Connect Power & USB

1. **USB Programming**: Connect ESP32-C3 to computer via USB for initial flashing
2. **Permanent Power** (optional): Connect 3.3V supply for standalone operation

## Antenna Placement

For optimal Wi-Fi signal:

- **Orientation**: Position AS3935 antenna vertically (pointing upward)
- **Height**: Mount at least 1 meter above ground
- **Clearance**: Keep away from metal objects and RF sources
- **Indoors**: Place near window or high location for best signal

## Optional Enclosure

If housing the device:

- **Material**: Plastic enclosure with IP65 rating (water/dust resistant)
- **Antenna**: Mount antenna externally or on enclosure lid
- **Ventilation**: Ensure adequate airflow; device draws minimal power
- **Mounting**: Use DIN rail clips or wall mounts

### Example Enclosure Setup

```
┌─────────────────────────┐
│   Plastic IP65 Box      │
│  ┌───────────────────┐  │
│  │  ESP32-C3 + AS3935│  │
│  │  [mounted inside] │  │
│  └───────────────────┘  │
│         ↑               │
│    [Antenna via hole]   │
│                         │
│  ○ USB port (side)      │
│  ○ Vent holes (bottom)  │
└─────────────────────────┘
```

## Power Requirements

| Component | Current | Voltage |
|-----------|---------|---------|
| ESP32-C3 | 80-200 mA | 3.3V |
| AS3935 | ~10 mA | 3.3V |
| Wi-Fi Active | +100-150 mA | 3.3V |
| **Total Peak** | **~350 mA** | **3.3V** |

**Power Supply Recommendation**: 5V/1A USB adapter with 3.3V regulator (most ESP32 boards include this).

## I2C Address Configuration

The AS3935 supports multiple I2C slave addresses via hardware pin strapping:

| Address | Configuration |
|---------|---------------|
| 0x03 | Default (most modules) |
| 0x02 | ADDR pin to GND |
| 0x01 | ADDR pin to VCC |

**To change address:** Solder ADDR pin connection on AS3935 module or configure via web UI at runtime (software address remapping).

## Troubleshooting Hardware

### Problem: No I2C Communication

**Causes & Solutions:**
1. **Check wiring**: Use multimeter to verify continuity on SDA/SCL
2. **Verify voltage**: Confirm 3.3V on VCC, 0V on GND
3. **Add pull-ups**: Missing pull-ups can prevent I2C communication
4. **Reduce wire length**: Keep I2C lines short (<10cm) to minimize noise
5. **Check address**: Verify correct I2C address (default 0x03)

### Problem: IRQ Not Triggering

**Causes & Solutions:**
1. **Check IRQ connection**: Verify GPIO 23 is connected to AS3935 IRQ pin
2. **Verify polarity**: IRQ is active-low (0V when triggered)
3. **Check sensor orientation**: Antenna must point upward for proper detection
4. **Test with API**: Use `/api/as3935/registers/all` to check sensor status

### Problem: Unstable Connection

**Causes & Solutions:**
1. **Add capacitors**: Place 0.1µF capacitors near VCC on both boards
2. **Use shorter wires**: Reduces noise and improves signal integrity
3. **Check power supply**: Ensure adequate current (1A recommended)
4. **Separate GND plane**: Minimize ground loops

## Testing Hardware

### Step 1: Verify Power

```bash
# Power on device
# Check serial monitor for messages
idf.py monitor
```

Expected: `I (xxx) as3935: Sensor initialized successfully`

### Step 2: Verify I2C

```bash
# Check I2C communication
curl http://192.168.1.42/api/as3935/status
```

Expected: `{"initialized":true,"i2c_addr":"0x03",...}`

### Step 3: Verify IRQ

Bring a magnet or RF source near the antenna (or use test signal if available).

Expected: Lightning events appear in logs or MQTT broker.

## Advanced Configuration

### Custom I2C Pins

To use different pins than defaults (21, 22, 23):

1. **Via Web UI**: AS3935 → Pins → Enter GPIO numbers
2. **Via REST API**:
   ```bash
   curl -X POST http://<ip>/api/as3935/pins/save \
     -H "Content-Type: application/json" \
     -d '{
       "i2c_port": 0,
       "sda_pin": 8,
       "scl_pin": 9,
       "irq_pin": 10
     }'
   ```

### Multiple I2C Buses

ESP32 supports multiple I2C buses (I2C0 and I2C1). For multiple AS3935 sensors:

1. Connect first sensor to I2C0 (GPIO 21/22, IRQ GPIO 23)
2. Connect second sensor to I2C1 (different GPIO pins)
3. Requires firmware modification to support multiple sensors

## Component Sourcing

### Recommended Vendors

| Component | Vendor | Notes |
|-----------|--------|-------|
| ESP32-C3 | AliExpress, SparkFun | ~$10 |
| AS3935 | AliExpress, LCSC | ~$15-25 |
| Resistors/Caps | AliExpress, SparkFun | ~$1 |

### Typical Total Cost: $30-50

## Warranty & Support

- **AS3935 Module**: Check manufacturer datasheet for operating specs
- **ESP32-C3**: See Espressif documentation
- **Firmware**: See [RELEASE.md](RELEASE.md) for support information

## References

- **AS3935 Datasheet**: Available from sensor module supplier
- **ESP32-C3 Datasheet**: https://www.espressif.com/
- **I2C Specification**: https://www.i2c-bus.org/
