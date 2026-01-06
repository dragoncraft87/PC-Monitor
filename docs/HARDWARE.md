# 🔧 Hardware Assembly Guide

Complete guide for building the PC Monitor 4x Display system.

## 📋 Bill of Materials (BOM)

### Required Components

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32-S3-DevKitC N16R8 | 1 | Must have 8MB PSRAM! |
| GC9A01 Round Display (240x240) | 4 | 1.28" diameter |
| USB-C Cable | 1 | For ESP32 → PC connection |
| Breadboard | 1 | Optional, for prototyping |
| Jumper Wires (M-M) | ~30 | Various colors recommended |
| Jumper Wires (M-F) | ~10 | If displays have headers |

### Optional Components

| Component | Purpose |
|-----------|---------|
| Custom PCB | Clean permanent installation |
| 3D Printed Case | Mount displays professionally |
| Powered USB Hub | If displays flicker (more power) |
| External 5V PSU | Alternative to USB power |

---

## 🔌 Wiring Diagram

### SPI Bus Overview

All 4 displays share the SPI bus (SCK + MOSI) but have individual control pins (CS, DC, RST).

```
                    ESP32-S3
             ┌──────────────────────┐
             │                      │
   SCK  ────┤ GPIO 4               │
   MOSI ────┤ GPIO 5               │
             │                      │
   CS1  ────┤ GPIO 11    (CPU)     │
   DC1  ────┤ GPIO 12              │
   RST1 ────┤ GPIO 13              │
             │                      │
   CS2  ────┤ GPIO 10    (GPU)     │
   DC2  ────┤ GPIO 9               │
   RST2 ────┤ GPIO 46              │
             │                      │
   CS3  ────┤ GPIO 3     (RAM)     │
   DC3  ────┤ GPIO 8               │
   RST3 ────┤ GPIO 18              │
             │                      │
   CS4  ────┤ GPIO 15 (Network)    │
   DC4  ────┤ GPIO 16              │
   RST4 ────┤ GPIO 17              │
             │                      │
   3.3V ────┤ 3.3V (to all)        │
   GND  ────┤ GND (to all)         │
             └──────────────────────┘
```

### Pin Assignment Table

| Display | Function | ESP32 GPIO | Display Pin |
|---------|----------|-----------|-------------|
| **ALL** | SCK (Clock) | GPIO 4 | SCL/SCK |
| **ALL** | MOSI (Data) | GPIO 5 | SDA/MOSI |
| **ALL** | Power | 3.3V | VCC |
| **ALL** | Ground | GND | GND |
|  |  |  |  |
| **CPU** | Chip Select | GPIO 11 | CS |
| **CPU** | Data/Command | GPIO 12 | DC |
| **CPU** | Reset | GPIO 13 | RST |
|  |  |  |  |
| **GPU** | Chip Select | GPIO 10 | CS |
| **GPU** | Data/Command | GPIO 9 | DC |
| **GPU** | Reset | GPIO 46 | RST |
|  |  |  |  |
| **RAM** | Chip Select | GPIO 3 | CS |
| **RAM** | Data/Command | GPIO 8 | DC |
| **RAM** | Reset | GPIO 18 | RST |
|  |  |  |  |
| **Network** | Chip Select | GPIO 15 | CS |
| **Network** | Data/Command | GPIO 16 | DC |
| **Network** | Reset | GPIO 17 | RST |

---

## 🛠️ Step-by-Step Assembly

### Step 1: Prepare Your Workspace

1. **Clear workspace** - Remove clutter
2. **Gather tools** - Wire strippers, multimeter (optional)
3. **Label displays** - Use tape to mark: CPU, GPU, RAM, Network
4. **Organize wires** - Use different colors for different signals

**Recommended Wire Colors:**
- **Red** → VCC (3.3V)
- **Black** → GND
- **Yellow** → SCK
- **Green** → MOSI
- **Blue** → CS pins
- **Orange** → DC pins
- **Purple** → RST pins

### Step 2: Connect Shared SPI Bus

Connect these wires from ESP32 to ALL 4 displays:

```
ESP32 GPIO 4  ──┬─→ Display 1 SCK
                ├─→ Display 2 SCK
                ├─→ Display 3 SCK
                └─→ Display 4 SCK

ESP32 GPIO 5  ──┬─→ Display 1 MOSI
                ├─→ Display 2 MOSI
                ├─→ Display 3 MOSI
                └─→ Display 4 MOSI
```

**Tip:** Use a breadboard power rail to distribute SCK and MOSI easily!

### Step 3: Connect Power Rails

```
ESP32 3.3V  ──┬─→ Display 1 VCC
              ├─→ Display 2 VCC
              ├─→ Display 3 VCC
              └─→ Display 4 VCC

ESP32 GND   ──┬─→ Display 1 GND
              ├─→ Display 2 GND
              ├─→ Display 3 GND
              └─→ Display 4 GND
```

**Important:** Ensure solid GND connections. Bad GND = flickering displays!

### Step 4: Connect Individual Control Pins

#### Display 1 (CPU)
```
ESP32 GPIO 11 → Display 1 CS
ESP32 GPIO 12 → Display 1 DC
ESP32 GPIO 13 → Display 1 RST
```

#### Display 2 (GPU)
```
ESP32 GPIO 10 → Display 2 CS
ESP32 GPIO 9  → Display 2 DC
ESP32 GPIO 46 → Display 2 RST
```

#### Display 3 (RAM)
```
ESP32 GPIO 3  → Display 3 CS
ESP32 GPIO 8  → Display 3 DC
ESP32 GPIO 18 → Display 3 RST
```

#### Display 4 (Network)
```
ESP32 GPIO 15 → Display 4 CS
ESP32 GPIO 16 → Display 4 DC
ESP32 GPIO 17 → Display 4 RST
```

### Step 5: Double-Check Connections

Use this checklist before powering on:

- [ ] All SCK connections verified
- [ ] All MOSI connections verified
- [ ] All VCC (3.3V) connections verified
- [ ] All GND connections verified
- [ ] Each display has unique CS pin
- [ ] Each display has unique DC pin
- [ ] Each display has unique RST pin
- [ ] No shorts between power rails
- [ ] USB cable ready

---

## ⚡ Power Considerations

### Power Requirements

- **Each Display:** ~80-120 mA (at full brightness)
- **4 Displays Total:** ~400-500 mA
- **ESP32-S3:** ~100-200 mA
- **Total System:** ~600-700 mA

### Power Sources

| Source | Current | Suitable? |
|--------|---------|-----------|
| USB 2.0 Port | 500 mA | ⚠️ Borderline (may flicker) |
| USB 3.0 Port | 900 mA | ✅ Recommended |
| Powered USB Hub | 1-2 A | ✅ Best option |
| External 5V PSU | 2 A+ | ✅ Permanent installation |

**Symptoms of insufficient power:**
- Displays flicker
- Displays randomly turn off
- ESP32 reboots
- Brownouts in serial monitor

**Solutions:**
1. Use USB 3.0 port (blue connector)
2. Use powered USB hub
3. Use external 5V 2A power supply

---

## 🧪 Testing Procedure

Test incrementally to catch issues early!

### Test 1: ESP32 Only

1. Connect ESP32 to PC via USB
2. Open serial monitor: `idf.py monitor`
3. Should see boot messages
4. ✅ ESP32 works

### Test 2: Add Display 1

1. Wire Display 1 (CPU) only
2. Flash firmware: `idf.py flash monitor`
3. Should see:
   - Display 1 lights up
   - Shows CPU gauge
4. ✅ Display 1 works

### Test 3: Add Display 2

1. Wire Display 2 (GPU)
2. Reset ESP32 (or reflash)
3. Should see:
   - Display 1 shows CPU
   - Display 2 shows GPU
4. ✅ Display 2 works

### Test 4: Add Display 3

1. Wire Display 3 (RAM)
2. Reset ESP32
3. All 3 displays should work
4. ✅ Display 3 works

### Test 5: Add Display 4

1. Wire Display 4 (Network)
2. Reset ESP32
3. All 4 displays should work
4. ✅ Complete system works!

---

## 🔍 Troubleshooting

### Problem: Display stays white/blank

**Possible Causes:**
- SCK or MOSI not connected
- Wrong CS pin
- Display defective

**Solutions:**
1. Check SCK connection with multimeter (continuity)
2. Check MOSI connection
3. Verify CS pin in code matches wiring
4. Try different display

### Problem: Display stays black

**Possible Causes:**
- No power (VCC or GND)
- Wrong voltage (must be 3.3V NOT 5V!)

**Solutions:**
1. Measure voltage at display VCC pin (should be 3.3V)
2. Check GND connection
3. Verify power supply can deliver enough current

### Problem: Only one display works

**Possible Causes:**
- CS pins swapped or duplicated
- SCK/MOSI not reaching all displays

**Solutions:**
1. Verify each display has unique CS pin
2. Check breadboard connections (sometimes loose)
3. Measure SCK/MOSI at each display

### Problem: Displays flicker

**Possible Causes:**
- Insufficient power
- Bad GND connections
- Wires too long

**Solutions:**
1. Use USB 3.0 port or powered hub
2. Strengthen GND connections
3. Shorten wires (ideally < 15cm)
4. Add 100uF capacitor near ESP32 VCC

### Problem: Wrong display shows wrong data

**Possible Causes:**
- CS pins in wrong order

**Solution:**
- Swap CS pin connections to match firmware order

---

## 💡 Pro Tips

### Tip 1: Use a Breadboard

Start with a breadboard for prototyping. Once working, move to permanent solution (PCB or soldered connections).

### Tip 2: Cable Management

Use different colored wires:
- Makes debugging easier
- Looks more professional
- Reduces mistakes

### Tip 3: Measure Twice, Connect Once

Before connecting power:
- Use multimeter continuity mode
- Verify no shorts between VCC and GND
- Double-check all pin assignments

### Tip 4: Add Decoupling Capacitors

For stable operation, add 100nF ceramic capacitor across VCC-GND:
- One near ESP32
- One near each display

### Tip 5: Short Wires = Better Signal

Signal quality degrades with wire length:
- **< 10cm**: Perfect
- **10-20cm**: Good
- **20-30cm**: Okay
- **> 30cm**: May have issues

---

## 📐 3D Printing (Optional)

### Case Design Considerations

If designing a 3D printed case:

1. **Display Spacing:** 50-60mm between centers
2. **Viewing Angle:** Tilt displays 15-20° toward user
3. **Ventilation:** ESP32 can get warm
4. **Cable Management:** Route wires behind displays
5. **Mounting:** M3 screws work well for GC9A01

### Recommended Layout

```
┌─────┐  ┌─────┐
│ CPU │  │ GPU │
└─────┘  └─────┘

┌─────┐  ┌─────┐
│ RAM │  │ NET │
└─────┘  └─────┘
```

Or in a horizontal row:

```
┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐
│ CPU │  │ GPU │  │ RAM │  │ NET │
└─────┘  └─────┘  └─────┘  └─────┘
```

---

## ✅ Final Checklist

Before declaring success:

- [ ] All 4 displays light up
- [ ] Each display shows correct data
- [ ] No flickering
- [ ] Serial monitor shows no errors
- [ ] Python script connects automatically
- [ ] Data updates every second
- [ ] Can run for 10+ minutes without issues

**Congratulations! Your hardware is ready! 🎉**

Next step: See [SOFTWARE.md](SOFTWARE.md) for software setup.
