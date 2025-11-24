# 🔨 Build Guide - PC Monitor mit LVGL

Schritt-für-Schritt Anleitung zum Kompilieren und Flashen des Projekts.

---

## Voraussetzungen

### Hardware
- ✅ ESP32-S3-DevKitC-1 **N16R8** (mit 8MB PSRAM!)
- ✅ 4x GC9A01 240x240 Round Displays
- ✅ USB-C Kabel

### Software
- ✅ ESP-IDF v5.0 oder höher
- ✅ Python 3.8+
- ✅ Git

---

## 1. ESP-IDF Installation

Falls noch nicht installiert:

### Windows

```powershell
# ESP-IDF Installer herunterladen
# https://dl.espressif.com/dl/esp-idf/

# Oder mit Chocolatey:
choco install esp-idf

# PowerShell neu starten
```

### Linux/Mac

```bash
# ESP-IDF klonen
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1

# Install script
./install.sh esp32s3

# Environment setup (zu ~/.bashrc hinzufügen)
. $HOME/esp/esp-idf/export.sh
```

**Verifizierung:**

```bash
idf.py --version
# Sollte "ESP-IDF v5.x" zeigen
```

---

## 2. Projekt Setup

```bash
# In den Projekt-Ordner wechseln
cd C:\Users\richa\Desktop\pc-monitor-poc

# Dependencies laden (LVGL wird automatisch geholt)
idf.py reconfigure

# Sollte zeigen:
# - lvgl/lvgl: ^9.3.0
# - espressif/esp_lcd_gc9a01
```

**Troubleshooting:**

Falls `idf_component.yml` nicht gefunden wird:

```bash
idf.py update-dependencies
```

---

## 3. PSRAM Konfiguration prüfen

```bash
idf.py menuconfig
```

Prüfe folgende Settings:

1. **Component config → ESP32S3-Specific**
   - `[*] Support for external, SPI-connected RAM`
   - Mode: `Octal Mode PSRAM`
   - Speed: `80MHz clock speed`

2. **Component config → ESP PSRAM**
   - `[*] Try to allocate memories of WiFi and LWIP in SPIRAM firstly`
   - `[*] Enable PSRAM malloc`

Falls nicht gesetzt: `sdkconfig.defaults` sollte automatisch geladen werden beim Build.

---

## 4. Build

```bash
# Clean build (empfohlen beim ersten Mal)
idf.py fullclean

# Build
idf.py build

# Sollte enden mit:
# Project build complete. To flash, run:
#   idf.py -p (PORT) flash
```

**Build-Zeit:** ~2-5 Minuten (beim ersten Mal länger)

**Erwartete Ausgabe:**

```
[100%] Built target pc-monitor-poc.elf
Project build complete. To flash, run:
  idf.py -p (PORT) flash
```

**Troubleshooting:**

### Error: "lvgl.h not found"

```bash
# Dependencies neu laden
idf.py reconfigure
rm -rf build/
idf.py build
```

### Error: "lv_conf.h not found"

Prüfe dass `main/lv_conf.h` existiert und in CMakeLists.txt `INCLUDE_DIRS "."` enthält.

### Error: "PSRAM not enabled"

```bash
# sdkconfig löschen und neu generieren
rm sdkconfig
idf.py reconfigure
idf.py build
```

---

## 5. USB Port finden

### Windows

```powershell
# Device Manager öffnen
devmgmt.msc

# Suche nach "USB Serial Device" oder "CP210x"
# Notiere COM-Port (z.B. COM3)
```

### Linux

```bash
# Automatisch finden
ls /dev/ttyUSB* /dev/ttyACM*

# Sollte zeigen: /dev/ttyUSB0 oder ähnlich
```

### Mac

```bash
ls /dev/cu.usb*
# Sollte zeigen: /dev/cu.usbserial-xyz
```

---

## 6. Flash auf ESP32

```bash
# Flash (ersetze COM3 mit deinem Port)
idf.py -p COM3 flash

# Mit Monitor (zeigt Log-Ausgabe)
idf.py -p COM3 flash monitor

# Nur Monitor (nach dem Flash)
idf.py -p COM3 monitor
```

**Erwartete Ausgabe beim Flash:**

```
Connecting....
Detecting chip type... ESP32-S3
Chip is ESP32-S3 (revision v0.1)
Features: WiFi, BLE
Crystal is 40MHz
...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
```

**Troubleshooting:**

### Error: "Failed to connect"

```bash
# ESP32 in Download Mode versetzen:
# 1. BOOT-Taste gedrückt halten
# 2. RESET-Taste drücken und loslassen
# 3. BOOT-Taste loslassen
# 4. Flash-Befehl erneut ausführen
```

### Error: "Permission denied (Linux)"

```bash
# User zu dialout-Gruppe hinzufügen
sudo usermod -a -G dialout $USER

# Logout und wieder Login
```

---

## 7. Monitor öffnen

```bash
idf.py -p COM3 monitor
```

**Erwartete Ausgabe:**

```
I (123) PC-MONITOR-LVGL: === PC Monitor 4x Display with LVGL ===
I (234) PC-MONITOR-LVGL: ESP32-S3 N16R8 with PSRAM
I (345) PC-MONITOR-LVGL: USB Serial JTAG configured
I (456) PC-MONITOR-LVGL: SPI bus initialized
I (567) PC-MONITOR-LVGL: Initializing LVGL...
I (678) LVGL_GC9A01: Initializing GC9A01 display (CS=11, DC=12, RST=13)
I (789) LVGL_GC9A01: Allocating draw buffers in PSRAM (19200 bytes each)
I (890) LVGL_GC9A01: Draw buffers allocated: buf1=0x3FC9A000, buf2=0x3FC9F000
I (991) LVGL_GC9A01: GC9A01 display initialized successfully
...
I (2345) PC-MONITOR-LVGL: All 4 displays initialized!
I (2456) PC-MONITOR-LVGL: === System ready! ===
```

**Monitor beenden:** `Ctrl+]`

---

## 8. Python PC-Monitor starten

In einem **zweiten Terminal**:

```bash
cd python

# Dependencies installieren (einmalig)
pip install -r requirements.txt

# Monitor starten
python pc_monitor.py

# Sollte zeigen:
# Found ESP32 on COM3
# Sending data every 1 second...
# CPU: 45% (62.5°C), GPU: 72% (68.3°C), RAM: 10.4/16.0 GB
```

**Troubleshooting:**

### Error: "ESP32 not found"

```bash
# Port manuell angeben
python pc_monitor.py --port COM3
```

### Error: "Module not found"

```bash
pip install --upgrade pip
pip install psutil GPUtil pyserial
```

---

## 9. Verifizierung

Nach erfolgreichem Flash solltest du sehen:

1. ✅ **Display 1 (CPU):** Blauer Ring mit Prozent-Anzeige
2. ✅ **Display 2 (GPU):** Cyan Ring mit Prozent + VRAM
3. ✅ **Display 3 (RAM):** Grüner Balken mit GB-Anzeige
4. ✅ **Display 4 (Network):** Cyan/Magenta Graph

**Falls Displays schwarz bleiben:**

1. Prüfe Verkabelung (siehe WIRING.md)
2. Prüfe 3.3V/GND an allen Displays
3. Checke Monitor-Log für Fehlermeldungen
4. Teste ein Display einzeln

---

## 10. Debugging

### Verbose Logging aktivieren

```bash
idf.py menuconfig

# Component config → Log output
# → Default log verbosity → Verbose
```

Neu builden und flashen.

### Display-Test

Füge in `main_lvgl.c` nach Display-Init hinzu:

```c
// Test: Display rot füllen
lv_obj_set_style_bg_color(screen_cpu->screen, lv_color_make(0xff, 0, 0), 0);
vTaskDelay(pdMS_TO_TICKS(2000));
```

### Memory Check

```bash
# Im Monitor:
I (2345) heap_init: At 3FCA0000 len 00060000 (384 KiB): DRAM
I (2346) heap_init: At 3C000000 len 00800000 (8192 KiB): SPIRAM
```

Sollte 8 MB SPIRAM zeigen!

---

## 11. OTA Updates (Optional)

Für drahtlose Updates:

```bash
idf.py menuconfig

# Partition Table
# → Partition table → Factory app, two OTA definitions
```

Dann kann mit `idf.py ota` über WiFi geflasht werden.

---

## 12. Performance Tuning

### Optimization Level

```bash
idf.py menuconfig

# Compiler options
# → Optimization Level → Optimize for performance (-O2)
```

### FreeRTOS Tick Rate

In `sdkconfig.defaults`:

```ini
CONFIG_FREERTOS_HZ=1000  # 1ms tick (Default: 100)
```

### Display Buffer Size

In `lvgl_gc9a01_driver.c:14`:

```c
#define GC9A01_BUF_SIZE (GC9A01_WIDTH * 60) // 60 Zeilen statt 40
```

Mehr Buffer = schneller, aber mehr RAM-Nutzung.

---

## 📊 Resource Usage

Nach dem Build:

```
Flash usage: ~1.2 MB / 16 MB
SRAM usage: ~150 KB / 512 KB
PSRAM usage: ~460 KB / 8 MB (4x Display-Buffer)
```

Noch viel Platz für Erweiterungen! 🚀

---

## ✅ Checkliste

Vor dem ersten Build:

- [ ] ESP-IDF v5.x installiert
- [ ] ESP32-S3 N16R8 Board vorhanden
- [ ] USB-C Kabel verbunden
- [ ] Displays verkabelt (siehe WIRING.md)
- [ ] Python 3.8+ installiert
- [ ] Git installiert

Build-Steps:

- [ ] `idf.py reconfigure` erfolgreich
- [ ] `idf.py build` erfolgreich
- [ ] `idf.py flash` erfolgreich
- [ ] Monitor zeigt "System ready!"
- [ ] Displays zeigen UI
- [ ] Python-Script läuft
- [ ] Daten werden aktualisiert

---

## 🆘 Support

Bei Problemen:

1. **Monitor-Log prüfen:** `idf.py monitor`
2. **Verkabelung prüfen:** Siehe WIRING.md
3. **Clean Build:** `idf.py fullclean && idf.py build`
4. **GitHub Issues:** Öffne ein Issue mit Log-Ausgabe

---

**Happy Building! 🔨**
