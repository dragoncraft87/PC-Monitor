# 🖥️ PC Monitor - System Tray Application

Windows System-Tray-Anwendung zur Verwaltung des PC Monitors und OpenHardwareMonitor.

## ✨ Features

### System Tray Icon
- **Minimierter Start**: Läuft als Icon in der Taskleiste (rechts unten)
- **Visueller Status**:
  - 🟢 Grün = Monitoring aktiv
  - 🔴 Rot = Monitoring gestoppt
- **Rechtsklick-Menü** für alle Funktionen

### Funktionen im Menü

| Menüpunkt | Beschreibung |
|-----------|--------------|
| **OpenHardwareMonitor Pfad definieren** | Öffnet File-Browser zur Auswahl der OpenHardwareMonitor.exe |
| **Start** | Startet OpenHardwareMonitor + PC Monitor Script |
| **Stop** | Stoppt das PC Monitor Script |
| **Alles beenden** | Beendet die Tray-App komplett |

## 🚀 Quick Start

### Methode 1: EXE verwenden (empfohlen)

```bash
# 1. EXE bauen
build_exe.bat

# 2. EXE starten (aus dist\ Ordner)
.\dist\PC Monitor Tray.exe
```

### Methode 2: Python direkt

```bash
# 1. Dependencies installieren
pip install -r requirements_tray.txt

# 2. Script starten
python pc_monitor_tray.py
```

## 📋 Erste Verwendung

1. **Starte die App** (Tray-Icon erscheint rechts unten)
2. **Rechtsklick** auf das Icon
3. Wähle **"OpenHardwareMonitor Pfad definieren"**
4. Navigiere zu deiner `OpenHardwareMonitor.exe` und wähle sie aus
5. Rechtsklick → **"Start"**
6. ✅ Icon wird grün → Monitoring läuft!

## ⚙️ Konfiguration

Die Einstellungen werden automatisch in `pc_monitor_config.json` gespeichert:

```json
{
    "ohm_path": "C:\\Program Files\\OpenHardwareMonitor\\OpenHardwareMonitor.exe",
    "auto_start": false
}
```

## 🔄 Autostart mit Windows

Um die App beim Windows-Start automatisch zu starten:

1. Drücke `Win + R`
2. Tippe `shell:startup` und drücke Enter
3. Erstelle eine Verknüpfung zur `PC Monitor Tray.exe` in diesem Ordner
4. Fertig! Die App startet ab jetzt automatisch

## 📁 Projektstruktur

```
pc-monitor-poc/
├── pc_monitor_tray.py          # Hauptanwendung (System Tray)
├── python/
│   └── pc_monitor_bidirectional.pyw  # Monitoring-Script (läuft ohne Konsole)
├── start_pc_monitor.bat        # Batch-Script zum Starten
├── build_exe.bat               # EXE-Builder
├── create_icon.py              # Icon-Generator
├── requirements_tray.txt       # Python-Dependencies
├── pc_monitor_config.json      # Konfiguration (wird automatisch erstellt)
└── dist/
    └── PC Monitor Tray.exe     # Fertige EXE (nach build_exe.bat)
```

## 🛠️ Technische Details

### Änderungen an bestehenden Dateien

1. **pc_monitor_bidirectional.py → .pyw**
   - Läuft nun ohne Konsolen-Fenster

2. **start_pc_monitor.bat**
   - Verwendet `pythonw` statt `python`
   - Referenziert `.pyw` Datei

3. **start_pc_monitor.ps1**
   - Verwendet `pythonw` statt `python`
   - Referenziert `.pyw` Datei

### Dependencies

- `pystray` - System Tray Icon
- `Pillow` - Icon-Erstellung
- `tkinter` - Datei-Dialoge (in Python enthalten)
- `psutil`, `pyserial`, `gputil`, `wmi` - PC-Monitoring

### Plattform

- **Windows only** (verwendet Windows-spezifische Features)
- Python 3.7+

## 🐛 Troubleshooting

### Problem: "PC Monitor läuft bereits"
**Lösung**: Rechtsklick → "Stop" → dann "Start"

### Problem: "Konnte nicht starten"
**Mögliche Ursachen**:
- OpenHardwareMonitor-Pfad nicht konfiguriert
- Python nicht installiert
- ESP32 nicht verbunden

**Lösung**:
1. Prüfe Pfad: Rechtsklick → "OpenHardwareMonitor Pfad definieren"
2. Prüfe ob Python installiert ist: `python --version`
3. Prüfe ESP32-Verbindung

### Problem: Icon nicht sichtbar
**Lösung**:
- Klicke auf "^" in der Taskleiste (versteckte Icons)
- Eventuell Taskleisten-Einstellungen anpassen

### Problem: EXE-Build schlägt fehl
**Lösung**:
```bash
# Installiere PyInstaller
pip install pyinstaller

# Installiere Dependencies
pip install -r requirements_tray.txt

# Nochmal versuchen
build_exe.bat
```

## 📝 Weitere Informationen

- Detaillierte Anleitung: [ANLEITUNG_TRAY_APP.md](ANLEITUNG_TRAY_APP.md)
- Autostart-Anleitung: [AUTOSTART_ANLEITUNG.md](AUTOSTART_ANLEITUNG.md)

## 💡 Zukünftige Features (optional)

Mögliche Erweiterungen:
- [ ] Notifications beim Start/Stop
- [ ] Live-Status im Tooltip (CPU, GPU, etc.)
- [ ] Log-Viewer im Menü
- [ ] Auto-Start Toggle im Menü
- [ ] Mehrere Profile (verschiedene ESP32-Ports)

## 📄 Lizenz

Dieses Projekt ist für deinen persönlichen Gebrauch.
