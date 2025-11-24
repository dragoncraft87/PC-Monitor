# 🚀 PC Monitor - 4x GC9A01 Displays mit LVGL

**Intelligentes PC-Monitoring Dashboard mit 4 runden Displays - Powered by LVGL!**

Zeigt CPU, GPU, RAM und Network Stats in Echtzeit mit professioneller LVGL-UI! 💪

---

## ✨ Was ist neu? (LVGL Version)

### 🎨 Komplett neue UI mit LVGL v9.3

Die alte `graphics.c`-basierte Implementierung wurde komplett durch **LVGL (Light and Versatile Graphics Library)** ersetzt!

**Vorteile:**
- ✅ Professionelle Widget-Bibliothek (Arc, Bar, Chart)
- ✅ Smooth Animationen und Transitions
- ✅ PSRAM-optimiert für 4 Displays gleichzeitig
- ✅ Anti-Aliasing für glatte Kreise und Arcs
- ✅ Einfachere Wartung und Erweiterung
- ✅ Multi-Display Support out-of-the-box

---

## 📊 Display-Übersicht

### Display 1: CPU Gauge ⚡
- **Widget:** Arc (Ring-Gauge)
- **Anzeige:** CPU Auslastung 0-100%
- **Details:** Echtzeit-Temperatur mit Farbwechsel
- **Farben:** Blau (#667eea) → Lila (#764ba2)
- **Datei:** `screens/screen_cpu_lvgl.c`

### Display 2: GPU Gauge 🎮
- **Widget:** Arc (Ring-Gauge)
- **Anzeige:** GPU Auslastung 0-100%
- **Details:** GPU Temp + VRAM Nutzung
- **Farben:** Cyan (#4cc9f0) → Blau (#4361ee)
- **Datei:** `screens/screen_gpu_lvgl.c`

### Display 3: RAM Bar 📊
- **Widget:** Bar (Horizontaler Balken)
- **Anzeige:** RAM Nutzung in GB und %
- **Details:** Farbwechsel je nach Auslastung
- **Farben:** Grün → Orange → Rot
- **Datei:** `screens/screen_ram_lvgl.c`

### Display 4: Network Graph 🌐
- **Widget:** Chart (Line Chart)
- **Anzeige:** 60 Sekunden Traffic-Verlauf
- **Details:** Connection Type, Speed, Down/Up
- **Style:** Cyberpunk (Cyan/Magenta)
- **Datei:** `screens/screen_network_lvgl.c`

---

## 🛠️ Hardware

### Was du brauchst:

✅ **1x ESP32-S3-DevKitC-1 N16R8** (mit 8MB PSRAM!)
✅ **4x GC9A01 1.28" Round Display** (240x240 SPI)
✅ **Jumperkabel** (M/M, M/F)
✅ **USB-C Kabel** für ESP32 → PC

**Wichtig:** Der ESP32-S3 **muss** PSRAM haben (N16R8 = 16MB Flash + 8MB PSRAM)!
Die 4 Display-Framebuffer benötigen ~460 KB RAM, nur mit PSRAM möglich.

---

## 📐 Verdrahtung

Siehe `WIRING.md` für detaillierte Anleitung!

**Alle 4 Displays teilen sich SPI:**
- **SCK:** GPIO 4
- **MOSI:** GPIO 5

**Jedes Display hat eigene Pins:**

| Display | CS  | DC  | RST |
|---------|-----|-----|-----|
| CPU     | 11  | 12  | 13  |
| GPU     | 10  | 9   | 46  |
| RAM     | 3   | 8   | 18  |
| Network | 15  | 16  | 17  |

---

## 🚀 Installation

### 1. ESP-IDF Setup

```bash
# ESP-IDF 5.x installieren (falls noch nicht vorhanden)
# https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

# Im Projekt-Ordner:
cd pc-monitor-poc

# Dependencies holen (LVGL wird automatisch geladen)
idf.py reconfigure

# Build
idf.py build

# Flash auf ESP32
idf.py -p COM3 flash  # Port anpassen!

# Monitor öffnen
idf.py -p COM3 monitor
```

### 2. Python PC-Monitor starten

```bash
cd python

# Dependencies installieren
pip install -r requirements.txt

# Monitor starten
python pc_monitor.py
```

Das Script findet automatisch den ESP32 USB-Port und sendet Daten!

---

## 🎨 LVGL Konfiguration

### lv_conf.h Highlights

Die `main/lv_conf.h` ist optimiert für 4 Displays:

```c
// PSRAM für Frame-Buffer
#define LV_MEM_CUSTOM 1
#define LV_MEM_SIZE (128 * 1024U)

// Nur benötigte Widgets aktiviert
#define LV_USE_ARC 1      // CPU/GPU Gauges
#define LV_USE_BAR 1      // RAM Bar
#define LV_USE_CHART 1    // Network Graph
#define LV_USE_LABEL 1    // Text
```

### PSRAM-Nutzung

Die `sdkconfig.defaults` aktiviert Octal PSRAM:

```ini
CONFIG_ESP32S3_SPIRAM_SUPPORT=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=0
```

Jedes Display nutzt ~115 KB PSRAM für Double-Buffering (2x 40 Zeilen @ 240px).

---

## 📁 Projekt-Struktur (Neue LVGL Version)

```
pc-monitor-poc/
├── main/
│   ├── main_lvgl.c              # Neue Main mit LVGL init
│   ├── lv_conf.h                # LVGL Konfiguration
│   ├── lvgl_gc9a01_driver.c/.h  # Display-Treiber für LVGL
│   ├── idf_component.yml        # Dependencies (LVGL)
│   ├── CMakeLists.txt           # Build-Konfiguration
│   └── screens/
│       ├── screens_lvgl.h       # Screen API
│       ├── screen_cpu_lvgl.c    # CPU Screen (Arc Widget)
│       ├── screen_gpu_lvgl.c    # GPU Screen (Arc Widget)
│       ├── screen_ram_lvgl.c    # RAM Screen (Bar Widget)
│       └── screen_network_lvgl.c # Network Screen (Chart Widget)
├── screen_mockups/              # HTML UI-Mockups
├── python/                      # PC Monitor Script
├── sdkconfig.defaults           # ESP32-S3 PSRAM Config
└── README_LVGL.md              # Diese Datei
```

**Alte Dateien (nicht mehr verwendet):**
- ❌ `main.c` (ersetzt durch `main_lvgl.c`)
- ❌ `graphics.c/.h` (ersetzt durch LVGL)
- ❌ `bitmap_fonts.c/.h` (LVGL nutzt eigene Fonts)
- ❌ `screens/screen_*.c` (ersetzt durch `*_lvgl.c`)

---

## 📊 Datenformat

Unverändert! Das Python-Script sendet weiterhin:

```
CPU:45,CPUT:62.5,GPU:72,GPUT:68.3,VRAM:4.2/8.0,RAM:10.4/16.0,NET:LAN,SPEED:1000,DOWN:12.4,UP:2.1
```

---

## 🎯 Anpassungen

### Display-Farben ändern

In den `screen_*_lvgl.c` Dateien:

```c
// Beispiel: CPU Arc-Farbe ändern
lv_obj_set_style_arc_color(s->arc,
    lv_color_make(0xff, 0x00, 0x00), // Rot statt Blau
    LV_PART_INDICATOR);
```

### Fonts ändern

In `lv_conf.h` weitere Montserrat-Größen aktivieren:

```c
#define LV_FONT_MONTSERRAT_48 1  // Für noch größere Zahlen
```

Dann in den Screen-Dateien:

```c
lv_obj_set_style_text_font(label, &lv_font_montserrat_48, 0);
```

### Update-Rate ändern

In `main_lvgl.c:235`:

```c
vTaskDelay(pdMS_TO_TICKS(500)); // 500ms = 2x pro Sekunde
```

---

## 🐛 Troubleshooting

### "PSRAM not found"
- **Problem:** ESP32-S3 ohne PSRAM oder falsche Konfiguration
- **Lösung:** Prüfe ob dein Board **N16R8** ist (8MB PSRAM)
- **Check:** `idf.py menuconfig` → Component config → ESP32S3-specific

### "lvgl.h not found"
- **Problem:** LVGL Component nicht geladen
- **Lösung:** `idf.py reconfigure` oder lösche `build/` und neu builden

### "Display bleibt schwarz"
- **Problem:** SPI-Pins oder Display-Init fehlerhaft
- **Debug:** Checke `idf.py monitor` für Fehlermeldungen
- **Test:** `lvgl_gc9a01_init()` muss `ESP_OK` returnen

### "Frame-Buffer allocation failed"
- **Problem:** PSRAM nicht korrekt konfiguriert
- **Lösung:** `sdkconfig.defaults` übernehmen und neu builden
- **Check:** Im Monitor sollte "Draw buffers allocated in PSRAM" erscheinen

### "Arc/Bar wird nicht angezeigt"
- **Problem:** Widget außerhalb des sichtbaren Bereichs
- **Debug:** Prüfe `lv_obj_align()` und Koordinaten
- **Tipp:** 240x240 Display, Center ist (120, 120)

---

## 💡 Tipps & Tricks

### Performance optimieren

1. **Partial Rendering:** Nur geänderte Bereiche neu zeichnen
   ```c
   lv_display_set_buffers(..., LV_DISPLAY_RENDER_MODE_PARTIAL);
   ```

2. **Task Prioritäten:** LVGL Timer hat höhere Prio als Display Update
   ```c
   xTaskCreate(lvgl_timer_task, ..., 5, NULL);  // Prio 5
   xTaskCreate(display_update_task, ..., 4, NULL); // Prio 4
   ```

3. **Reduziere Updates:** Nur bei Wertänderung neu zeichnen
   ```c
   if (stats->cpu_percent != last_cpu_percent) {
       lv_arc_set_value(s->arc, stats->cpu_percent);
   }
   ```

### Mehr Displays hinzufügen

Das 5. Display kannst du einfach hinzufügen:

1. Config in `main_lvgl.c` erstellen
2. `lvgl_gc9a01_init()` mit neuen Pins aufrufen
3. Neuen Screen erstellen (z.B. `screen_storage_lvgl.c`)
4. In `display_update_task()` updaten

### Eigene Widgets erstellen

LVGL macht es einfach! Beispiel: Custom Gauge

```c
lv_obj_t *custom_gauge = lv_obj_create(screen);
lv_obj_set_size(custom_gauge, 100, 100);
// Weitere Anpassungen...
```

Siehe [LVGL Docs](https://docs.lvgl.io/) für mehr Infos.

---

## 🔧 Tools & Links

### Benötigte Software

- **ESP-IDF 5.x:** https://docs.espressif.com/projects/esp-idf/
- **Python 3.8+:** https://python.org
- **USB Serial Driver:** CP210x (meist automatisch)

### LVGL Resources

- **LVGL Dokumentation:** https://docs.lvgl.io/
- **LVGL Simulator:** https://lvgl.io/tools/simulator
- **SquareLine Studio:** GUI-Designer für LVGL (optional)

### Komponenten

- **LVGL v9.3:** ESP-IDF Component Manager
- **GC9A01 Driver:** `esp_lcd_gc9a01` Component

---

## 🎯 Nächste Schritte

1. ✅ Hardware verkabeln (siehe WIRING.md)
2. ✅ ESP-IDF installieren
3. ✅ Projekt builden & flashen
4. ✅ Python-Script starten
5. ✅ UI-Mockups in `screen_mockups/` anschauen
6. ✅ Genießen! 🎉

**Dein DIY PC Monitor ist jetzt 100x professioneller mit LVGL!** 🚀

---

## 📝 Migration von old → LVGL

Falls du die alte Version hattest:

1. **Backup:** Sichere alte `main.c`, `graphics.c`
2. **Clean:** `rm -rf build/` und `rm sdkconfig`
3. **Build:** `idf.py reconfigure && idf.py build`
4. **Flash:** `idf.py -p COM3 flash`

Die alte Implementation bleibt als Referenz im Git-History.

---

## 🙏 Credits

- **Hardware:** Richard's Custom Wiring
- **LVGL:** LVGL Team (https://lvgl.io)
- **ESP32-S3:** Espressif Systems
- **GC9A01 Driver:** Espressif Component Registry
- **UI Design:** Inspiriert von Cyberpunk & Modern Dashboard UIs

---

## 📝 Lizenz

MIT License - Do whatever you want!

**Built with ❤️ by Richard with Claude AI 🤖**
