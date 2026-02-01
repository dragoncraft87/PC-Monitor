# Hardware Assembly Guide

Complete build guide for the Scarab Monitor 4-display system.

---

## Bill of Materials

### Required Components

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32-S3-DevKitC N16R8 | 1 | Must have 8MB PSRAM |
| GC9A01 Round Display (240x240) | 4 | 1.28" diameter, SPI interface |
| USB-C Cable | 1 | ESP32 to PC connection |
| Jumper Wires (M-M) | ~30 | Various colors recommended |
| Jumper Wires (M-F) | ~10 | If displays have pin headers |

### Optional Components

| Component | Purpose |
|-----------|---------|
| Breadboard | Prototyping before permanent install |
| Custom PCB | Clean permanent installation |
| 3D Printed Case | Professional display mounting |
| Powered USB Hub | If displays flicker (more current) |

---

## Power Architecture

### Voltage Levels

```
USB 5V ──> ESP32-S3 Onboard Regulator ──> 3.3V Logic
```

| Rail | Voltage | Source |
|------|---------|--------|
| USB Input | 5V | PC USB port or powered hub |
| ESP32 Logic | 3.3V | Onboard LDO regulator |
| Display VCC | 3.3V | From ESP32 3.3V pin |
| Display I/O | 3.3V | Native 3.3V logic levels |

**Important:** The GC9A01 displays are 3.3V devices. Connect VCC to the ESP32's 3.3V output, NOT to 5V. The 5V USB power is only for the ESP32's internal regulator.

### Current Requirements

| Component | Current Draw |
|-----------|--------------|
| ESP32-S3 | 100-200 mA |
| GC9A01 Display (each) | 80-120 mA |
| **Total System** | **~600-700 mA** |

### Recommended Power Sources

| Source | Rating | Status |
|--------|--------|--------|
| USB 2.0 Port | 500 mA | Borderline (may flicker) |
| USB 3.0 Port | 900 mA | Recommended |
| Powered USB Hub | 1-2 A | Best for reliability |
| External 5V PSU | 2 A+ | Permanent installations |

---

## Wiring

### Complete Pin Reference

All 4 displays share the SPI bus (SCK + MOSI) but require individual control pins.

#### Shared SPI Bus

| Signal | ESP32 GPIO | Connects To |
|--------|------------|-------------|
| SCK (Clock) | GPIO 4 | All display SCL/SCK pins |
| MOSI (Data) | GPIO 5 | All display SDA/MOSI pins |

#### Power Rails

| Signal | ESP32 Pin | Connects To |
|--------|-----------|-------------|
| 3.3V | 3V3 | All display VCC pins |
| GND | GND | All display GND pins |

#### Individual Control Pins

| Display | CS (Chip Select) | DC (Data/Command) | RST (Reset) |
|---------|------------------|-------------------|-------------|
| **CPU** | GPIO 12 | GPIO 11 | GPIO 13 |
| **GPU** | GPIO 9 | GPIO 46 | GPIO 10 |
| **RAM** | GPIO 8 | GPIO 18 | GPIO 3 |
| **Network** | GPIO 16 | GPIO 15 | GPIO 17 |

### Wiring Diagram

```
                         ESP32-S3-DevKitC
                    ┌────────────────────────┐
                    │                        │
     SPI Bus ──────>│ GPIO 4  (SCK)  ────────┼──┬── SCK  (all displays)
                    │ GPIO 5  (MOSI) ────────┼──┴── MOSI (all displays)
                    │                        │
     CPU Display ──>│ GPIO 12 (CS)   ────────┼───── CS   (CPU)
                    │ GPIO 11 (DC)   ────────┼───── DC   (CPU)
                    │ GPIO 13 (RST)  ────────┼───── RST  (CPU)
                    │                        │
     GPU Display ──>│ GPIO 9  (CS)   ────────┼───── CS   (GPU)
                    │ GPIO 46 (DC)   ────────┼───── DC   (GPU)
                    │ GPIO 10 (RST)  ────────┼───── RST  (GPU)
                    │                        │
     RAM Display ──>│ GPIO 8  (CS)   ────────┼───── CS   (RAM)
                    │ GPIO 18 (DC)   ────────┼───── DC   (RAM)
                    │ GPIO 3  (RST)  ────────┼───── RST  (RAM)
                    │                        │
     NET Display ──>│ GPIO 16 (CS)   ────────┼───── CS   (Network)
                    │ GPIO 15 (DC)   ────────┼───── DC   (Network)
                    │ GPIO 17 (RST)  ────────┼───── RST  (Network)
                    │                        │
     Power ────────>│ 3V3 ───────────────────┼──┬── VCC  (all displays)
                    │ GND ───────────────────┼──┴── GND  (all displays)
                    │                        │
                    └────────────────────────┘
```

---

## Assembly Steps

### Step 1: Prepare Workspace

1. Clear workspace, have multimeter ready
2. Label displays with tape: CPU, GPU, RAM, Network
3. Organize wires by color

**Recommended Wire Colors:**

| Color | Signal |
|-------|--------|
| Red | VCC (3.3V) |
| Black | GND |
| Yellow | SCK |
| Green | MOSI |
| Blue | CS pins |
| Orange | DC pins |
| Purple | RST pins |

### Step 2: Connect Shared Bus

Connect from ESP32 to ALL 4 displays:

```
ESP32 GPIO 4  ──┬── Display 1 SCK
                ├── Display 2 SCK
                ├── Display 3 SCK
                └── Display 4 SCK

ESP32 GPIO 5  ──┬── Display 1 MOSI
                ├── Display 2 MOSI
                ├── Display 3 MOSI
                └── Display 4 MOSI
```

### Step 3: Connect Power

```
ESP32 3V3  ──┬── Display 1 VCC
             ├── Display 2 VCC
             ├── Display 3 VCC
             └── Display 4 VCC

ESP32 GND  ──┬── Display 1 GND
             ├── Display 2 GND
             ├── Display 3 GND
             └── Display 4 GND
```

**Important:** Ensure solid GND connections. Poor ground = flickering displays.

### Step 4: Connect Control Pins

#### CPU Display
```
GPIO 12 ──> CS
GPIO 11 ──> DC
GPIO 13 ──> RST
```

#### GPU Display
```
GPIO 9  ──> CS
GPIO 46 ──> DC
GPIO 10 ──> RST
```

#### RAM Display
```
GPIO 8  ──> CS
GPIO 18 ──> DC
GPIO 3  ──> RST
```

#### Network Display
```
GPIO 16 ──> CS
GPIO 15 ──> DC
GPIO 17 ──> RST
```

### Step 5: Pre-Power Checklist

Before connecting USB:

- [ ] All SCK wires connected to GPIO 4
- [ ] All MOSI wires connected to GPIO 5
- [ ] All VCC wires connected to 3V3 (NOT 5V)
- [ ] All GND wires connected
- [ ] Each display has unique CS, DC, RST pins
- [ ] No shorts between VCC and GND

---

## Testing Procedure

Test incrementally to isolate problems:

### Test 1: ESP32 Only

1. Connect ESP32 via USB (no displays)
2. Run: `idf.py monitor`
3. Verify boot messages appear
4. Success: ESP32 is functional

### Test 2: Add One Display

1. Wire CPU display only
2. Flash: `idf.py flash monitor`
3. Verify: Display lights up, shows CPU gauge
4. Success: SPI bus works

### Test 3: Add Remaining Displays

1. Add GPU, RAM, Network displays one at a time
2. Reset ESP32 after each addition
3. Verify each display shows correct data
4. Success: Complete system operational

---

## Troubleshooting

### Display Stays White/Blank

| Cause | Solution |
|-------|----------|
| SCK/MOSI disconnected | Check continuity with multimeter |
| Wrong CS pin | Verify pin matches firmware |
| Defective display | Try different display |

### Display Stays Black

| Cause | Solution |
|-------|----------|
| No power | Measure VCC (should be 3.3V) |
| Wrong voltage | Ensure VCC is on 3V3, not 5V |
| Bad GND | Check ground connections |

### Displays Flicker

| Cause | Solution |
|-------|----------|
| Insufficient power | Use USB 3.0 or powered hub |
| Loose GND | Strengthen ground connections |
| Long wires | Shorten to < 15cm |
| No decoupling | Add 100nF caps near displays |

### Wrong Display Shows Wrong Data

| Cause | Solution |
|-------|----------|
| CS pins swapped | Swap physical wire connections |

---

## Recommendations

### Wire Length

| Length | Quality |
|--------|---------|
| < 10cm | Optimal |
| 10-20cm | Good |
| 20-30cm | Acceptable |
| > 30cm | May cause issues |

### Decoupling Capacitors

For stable operation, add 100nF ceramic capacitors:
- One across VCC-GND near ESP32
- One across VCC-GND near each display

### Display Layout Options

**2x2 Grid:**
```
┌─────┐  ┌─────┐
│ CPU │  │ GPU │
└─────┘  └─────┘
┌─────┐  ┌─────┐
│ RAM │  │ NET │
└─────┘  └─────┘
```

**Horizontal Row:**
```
┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐
│ CPU │  │ GPU │  │ RAM │  │ NET │
└─────┘  └─────┘  └─────┘  └─────┘
```

---

## Final Verification

Before declaring success:

- [ ] All 4 displays light up
- [ ] Each display shows correct metric
- [ ] No flickering under load
- [ ] Serial monitor shows no errors
- [ ] Windows client connects automatically
- [ ] System runs stable for 10+ minutes

---

Next: Install the Windows client from the [main README](../README.md).
