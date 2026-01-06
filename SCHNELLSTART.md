# 🚀 SCHNELLSTART - PC Monitor Tray App

## ✅ BUILD ERFOLGREICH!

Die EXE wurde erstellt und ist bereit zur Verwendung:

**Datei**: `dist\PC Monitor Tray.exe`
**Größe**: ~18 MB (standalone, keine Python-Installation nötig)

---

## 🎯 So startest du die App:

### 1. Doppelklick auf die EXE

```
dist\PC Monitor Tray.exe
```

Ein **rotes Icon** erscheint in der Taskleiste (rechts unten bei der Uhr).

---

### 2. Rechtsklick auf das Icon

Du siehst folgendes Menü:
```
┌─────────────────────────────────────────────┐
│ OpenHardwareMonitor Pfad definieren         │
├─────────────────────────────────────────────┤
│ Start                                       │
│ Stop                                        │
├─────────────────────────────────────────────┤
│ Alles beenden                               │
└─────────────────────────────────────────────┘
```

---

### 3. Erstkonfiguration (nur einmal nötig)

1. **Rechtsklick** → **"OpenHardwareMonitor Pfad definieren"**
2. Navigiere zu deiner `OpenHardwareMonitor.exe`
   - Typischer Pfad: `C:\Program Files\OpenHardwareMonitor\OpenHardwareMonitor.exe`
3. Wähle die Datei aus → **"Öffnen"**
4. Erfolgsmeldung erscheint ✅

Der Pfad wird automatisch in `pc_monitor_config.json` gespeichert.

---

### 4. PC Monitor starten

1. **Rechtsklick** → **"Start"**
2. Icon wird **GRÜN** 🟢
3. Erfolgsmeldung: "PC Monitor gestartet!"

**Was passiert jetzt:**
- OpenHardwareMonitor startet (minimiert)
- PC Monitor Script läuft (ohne sichtbares Fenster)
- ESP32 empfängt Daten (CPU, GPU, RAM, etc.)

---

### 5. PC Monitor stoppen

1. **Rechtsklick** → **"Stop"**
2. Icon wird **ROT** 🔴
3. Erfolgsmeldung: "PC Monitor gestoppt!"

OpenHardwareMonitor läuft weiter im Hintergrund.

---

### 6. App beenden

**Rechtsklick** → **"Alles beenden"**

Stoppt das Monitoring und schließt die Tray-App komplett.

---

## 🔄 Autostart einrichten (optional)

Um die App automatisch mit Windows zu starten:

1. Drücke **Win + R**
2. Tippe `shell:startup` → Enter
3. Kopiere `PC Monitor Tray.exe` in diesen Ordner
   - ODER erstelle eine Verknüpfung zur EXE

Beim nächsten Windows-Start läuft die App automatisch!

---

## 📊 Icon-Status

| Icon-Farbe | Bedeutung |
|------------|-----------|
| 🔴 **ROT** | PC Monitor gestoppt |
| 🟢 **GRÜN** | PC Monitor läuft aktiv |

---

## 📁 Projektstruktur

```
pc-monitor-poc/
├── dist/
│   └── PC Monitor Tray.exe       ← FERTIGE EXE (18 MB)
├── pc_monitor_tray.py            ← Quellcode
├── pc_monitor_config.json        ← Konfiguration (wird automatisch erstellt)
├── python/
│   └── pc_monitor_bidirectional.pyw
├── start_pc_monitor.bat
└── icon.ico
```

---

## ⚙️ Konfigurationsdatei

Nach dem ersten Start wird `pc_monitor_config.json` erstellt:

```json
{
    "ohm_path": "C:\\Program Files\\OpenHardwareMonitor\\OpenHardwareMonitor.exe",
    "auto_start": false
}
```

Du kannst diese Datei auch manuell bearbeiten.

---

## 🎮 Beispiel-Workflow

```
1. Doppelklick: dist\PC Monitor Tray.exe
   → Rotes Icon erscheint in Taskleiste ✅

2. Rechtsklick → "OpenHardwareMonitor Pfad definieren"
   → Pfad zu OpenHardwareMonitor.exe auswählen ✅

3. Rechtsklick → "Start"
   → Icon wird GRÜN 🟢
   → OpenHardwareMonitor startet
   → PC Monitor läuft
   → ESP32 zeigt Daten an ✅

4. Monitoring läuft! ESP32 zeigt live:
   - CPU: 35%
   - GPU: 42%
   - RAM: 8.2 GB
   - Netzwerk: ↓ 2.3 Mbps, ↑ 0.5 Mbps

5. Rechtsklick → "Stop"
   → Icon wird ROT 🔴
   → Monitoring gestoppt ✅

6. Rechtsklick → "Alles beenden"
   → App geschlossen ✅
```

---

## 🐛 Troubleshooting

### Icon erscheint nicht
- Prüfe Taskleiste rechts unten
- Klicke auf "^" (versteckte Icons)

### "Konnte nicht starten"
- Prüfe ob OpenHardwareMonitor-Pfad korrekt ist
- Rechtsklick → "OpenHardwareMonitor Pfad definieren" → neu auswählen

### ESP32 empfängt keine Daten
- Prüfe ob ESP32 per USB verbunden ist
- Prüfe COM-Port im Geräte-Manager
- Prüfe ob icon.ico GRÜN ist (= läuft)

### EXE läuft nicht
- Prüfe ob Antivirus die EXE blockiert
- Eventuell als Administrator ausführen

---

## 📝 Weitere Dokumentation

- **Vollständige Anleitung**: [ANLEITUNG_TRAY_APP.md](ANLEITUNG_TRAY_APP.md)
- **README**: [README_TRAY_APP.md](README_TRAY_APP.md)
- **Test-Guide**: [TEST_ANLEITUNG.md](TEST_ANLEITUNG.md)

---

## 🎉 Fertig!

Die App ist bereit zur Verwendung. Viel Spaß mit deinem PC Monitor! 🚀

**Nächster Schritt**: Doppelklick auf `dist\PC Monitor Tray.exe` und loslegen!
