# 📁 Datei-Übersicht - LVGL Version

Vollständige Übersicht aller Projekt-Dateien nach LVGL-Migration.

---

## 🎯 Neue LVGL-Dateien

### Core Implementation

| Datei | Zeilen | Beschreibung |
|-------|--------|--------------|
| `main/lv_conf.h` | ~140 | LVGL Konfiguration, PSRAM-Settings, Widget-Auswahl |
| `main/main_lvgl.c` | ~240 | Hauptprogramm mit LVGL-Init, 4 Display-Setup, Tasks |
| `main/lvgl_gc9a01_driver.h` | ~50 | Display-Treiber Header (API) |
| `main/lvgl_gc9a01_driver.c` | ~100 | Display-Treiber Implementation (GC9A01 + LVGL) |

### Screen Implementations

| Datei | Zeilen | Widget | Beschreibung |
|-------|--------|--------|--------------|
| `main/screens/screens_lvgl.h` | ~80 | - | Screen API Header |
| `main/screens/screen_cpu_lvgl.c` | ~120 | Arc | CPU Gauge (Ring mit % + Temp) |
| `main/screens/screen_gpu_lvgl.c` | ~130 | Arc | GPU Gauge (Ring mit % + Temp + VRAM) |
| `main/screens/screen_ram_lvgl.c` | ~110 | Bar | RAM Bar (Horizontal mit Segmenten) |
| `main/screens/screen_network_lvgl.c` | ~150 | Chart | Network Graph (60s Verlauf) |

**Total neue Dateien:** ~1020 Zeilen Code

---

## 📝 Dokumentation

| Datei | Beschreibung |
|-------|--------------|
| `README.md` | Haupt-README (aktualisiert mit LVGL-Hinweis) |
| `README_LVGL.md` | Vollständige LVGL-Dokumentation, Features, Installation |
| `BUILD_GUIDE.md` | Schritt-für-Schritt Build-Anleitung, Troubleshooting |
| `CHANGELOG_LVGL.md` | Detaillierte Änderungen, Migration-Guide |
| `FILES_OVERVIEW.md` | Diese Datei |
| `WIRING.md` | Pin-Belegung und Verkabelung |

---

## ⚙️ Konfiguration

| Datei | Beschreibung |
|-------|--------------|
| `main/idf_component.yml` | ESP-IDF Dependencies (LVGL v9.3, GC9A01 Driver) |
| `main/CMakeLists.txt` | Build-Konfiguration (geändert für LVGL) |
| `sdkconfig.defaults` | ESP32-S3 PSRAM-Konfiguration |
| `CMakeLists.txt` | Root CMake (unverändert) |

---

## 🗑️ Alte Dateien (Deprecated)

Diese Dateien werden **nicht mehr kompiliert**, bleiben aber als Referenz:

| Datei | Ersetzt durch | Status |
|-------|---------------|--------|
| `main/main.c` | `main_lvgl.c` | ❌ Deprecated |
| `main/graphics.c/.h` | LVGL Widgets | ❌ Deprecated |
| `main/bitmap_fonts.c/.h` | LVGL Fonts | ❌ Deprecated |
| `main/screens/screen_cpu.c` | `screen_cpu_lvgl.c` | ❌ Deprecated |
| `main/screens/screen_gpu.c` | `screen_gpu_lvgl.c` | ❌ Deprecated |
| `main/screens/screen_ram.c` | `screen_ram_lvgl.c` | ❌ Deprecated |
| `main/screens/screen_network.c` | `screen_network_lvgl.c` | ❌ Deprecated |
| `main/screens/screens.h` | `screens_lvgl.h` | ❌ Deprecated |

**Tipp:** Diese können gelöscht werden, falls du sie nicht mehr brauchst:
```bash
# Alte Files löschen (optional)
rm main/main.c
rm main/graphics.c main/graphics.h
rm main/bitmap_fonts.c main/bitmap_fonts.h
rm main/screens/screen_*.c  # Aber nicht screen_*_lvgl.c!
rm main/screens/screens.h   # Aber nicht screens_lvgl.h!
```

---

## 🔧 Hardware-Treiber (Unverändert)

| Datei | Beschreibung |
|-------|--------------|
| `main/gc9a01.c/.h` | GC9A01 Low-Level Display-Treiber (wird nicht mehr direkt genutzt) |

**Hinweis:** Die LVGL-Version nutzt den ESP-LCD GC9A01-Treiber aus dem Component Registry statt des lokalen `gc9a01.c`. Der lokale Treiber kann entfernt werden.

---

## 🎨 Design-Referenzen

| Datei | Beschreibung |
|-------|--------------|
| `screen_mockups/cpu-gauge.html` | CPU Gauge Design (Blau→Lila Ring) |
| `screen_mockups/gpu-gauge.html` | GPU Gauge Design (Cyan→Blau Ring) |
| `screen_mockups/ram-bars.html` | RAM Bar Design (Grün→Türkis Bar) |
| `screen_mockups/cyberpunk-style.html` | Network Design (Cyberpunk Graph) |
| `screen_mockups/multi-display-overview.html` | Alle 4 Displays zusammen |

Diese HTML-Dateien waren die Vorlage für die LVGL-Implementation!

---

## 🐍 Python Monitor

| Datei | Beschreibung |
|-------|--------------|
| `python/pc_monitor.py` | PC-Monitor Script (sendet Daten an ESP32) |
| `python/requirements.txt` | Python Dependencies |

**Unverändert!** Das Datenformat ist kompatibel mit der LVGL-Version.

---

## 📦 Managed Components (Auto-generiert)

```
managed_components/
├── lvgl__lvgl/                  # LVGL v9.3 (automatisch geladen)
├── espressif__esp_lcd_gc9a01/  # GC9A01 Driver
└── espressif__cmake_utilities/ # ESP-IDF Utilities
```

Diese werden automatisch vom ESP-IDF Component Manager heruntergeladen bei `idf.py reconfigure`.

---

## 🏗️ Build-Artefakte (Ignoriert)

```
build/              # Kompilierte Binaries
sdkconfig           # Generierte Konfiguration (aus sdkconfig.defaults)
dependencies.lock   # Component-Versionen
.vscode/            # VS Code Settings
```

Diese Dateien werden von `.gitignore` ausgeschlossen.

---

## 📊 Projekt-Statistik

### Code-Zeilen (ohne Kommentare)

| Kategorie | Zeilen | Dateien |
|-----------|--------|---------|
| LVGL Core | ~140 | 1 (lv_conf.h) |
| Main | ~240 | 1 (main_lvgl.c) |
| Display Driver | ~150 | 2 (driver.c/.h) |
| Screens | ~510 | 5 (screens_lvgl.h + 4x .c) |
| **Total NEU** | **~1040** | **9** |
| | |
| Documentation | ~2500 | 5 (.md files) |
| HTML Mockups | ~2000 | 11 (.html) |
| Python | ~300 | 2 (.py) |
| **Total Projekt** | **~5840** | **27** |

### Binär-Größen (nach Build)

| Component | Größe | % von Total |
|-----------|-------|-------------|
| LVGL Library | ~600 KB | 50% |
| Application | ~400 KB | 33% |
| ESP-IDF | ~200 KB | 17% |
| **Total Flash** | **~1.2 MB** | **7.5% von 16MB** |
| | |
| SRAM | ~150 KB | ~30% von 512KB |
| PSRAM (Buffers) | ~460 KB | ~6% von 8MB |

Noch **viel Platz** für Erweiterungen! 🚀

---

## 🔍 Datei-Abhängigkeiten

```
main_lvgl.c
    ├─→ lv_conf.h
    ├─→ lvgl_gc9a01_driver.h
    │   └─→ esp_lcd_gc9a01 (component)
    └─→ screens/screens_lvgl.h
        ├─→ screen_cpu_lvgl.c
        ├─→ screen_gpu_lvgl.c
        ├─→ screen_ram_lvgl.c
        └─→ screen_network_lvgl.c

Alle .c Files → lvgl.h (aus lvgl__lvgl component)
```

### Build-Reihenfolge

1. LVGL Component wird geladen
2. `lv_conf.h` wird von LVGL verwendet
3. Display-Treiber wird kompiliert
4. Screen-Implementierungen werden kompiliert
5. Main wird kompiliert und linkt alles zusammen

---

## 📋 Checkliste: Projekt-Sauberkeit

Optionale Aufräum-Schritte:

- [ ] Alte `main.c` umbenennen zu `main_old.c` (als Backup)
- [ ] `graphics.c/.h` löschen (nicht mehr benötigt)
- [ ] `bitmap_fonts.c/.h` löschen (LVGL Fonts werden verwendet)
- [ ] Alte `screens/screen_*.c` löschen (außer `*_lvgl.c`)
- [ ] Alte `screens.h` löschen (außer `screens_lvgl.h`)
- [ ] `gc9a01.c/.h` löschen (ESP-LCD Driver wird verwendet)
- [ ] `.gitignore` prüfen (build/, sdkconfig)

**Oder:** Alles behalten als Referenz/Backup!

---

## 🚀 Nächste Schritte

Mögliche Erweiterungen:

1. **5. Display hinzufügen:** Storage/Disk Usage
2. **WiFi OTA:** Drahtlose Updates
3. **Web-Interface:** Browser-Konfiguration
4. **Custom Themes:** Dark/Light Mode
5. **Alarme:** Warnungen bei hoher Temp/RAM

Alles möglich dank modularer LVGL-Architektur! 💪

---

## 📞 Kontakt & Support

- **Git Repository:** (Dein Repo)
- **Issues:** GitHub Issues
- **Docs:** Siehe `README_LVGL.md` und `BUILD_GUIDE.md`

---

**Letzte Aktualisierung:** 2025-01-24 (LVGL v2.0.0)
