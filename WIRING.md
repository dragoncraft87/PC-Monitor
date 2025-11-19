# 🔌 Verdrahtungsanleitung - 4x GC9A01 Displays

## 📋 Übersicht

Du verbindest 4 GC9A01 Displays mit einem ESP32-S3 über SPI.

**Wichtig:** Alle Displays teilen sich den SPI-Bus (SCK + MOSI), aber jedes hat eigene CS/DC/RST Pins!

---

## 🎯 Pin-Belegung Tabelle

| Display | Function | ESP32 GPIO | GC9A01 Pin |
|---------|----------|-----------|-----------|
| **Alle 4** | SCK  | GPIO 4    | SCL/SCK   |
| **Alle 4** | MOSI | GPIO 5    | SDA/MOSI  |
| **Alle 4** | VCC  | 3.3V      | VCC       |
| **Alle 4** | GND  | GND       | GND       |
|||
| **Display 1 (CPU)** | CS  | GPIO 11   | CS        |
| **Display 1 (CPU)** | DC  | GPIO 12   | DC        |
| **Display 1 (CPU)** | RST | GPIO 13   | RST       |
|||
| **Display 2 (GPU)** | CS  | GPIO 10   | CS        |
| **Display 2 (GPU)** | DC  | GPIO 9    | DC        |
| **Display 2 (GPU)** | RST | GPIO 46   | RST       |
|||
| **Display 3 (RAM)** | CS  | GPIO 3    | CS        |
| **Display 3 (RAM)** | DC  | GPIO 8    | DC        |
| **Display 3 (RAM)** | RST | GPIO 18   | RST       |
|||
| **Display 4 (Network)** | CS  | GPIO 15   | CS        |
| **Display 4 (Network)** | DC  | GPIO 16   | DC        |
| **Display 4 (Network)** | RST | GPIO 17   | RST       |

---

## 🔧 Schritt-für-Schritt Anleitung

### Schritt 1: Shared SPI Bus verkabeln

**Diese Leitungen gehen zu ALLEN 4 Displays:**

```
ESP32 GPIO 18 (SCK)  ──┬── Display 1 SCK
                       ├── Display 2 SCK
                       ├── Display 3 SCK
                       └── Display 4 SCK

ESP32 GPIO 23 (MOSI) ──┬── Display 1 MOSI
                       ├── Display 2 MOSI
                       ├── Display 3 MOSI
                       └── Display 4 MOSI

ESP32 3.3V ────────────┬── Display 1 VCC
                       ├── Display 2 VCC
                       ├── Display 3 VCC
                       └── Display 4 VCC

ESP32 GND ─────────────┬── Display 1 GND
                       ├── Display 2 GND
                       ├── Display 3 GND
                       └── Display 4 GND
```

**Tipp:** Nutze ein Breadboard, um die gemeinsamen Leitungen zu verteilen!

---

### Schritt 2: Individuelle Control Pins

**Jedes Display bekommt eigene CS, DC und RST Pins:**

#### Display 1 (CPU):
```
ESP32 GPIO 5  → Display 1 CS
ESP32 GPIO 2  → Display 1 DC
ESP32 GPIO 4  → Display 1 RST
```

#### Display 2 (GPU):
```
ESP32 GPIO 15 → Display 2 CS
ESP32 GPIO 16 → Display 2 DC
ESP32 GPIO 17 → Display 2 RST
```

#### Display 3 (RAM):
```
ESP32 GPIO 21 → Display 3 CS
ESP32 GPIO 22 → Display 3 DC
ESP32 GPIO 19 → Display 3 RST
```

#### Display 4 (Network):
```
ESP32 GPIO 25 → Display 4 CS
ESP32 GPIO 26 → Display 4 DC
ESP32 GPIO 27 → Display 4 RST
```

---

## 📸 Visuelles Diagramm

```
        ESP32-S3-DevKitC-1
        ┌──────────────────┐
        │                  │
GPIO 18 │ SCK          USB │ ← PC
GPIO 23 │ MOSI             │
GPIO 5  │ CS1          3.3V│ ←┐
GPIO 2  │ DC1           GND│ ←┼─ Zu allen Displays
GPIO 4  │ RST1             │  │
GPIO 15 │ CS2              │  │
GPIO 16 │ DC2              │  │
GPIO 17 │ RST2             │  │
GPIO 21 │ CS3              │  │
GPIO 22 │ DC3              │  │
GPIO 19 │ RST3             │  │
GPIO 25 │ CS4              │  │
GPIO 26 │ DC4              │  │
GPIO 27 │ RST4             │  │
        └──────────────────┘  │
                              │
    ┌─────────────────────────┘
    │   ┌─────────────────────────────┐
    │   │                             │
    ▼   ▼    Display 1 (CPU)          │
  [Display]  ┌─────────────┐          │
  SCK  MOSI  │ GC9A01      │          │
  ─┬────┬────│  240x240    │          │
   │    │    │   Round     │          │
   │    │    └─────────────┘          │
   │    │    CS=5, DC=2, RST=4        │
   │    │                             │
   │    │    Display 2 (GPU)          │
   │    │    ┌─────────────┐          │
   │    └────│ GC9A01      │          │
   │         │  240x240    │          │
   │         │   Round     │          │
   │         └─────────────┘          │
   │         CS=15, DC=16, RST=17     │
   │                                  │
   │         Display 3 (RAM)          │
   │         ┌─────────────┐          │
   └─────────│ GC9A01      │          │
             │  240x240    │          │
             │   Round     │          │
             └─────────────┘          │
             CS=21, DC=22, RST=19     │
                                      │
             Display 4 (Network)      │
             ┌─────────────┐          │
             │ GC9A01      │ ◄────────┘
             │  240x240    │
             │   Round     │
             └─────────────┘
             CS=25, DC=26, RST=27
```

---

## ⚡ Stromversorgung

**Wichtig:** 4 Displays ziehen ca. 400-500 mA zusammen!

✅ **USB-Power vom PC reicht aus** (USB 2.0 = 500mA, USB 3.0 = 900mA)

Wenn die Displays flackern:
- Nutze USB 3.0 Port (mehr Strom)
- Oder nutze powered USB-Hub
- Oder externes 5V Netzteil

---

## 🧪 Test-Reihenfolge

1. **Erst 1 Display** anschließen und testen
2. Wenn es funktioniert, **2. Display** dazu
3. Wenn beide funktionieren, **3. Display** dazu
4. Wenn alle 3 funktionieren, **4. Display** dazu

So findest du Fehler schneller!

---

## 🔍 Checkliste

Bevor du flashst, checke:

- [ ] Alle SCK Leitungen verbunden?
- [ ] Alle MOSI Leitungen verbunden?
- [ ] Alle VCC (3.3V) verbunden?
- [ ] Alle GND verbunden?
- [ ] Jedes Display hat eigenen CS Pin?
- [ ] Jedes Display hat eigenen DC Pin?
- [ ] Jedes Display hat eigenen RST Pin?
- [ ] USB-Kabel angeschlossen (PC → ESP32)?

---

## 💡 Profi-Tipps

### Tip 1: Nutze Breadboard
Verbinde erst ESP32 mit Breadboard, dann verteile von dort die Power-Rails zu den Displays.

### Tip 2: Farbige Kabel
- **Rot:** VCC (3.3V)
- **Schwarz:** GND
- **Gelb:** SCK
- **Grün:** MOSI
- **Andere Farben:** CS, DC, RST

### Tip 3: Beschrifte Displays
Schreibe mit Edding auf die Rückseite:
- "Display 1 - CPU"
- "Display 2 - GPU"
- etc.

### Tip 4: Kurze Kabel
Je kürzer die Kabel, desto stabiler das Signal! Ideal: < 10cm

---

## ❓ Häufige Fehler

### "Display bleibt weiß"
→ SCK oder MOSI nicht verbunden

### "Display bleibt schwarz"
→ VCC oder GND nicht verbunden

### "Nur ein Display funktioniert"
→ CS Pins vertauscht oder doppelt belegt

### "Displays zeigen falschen Content"
→ CS Pins in falscher Reihenfolge

### "Flackern"
→ Zu wenig Strom (nutze USB 3.0)

---

**Ready to Rock! 🚀**

Wenn alles verkabelt ist:
```bash
cd pc-monitor-4displays
idf.py flash monitor
```
