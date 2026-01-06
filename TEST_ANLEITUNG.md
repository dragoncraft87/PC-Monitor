# 🧪 Test-Anleitung für PC Monitor Tray App

## Schritt-für-Schritt Test

### 1️⃣ Vorbereitung

```bash
# Wechsle ins Projektverzeichnis
cd c:\Users\richa\Desktop\pc-monitor-poc

# Installiere Dependencies
pip install -r requirements_tray.txt
```

**Erwartetes Ergebnis**: Alle Pakete werden erfolgreich installiert

---

### 2️⃣ Test: Python-Script direkt

```bash
# Starte die Tray-App
python pc_monitor_tray.py
```

**Erwartetes Ergebnis**:
- ✅ Ein rotes Icon erscheint in der Taskleiste (rechts unten)
- ✅ Rechtsklick öffnet ein Menü mit 5 Einträgen
- ✅ Das Programm läuft ohne Fehler

**Test das Menü**:
1. Rechtsklick auf Icon
2. Klicke "OpenHardwareMonitor Pfad definieren"
3. Wähle deine OpenHardwareMonitor.exe aus
4. ✅ Erfolgsmeldung erscheint

**Stoppe die App**: Rechtsklick → "Alles beenden"

---

### 3️⃣ Test: Start/Stop Funktionalität

```bash
# Starte die App erneut
python pc_monitor_tray.py
```

**Test Start**:
1. Rechtsklick → "Start"
2. ✅ Icon wird **grün**
3. ✅ "PC Monitor gestartet!" Meldung
4. ✅ OpenHardwareMonitor startet (falls Pfad korrekt)
5. ✅ PC Monitor Script läuft (prüfe Task-Manager: pythonw.exe)

**Test Stop**:
1. Rechtsklick → "Stop"
2. ✅ Icon wird **rot**
3. ✅ "PC Monitor gestoppt!" Meldung
4. ✅ pythonw.exe Prozess beendet

**Stoppe die App**: Rechtsklick → "Alles beenden"

---

### 4️⃣ Test: Konfigurationsdatei

```bash
# Prüfe ob Config erstellt wurde
type pc_monitor_config.json
```

**Erwartete Ausgabe**:
```json
{
    "ohm_path": "C:\\Program Files\\OpenHardwareMonitor\\OpenHardwareMonitor.exe",
    "auto_start": false
}
```

---

### 5️⃣ Test: EXE-Erstellung

```bash
# Baue die EXE
build_exe.bat
```

**Erwartetes Ergebnis**:
- ✅ Dependencies werden installiert
- ✅ icon.ico wird erstellt (falls nicht vorhanden)
- ✅ PyInstaller läuft durch
- ✅ "BUILD ERFOLGREICH!" Meldung
- ✅ `dist\PC Monitor Tray.exe` existiert

**Prüfe die EXE-Größe**:
```bash
dir dist\
```
Erwartete Größe: ~15-25 MB (standalone mit allen Dependencies)

---

### 6️⃣ Test: EXE ausführen

```bash
# Starte die EXE
.\dist\PC Monitor Tray.exe
```

**Erwartetes Ergebnis**:
- ✅ Icon erscheint in Taskleiste
- ✅ Gleiche Funktionalität wie Python-Script
- ✅ Keine Python-Installation erforderlich (standalone)

**Vollständiger Funktionstest**:
1. Rechtsklick → "Start" → Icon wird grün ✅
2. Rechtsklick → "Stop" → Icon wird rot ✅
3. Rechtsklick → "Alles beenden" → App schließt ✅

---

### 7️⃣ Test: .pyw Datei

```bash
# Prüfe ob .pyw existiert
dir python\*.pyw
```

**Erwartetes Ergebnis**:
```
pc_monitor_bidirectional.pyw
```

**Test direkt**:
```bash
# Starte .pyw (sollte OHNE Konsole starten)
pythonw python\pc_monitor_bidirectional.pyw
```

**Erwartetes Ergebnis**:
- ✅ Kein Konsolen-Fenster öffnet sich
- ✅ Prozess läuft im Hintergrund (prüfe Task-Manager)
- ✅ ESP32 sollte Daten empfangen (falls angeschlossen)

**Stoppe**:
- Task-Manager → pythonw.exe → Task beenden

---

### 8️⃣ Test: Batch-Script

```bash
# Teste das aktualisierte Batch-Script
start_pc_monitor.bat
```

**Erwartetes Ergebnis**:
- ✅ OpenHardwareMonitor startet (falls gefunden)
- ✅ PC Monitor Script startet (.pyw mit pythonw)
- ✅ Konsole zeigt Status-Meldungen

**Prüfe Task-Manager**:
- OpenHardwareMonitor.exe ✅
- pythonw.exe (pc_monitor_bidirectional.pyw) ✅

Stoppe mit `Strg+C`

---

## 🎯 Vollständiger Integrations-Test

### Szenario: Kompletter Workflow

1. **Starte Tray-App**: `.\dist\PC Monitor Tray.exe`
2. **Konfiguriere OHM-Pfad**: Rechtsklick → "OpenHardwareMonitor Pfad definieren"
3. **Starte Monitoring**: Rechtsklick → "Start"
4. **Prüfe Task-Manager**:
   - PC Monitor Tray.exe ✅
   - OpenHardwareMonitor.exe ✅
   - pythonw.exe ✅
5. **Prüfe ESP32**: Sollte Daten empfangen und anzeigen ✅
6. **Stoppe Monitoring**: Rechtsklick → "Stop"
7. **Prüfe Task-Manager**: Nur PC Monitor Tray.exe läuft ✅
8. **Beende alles**: Rechtsklick → "Alles beenden"
9. **Prüfe Task-Manager**: Alle Prozesse beendet ✅

---

## ✅ Checkliste

Nach dem Test sollten alle diese Punkte erfüllt sein:

- [ ] `pc_monitor_tray.py` startet ohne Fehler
- [ ] Icon erscheint in der Taskleiste
- [ ] Rechtsklick-Menü funktioniert
- [ ] Pfad-Konfiguration funktioniert (File-Browser)
- [ ] Start-Funktion funktioniert (Icon → grün)
- [ ] Stop-Funktion funktioniert (Icon → rot)
- [ ] `pc_monitor_config.json` wird erstellt
- [ ] `pc_monitor_bidirectional.pyw` existiert
- [ ] `start_pc_monitor.bat` verwendet `.pyw` und `pythonw`
- [ ] `start_pc_monitor.ps1` verwendet `.pyw` und `pythonw`
- [ ] `build_exe.bat` läuft ohne Fehler
- [ ] `dist\PC Monitor Tray.exe` wird erstellt
- [ ] EXE ist standalone (läuft ohne Python)
- [ ] EXE funktioniert identisch zum Python-Script

---

## 🐛 Bei Problemen

### Python-Script startet nicht
```bash
# Prüfe Python-Version
python --version

# Installiere Dependencies nochmal
pip install -r requirements_tray.txt
```

### Icon erscheint nicht
- Prüfe Taskleiste → "^" (versteckte Icons)
- Prüfe ob Fehler in der Konsole erscheinen

### EXE-Build schlägt fehl
```bash
# Installiere PyInstaller
pip install pyinstaller

# Clean Build
rmdir /s /q build dist
rmdir /s /q "PC Monitor Tray.spec"
build_exe.bat
```

### Start-Funktion funktioniert nicht
- Prüfe ob `start_pc_monitor.bat` existiert
- Prüfe ob `python\pc_monitor_bidirectional.pyw` existiert
- Prüfe Pfad in `pc_monitor_config.json`

---

## 📊 Erwartete Prozesse im Task-Manager

**Wenn Monitoring läuft**:
| Prozess | Beschreibung |
|---------|--------------|
| `PC Monitor Tray.exe` | Tray-Anwendung |
| `OpenHardwareMonitor.exe` | Hardware-Monitoring |
| `pythonw.exe` | PC Monitor Script (.pyw) |

**Wenn gestoppt**:
| Prozess | Beschreibung |
|---------|--------------|
| `PC Monitor Tray.exe` | Nur Tray-Anwendung |

---

## ✨ Nächste Schritte

Nach erfolgreichem Test:

1. **Verteilen**: Kopiere `dist\PC Monitor Tray.exe` wohin du willst
2. **Autostart**: Erstelle Verknüpfung in `shell:startup`
3. **Genießen**: Starte mit einem Klick dein komplettes Monitoring-Setup!

---

**Viel Erfolg beim Testen!** 🚀
