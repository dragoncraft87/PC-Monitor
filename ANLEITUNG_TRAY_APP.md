# PC Monitor Tray - Anleitung

## Features

Die System-Tray-Anwendung bietet:

- **Minimierter Start**: Startet als Icon in der Taskleiste (rechts unten)
- **Rechtsklick-Menü** mit folgenden Optionen:
  - **OpenHardwareMonitor Pfad definieren**: Datei-Browser zum Auswählen der OpenHardwareMonitor.exe
  - **Start**: Startet OpenHardwareMonitor + PC Monitor Script
  - **Stop**: Stoppt das PC Monitor Script
  - **Alles beenden**: Beendet die Tray-App und das Monitoring

- **Icon-Status**:
  - 🟢 Grün = PC Monitor läuft
  - 🔴 Rot = PC Monitor gestoppt

## Installation & Verwendung

### Option 1: Python direkt ausführen

```bash
# Dependencies installieren
pip install -r requirements_tray.txt

# Tray-App starten
python pc_monitor_tray.py
```

### Option 2: EXE erstellen (empfohlen)

```bash
# EXE bauen
build_exe.bat

# Die EXE befindet sich dann in: dist\PC Monitor Tray.exe
```

Die EXE kann dann:
- Beliebig kopiert werden (standalone)
- In den Autostart-Ordner gelegt werden für automatischen Start
- Von überall ausgeführt werden

## Erste Schritte

1. **Starte die Tray-App** (entweder Python-Script oder EXE)
2. **Rechtsklick auf Icon** (in Taskleiste rechts unten)
3. **"OpenHardwareMonitor Pfad definieren"** → Navigiere zur `OpenHardwareMonitor.exe`
4. **"Start"** → Startet das Monitoring
5. Icon wird **grün** ✓

## Autostart einrichten

Um die App automatisch mit Windows zu starten:

1. Drücke `Win + R`
2. Tippe `shell:startup` und drücke Enter
3. Kopiere die `PC Monitor Tray.exe` (oder eine Verknüpfung) in diesen Ordner
4. Beim nächsten Windows-Start startet die App automatisch

## Konfiguration

Die Einstellungen werden in `pc_monitor_config.json` gespeichert:

```json
{
    "ohm_path": "C:\\Program Files\\OpenHardwareMonitor\\OpenHardwareMonitor.exe",
    "auto_start": false
}
```

Du kannst diese Datei auch manuell bearbeiten.

## Troubleshooting

### "PC Monitor läuft bereits"
- Das Script läuft schon → verwende "Stop" um es zu beenden

### "Konnte nicht starten"
- Prüfe ob der OpenHardwareMonitor-Pfad korrekt ist
- Prüfe ob Python installiert ist (für .pyw Script)
- Prüfe ob ESP32 angeschlossen ist

### Icon erscheint nicht
- Prüfe System Tray (rechts unten in Taskleiste)
- Eventuell auf "^" klicken um versteckte Icons zu sehen

## Technische Details

- **Python-Version**: 3.7+
- **Dependencies**: pystray, Pillow, tkinter
- **Plattform**: Windows (verwendet tkinter für Dialoge, subprocess für Batch-Scripts)

## Änderungen an ursprünglichen Dateien

- `pc_monitor_bidirectional.py` → `pc_monitor_bidirectional.pyw` (startet ohne Konsole)
- `start_pc_monitor.bat` → verwendet nun `pythonw` statt `python`
- `start_pc_monitor.ps1` → verwendet nun `pythonw` statt `python`

## Weitere Features (optional)

Du könntest noch hinzufügen:
- Auto-Start beim Windows-Start (auto_start flag)
- Notifications beim Start/Stop
- Status-Anzeige im Tooltip
- Logs anzeigen via Menü
