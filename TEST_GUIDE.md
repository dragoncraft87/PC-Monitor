# 🧪 Display Test Guide

## Schnell-Anleitung: Jedes Display einzeln testen

### Schritt 1: Test Mode aktivieren

In `main/CMakeLists.txt` Zeile 4:
```cmake
set(USE_TEST_MODE ON)  # ON = Test Mode
```

### Schritt 2: Display auswählen

In `main/main_lvgl_test.c` Zeilen 28-31:

**Nur EINE Zeile aktiv lassen!**

```c
/* ============================================================================
 * TEST KONFIGURATION - NUR EINE ZEILE AKTIVIEREN!
 * ========================================================================== */
#define TEST_DISPLAY_1_CPU      1   // ← Display 1 testen
//#define TEST_DISPLAY_2_GPU      1   // ← Display 2 testen
//#define TEST_DISPLAY_3_RAM      1   // ← Display 3 testen
//#define TEST_DISPLAY_4_NETWORK  1   // ← Display 4 testen
```

### Schritt 3: Build & Flash

```powershell
idf.py build
idf.py -p COM3 flash monitor
```

### Schritt 4: Erwartetes Ergebnis

Das ausgewählte Display sollte zeigen:
- **Roter Hintergrund**
- **Weißer Text:** "TEST OK" (groß, mittig)

Die anderen 3 Displays bleiben **schwarz** (nicht initialisiert).

---

## 🔍 Alle 4 Displays nacheinander testen

### Display 1 - CPU (CS=11, DC=12, RST=13)

```c
#define TEST_DISPLAY_1_CPU      1   // ✓ Aktiviert
//#define TEST_DISPLAY_2_GPU      1
//#define TEST_DISPLAY_3_RAM      1
//#define TEST_DISPLAY_4_NETWORK  1
```

```powershell
idf.py build flash monitor
```

**Erwartung:** Display 1 zeigt roten Screen mit "TEST OK"

---

### Display 2 - GPU (CS=10, DC=9, RST=46)

```c
//#define TEST_DISPLAY_1_CPU      1
#define TEST_DISPLAY_2_GPU      1   // ✓ Aktiviert
//#define TEST_DISPLAY_3_RAM      1
//#define TEST_DISPLAY_4_NETWORK  1
```

```powershell
idf.py build flash monitor
```

**Erwartung:** Display 2 zeigt roten Screen mit "TEST OK"

---

### Display 3 - RAM (CS=3, DC=8, RST=18)

```c
//#define TEST_DISPLAY_1_CPU      1
//#define TEST_DISPLAY_2_GPU      1
#define TEST_DISPLAY_3_RAM      1   // ✓ Aktiviert
//#define TEST_DISPLAY_4_NETWORK  1
```

```powershell
idf.py build flash monitor
```

**Erwartung:** Display 3 zeigt roten Screen mit "TEST OK"

---

### Display 4 - Network (CS=15, DC=16, RST=17)

```c
//#define TEST_DISPLAY_1_CPU      1
//#define TEST_DISPLAY_2_GPU      1
//#define TEST_DISPLAY_3_RAM      1
#define TEST_DISPLAY_4_NETWORK  1   // ✓ Aktiviert
```

```powershell
idf.py build flash monitor
```

**Erwartung:** Display 4 zeigt roten Screen mit "TEST OK"

---

## ✅ Checkliste

Nach allen 4 Tests solltest du haben:

- [ ] Display 1 (CPU) funktioniert - zeigt roten Screen
- [ ] Display 2 (GPU) funktioniert - zeigt roten Screen
- [ ] Display 3 (RAM) funktioniert - zeigt roten Screen
- [ ] Display 4 (Network) funktioniert - zeigt roten Screen

---

## 🐛 Troubleshooting

### Display bleibt schwarz

**Prüfe Verkabelung:**
```
Display X:
- VCC → 3.3V
- GND → GND
- SCK → GPIO 4  (alle Displays)
- MOSI → GPIO 5 (alle Displays)
- CS → GPIO XX  (siehe Pin-Tabelle)
- DC → GPIO XX
- RST → GPIO XX
```

**Pin-Tabelle:**

| Display | CS  | DC  | RST |
|---------|-----|-----|-----|
| CPU     | 11  | 12  | 13  |
| GPU     | 10  | 9   | 46  |
| RAM     | 3   | 8   | 18  |
| Network | 15  | 16  | 17  |

### Initialisierung OK, aber kein Bild

Wenn im Monitor steht:
```
I (1309) LVGL_GC9A01: GC9A01 display initialized successfully
I (1319) DISPLAY-TEST: Creating test screen...
I (1329) DISPLAY-TEST: Test screen created and loaded!
```

Aber Display bleibt schwarz:

1. **Prüfe Stromversorgung:**
   ```
   Messe mit Multimeter:
   - 3.3V zwischen VCC und GND
   - Strom: ~100-200mA pro Display
   ```

2. **Prüfe SPI-Signale:**
   - SCK sollte takten beim Flush
   - MOSI sollte Daten senden
   - CS sollte LOW gehen beim Transfer

3. **Display-Test:**
   - Schließe Display an Arduino an (Test ob Hardware OK)
   - Teste mit einfachem Adafruit GC9A01 Sketch

### Monitor zeigt Fehler

**"SPI bus initialization failed"**
```c
// Prüfe ob SPI2_HOST nicht schon belegt ist
// Oder probiere SPI3_HOST
```

**"Panel init failed"**
```c
// RST Pin falsch?
// Prüfe ob RST-Pin mit Reset-Taste kollidiert
```

**"Allocating draw buffers failed"**
```c
// PSRAM nicht korrekt?
// Prüfe in menuconfig:
// Component config → ESP32S3-Specific → PSRAM
```

---

## 🔄 Zurück zu Normal Mode

Wenn alle 4 Displays einzeln funktionieren:

### In `main/CMakeLists.txt`:
```cmake
set(USE_TEST_MODE OFF)  # ← OFF = Normal Mode
```

### Build & Flash:
```powershell
idf.py build flash monitor
```

Jetzt sollten **alle 4 Displays gleichzeitig** ihre UI anzeigen!

---

## 📊 Log-Analyse

**Gutes Log (Display funktioniert):**
```
I (1309) LVGL_GC9A01: Initializing GC9A01 display (CS=11, DC=12, RST=13)
I (1339) gc9a01: LCD panel create success, version: 1.2.0
I (1469) LVGL_GC9A01: Draw buffers allocated: buf1=0x3c0b09a0, buf2=0x3c0b7ba4
I (1469) LVGL_GC9A01: GC9A01 display initialized successfully
I (1479) DISPLAY-TEST: Test screen created and loaded!
I (1489) DISPLAY-TEST: === System ready! ===
I (1499) DISPLAY-TEST: You should see a RED screen with 'TEST OK' text
```

**Schlechtes Log (Fehler):**
```
E (1234) LVGL_GC9A01: Failed to allocate draw buffers!
E (1245) gc9a01: Panel init failed!
```

---

## 💡 Tipps

1. **Teste in Reihenfolge:** CPU → GPU → RAM → Network
2. **Notiere welche funktionieren:** Checklist oben nutzen
3. **Bei Fehler:** Wiring prüfen, dann nächstes Display testen
4. **Foto machen:** Von funktionierenden Displays als Beweis 📸

---

**Viel Erfolg beim Testen! 🚀**
