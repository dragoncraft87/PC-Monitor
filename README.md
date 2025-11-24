# 🚀 PC Monitor - 4x GC9A01 Displays

**Intelligentes PC-Monitoring Dashboard mit 4 runden Displays - Powered by LVGL!**

Zeigt CPU, GPU, RAM und Network Stats in Echtzeit - wie das Amazon-Produkt, nur besser und selbstgebaut! 💪

---

## 🎨 NEUE LVGL VERSION!

**Dieses Projekt wurde komplett auf LVGL umgestellt!**

Die alte `graphics.c`-basierte Implementierung wurde durch eine professionelle **LVGL v9.3** UI ersetzt mit:
- ✅ Arc Widgets für CPU/GPU Gauges
- ✅ Bar Widget für RAM Anzeige
- ✅ Chart Widget für Network Graph
- ✅ PSRAM-optimiert für 4 Displays
- ✅ Smooth Animationen & Anti-Aliasing

**👉 Siehe [README_LVGL.md](README_LVGL.md) für die vollständige LVGL-Dokumentation!**

---

## ✨ Features

### Display 1: CPU Gauge ⚡
- Ring-Gauge (Tachometer-Style)
- CPU Auslastung 0-100%
- Echtzeit-Temperatur
- Farbwechsel bei hohen Temps

### Display 2: GPU Gauge 🎮
- Ring-Gauge für GPU %
- GPU Temperatur
- VRAM Nutzung (used/total GB)
- Cyan-Blau Farbverlauf

### Display 3: RAM Bar 📊
- Horizontaler Progress-Bar
- GB Anzeige (used/total)
- Segmentierter Balken
- Farbwechsel je nach Auslastung

### Display 4: Cyberpunk Network 🌐
- Connection Type (LAN/WiFi)
- Link Speed (10/100/1000/2500 Mbps)
- Live Traffic-Graph (60 Sekunden)
- Upload/Download Speed

---

## 🛠️ Hardware

### Was du brauchst (HAST DU ALLES!):

✅ **1x ESP32-S3-DevKitC-1** (aus Inventar ID: 1763124900393)
✅ **4x GC9A01 1.28" Round Display** (aus Inventar ID: 1763124948807)
✅ **Jumperkabel** (M/M, M/F - hast du reichlich)
✅ **USB-C Kabel** für ESP32 → PC

**Optional:**
- Breadboard zum Testen
- 3D-gedrucktes Gehäuse

---

## 📐 Verdrahtung

Siehe `WIRING.md` für detaillierte Anleitung!

**Quick Reference:**

Alle 4 Displays teilen sich:
- SCK (GPIO 18)
- MOSI (GPIO 23)

Jedes Display hat eigene:
- CS (Chip Select)
- DC (Data/Command)
- RST (Reset)

**Display 1 (CPU):**    CS=5,  DC=2,  RST=4
**Display 2 (GPU):**    CS=15, DC=16, RST=17
**Display 3 (RAM):**    CS=21, DC=22, RST=19
**Display 4 (Network):** CS=25, DC=26, RST=27

---

## 🚀 Installation

### 1. ESP32 Software flashen

```bash
cd pc-monitor-4displays

# Build
idf.py build

# Flash
idf.py flash

# Monitor
idf.py monitor
```

### 2. Python PC-Monitor starten

```bash
cd python

# Installiere Dependencies
pip install -r requirements.txt

# Starte Monitor
python pc_monitor.py
```

Das Script findet automatisch den ESP32 USB-Port!

---

## 📊 Datenformat

Das Python-Script sendet jede Sekunde einen String im Format:

```
CPU:45,CPUT:62.5,GPU:72,GPUT:68.3,VRAM:4.2/8.0,RAM:10.4/16.0,NET:LAN,SPEED:1000,DOWN:12.4,UP:2.1
```

Der ESP32 parsed das und aktualisiert alle 4 Displays!

---

## 🎨 Anpassungen

### Display-Farben ändern

In den `screen_*.c` Dateien kannst du die Farben anpassen:

```c
// Beispiel: CPU Ring-Farbe ändern
uint16_t color_start = RGB565(255, 0, 0);   // Rot
uint16_t color_end = RGB565(255, 255, 0);   // Gelb
```

### GPIO Pins ändern

In `main.c` Zeile 25-52 kannst du die Pin-Belegung anpassen.

### Update-Rate ändern

In `main.c` Zeile 182:
```c
vTaskDelay(pdMS_TO_TICKS(1000)); // 1000 = 1 Sekunde
```

---

## 🐛 Troubleshooting

### "ESP32 not found"
- Prüfe USB-Kabel
- Installiere CP210x Treiber
- Checke im Geräte-Manager (Windows)

### "Displays bleiben schwarz"
- Prüfe Verkabelung (siehe WIRING.md)
- Prüfe 3.3V Stromversorgung
- Checke SPI Pins (SCK, MOSI)

### "Falsche Werte"
- Prüfe Python-Script Output
- Checke USB Serial Verbindung
- Monitor mit `idf.py monitor`

### "GPU zeigt 0%"
- Installiere: `pip install GPUtil`
- Nur Nvidia GPUs unterstützt
- Für AMD: anpassen in pc_monitor.py

---

## 💡 Tipps

### Mehrere PCs monitoren?
Du hast 5 Displays! Nutze das 5. für einen zweiten PC oder als Reserve.

### Permanente Installation
- 3D-drucke ein Gehäuse
- Befestige Displays in Reihe oder Kreis
- Nutze kürzere Kabel für Clean-Look

### WiFi statt USB?
Der ESP32-S3 hat WiFi! Du könntest das Python-Script anpassen, um Daten über WiFi zu senden.

---

## 🎯 Nächste Schritte

1. ✅ Hardware verkabeln (siehe WIRING.md)
2. ✅ ESP32 flashen
3. ✅ Python-Script starten
4. ✅ Genießen! 🎉

**Dein DIY PC Monitor ist 10x cooler als das Amazon-Ding!** 🚀

---

## 📝 Lizenz

MIT License - Do whatever you want! 

Gebaut von Richard mit Hilfe von Claude 🤖
