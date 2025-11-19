# 📋 Quick Reference Guide

## 🔍 Port finden (Windows)

**Gerätemanager öffnen:**
```
Win + X → Geräte-Manager → Anschlüsse (COM & LPT)
```

Dort siehst du z.B.:
```
USB-SERIAL CH340 (COM3)
```
→ Das ist dein ESP32!

**Python Auto-Detection:**
```bash
python pc_monitor.py
# Findet automatisch: "Found potential ESP32 at: COM3"
```

---

## 💻 Beispiel-Ausgaben

### ESP32 Serial Monitor (erfolgreich)

```
I (0) cpu_start: Starting scheduler on APP CPU.
I (327) MAIN: =================================
I (327) MAIN: PC Monitor - ESP32-S3 Proof of Concept
I (337) MAIN: Display: GC9A01 1.28" Round IPS
I (337) MAIN: =================================
I (347) USB_COMM: Initializing USB CDC communication
I (347) USB_COMM: USB CDC ready on UART0
I (357) GC9A01: Initializing GC9A01 display
I (1087) GC9A01: GC9A01 initialized successfully
I (3087) MAIN: System initialized, waiting for data...
I (4087) USB_COMM: Parsed: CPU=45.2% TEMP=62.5C RAM=65.3% (10.4/16.0GB) GPU=23.1% GPUT=55.0C
I (4087) DISPLAY_UI: Display updated - CPU: 45.2% (62.5C), RAM: 65.3%
```

✅ **Alles gut!**

### ESP32 Serial Monitor (Fehler)

```
E (1087) GC9A01: Failed to initialize SPI bus
E (1087) GC9A01: Check wiring!
```

❌ **Problem:** Verkabelung prüfen!

---

### Python Monitor (erfolgreich)

```
╔══════════════════════════════════════════════════╗
║        PC Monitor - ESP32 Hardware Stats         ║
║              Proof of Concept v1.0               ║
╚══════════════════════════════════════════════════╝

Auto-detecting ESP32...
Found potential ESP32 at: COM3 (USB-SERIAL CH340)
Connecting to COM3 at 115200 baud...
✓ Connected to ESP32!

==================================================
PC Monitor Running
Sending stats to ESP32 every second...
Press Ctrl+C to stop
==================================================

[14:23:45] CPU:  45.2% (62.5°C) | RAM:  65.3% (10.4/16.0GB) | GPU:  23.1% (55.0°C)
[14:23:46] CPU:  46.8% (63.1°C) | RAM:  65.3% (10.4/16.0GB) | GPU:  25.4% (55.5°C)
```

✅ **Perfect!**

### Python Monitor (Fehler - Port nicht gefunden)

```
ERROR: Could not find ESP32 USB CDC port!

Available ports:
  COM1: Communications Port (COM1)
  COM5: Intel(R) Active Management Technology - SOL
```

❌ **Problem:** ESP32 nicht eingesteckt oder Treiber fehlt

---

## 🛠️ Häufige Befehle

### ESP-IDF Commands

```bash
# Projekt bauen
idf.py build

# Flash + Monitor in einem
idf.py -p COM3 flash monitor

# Nur flashen
idf.py -p COM3 flash

# Nur monitor
idf.py -p COM3 monitor

# Monitor beenden
Ctrl + ]

# Vollständig neu bauen
idf.py fullclean
idf.py build

# USB Port automatisch erkennen
idf.py flash monitor
```

### Python Commands

```bash
# Dependencies installieren
pip install -r requirements.txt

# Script starten
python pc_monitor.py

# Mit spezifischem Port
python -c "from pc_monitor import PCMonitor; m = PCMonitor(port='COM3'); m.run()"

# Script stoppen
Ctrl + C
```

---

## 🎨 Pin-Referenz (Schnell-Copy)

```c
// ESP32-S3 zu GC9A01
#define PIN_NUM_MOSI   23  // SDA
#define PIN_NUM_CLK    18  // SCK
#define PIN_NUM_CS     5   // CS
#define PIN_NUM_DC     2   // DC
#define PIN_NUM_RST    4   // RST
// 3.3V → VCC
// GND  → GND
```

---

## 📊 Daten-Protokoll

### USB CDC Format (ESP32 empfängt):

```
CPU:45.2,TEMP:62.5,RAM:65.3,RAMU:10.4,RAMT:16.0,GPU:23.1,GPUT:55.0\n
```

**Felder:**
- `CPU` = CPU Auslastung (%)
- `TEMP` = CPU Temperatur (°C)
- `RAM` = RAM Auslastung (%)
- `RAMU` = RAM verwendet (GB)
- `RAMT` = RAM total (GB)
- `GPU` = GPU Auslastung (%)
- `GPUT` = GPU Temperatur (°C)

**Wichtig:** 
- Endet mit `\n` (Newline)
- Float-Werte mit `.` (nicht `,`)
- Keine Leerzeichen!

---

## 🔧 Troubleshooting Quick-Fixes

### Display bleibt schwarz

```bash
# 1. Check Logs
idf.py monitor

# 2. Test einzelne Pins mit Multimeter
# VCC: 3.3V?
# GND: Verbunden?

# 3. Pin-Nummern prüfen in gc9a01.h
```

### "Permission denied" (Linux)

```bash
sudo chmod 666 /dev/ttyUSB0
# oder
sudo usermod -a -G dialout $USER
# dann logout/login
```

### Python findet Modul nicht

```bash
# Virtual Environment nutzen
python -m venv venv
source venv/bin/activate  # Linux/Mac
venv\Scripts\activate     # Windows
pip install -r requirements.txt
```

### Display zeigt Müll/Artefakte

```bash
# SPI Clock zu schnell? In gc9a01.c ändern:
.clock_speed_hz = 20 * 1000 * 1000, // 20 MHz statt 40 MHz
```

---

## 🎯 Performance Tuning

### Schnellere Updates

**ESP32 (main.c):**
```c
vTaskDelay(pdMS_TO_TICKS(50));  // 50ms = 20 FPS
```

**Python (pc_monitor.py):**
```python
time.sleep(0.5)  # 500ms = 2 Hz
```

### Niedrigere CPU-Last

**ESP32 (display_ui.c):**
```c
// Nur Update wenn sich Werte ändern
if (abs(stats->cpu_usage - last_cpu) > 1.0) {
    // Update display
}
```

---

## 📞 Help!

**Bei Problemen:**

1. ✅ README.md komplett lesen
2. ✅ WIRING.md Checkliste durchgehen
3. ✅ Serial Monitor Output checken
4. ✅ Verkabelung mit Multimeter prüfen
5. ✅ Google den Error Code
6. ✅ ESP-IDF Dokumentation: docs.espressif.com

**Logs sammeln:**
```bash
# ESP32 Log
idf.py monitor > esp32_log.txt

# Python Log
python pc_monitor.py > python_log.txt 2>&1
```

---

## 🎉 Es läuft? Awesome!

### Nächste Schritte:

1. 📸 Foto machen und stolz sein!
2. 🎨 Farben anpassen
3. 📊 Andere Werte hinzufügen
4. 🖼️ Gehäuse designen & drucken
5. 💡 Mehr Displays hinzufügen

**Viel Spaß beim Coden! 🚀**
