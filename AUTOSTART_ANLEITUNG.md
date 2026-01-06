# PC Monitor - Autostart Einrichtung 🚀

## Schnellstart

Du hast jetzt **2 Start-Scripts** zur Auswahl:

1. **`start_pc_monitor.bat`** - Einfache Batch-Datei (doppelklick funktioniert immer)
2. **`start_pc_monitor.ps1`** - PowerShell-Version (eleganter, bessere Fehlerbehandlung)

### Test: Sofort starten

**Option A: Batch-Script (empfohlen für Anfänger)**
```
Doppelklick auf: start_pc_monitor.bat
```

**Option B: PowerShell-Script**
```
Rechtsklick auf start_pc_monitor.ps1 → "Mit PowerShell ausführen"
```

---

## Autostart einrichten (Windows 11/10)

### Methode 1: Über Autostart-Ordner (EINFACHSTE)

1. **Drücke:** `Win + R`
2. **Tippe:** `shell:startup` → Enter
3. **Es öffnet sich der Autostart-Ordner**
4. **Erstelle eine Verknüpfung:**
   - Rechtsklick im Ordner → Neu → Verknüpfung
   - Pfad eingeben:
     ```
     C:\Users\richa\Desktop\pc-monitor-poc\start_pc_monitor.bat
     ```
   - Name: "PC Monitor" (oder was du willst)
   - Fertigstellen

5. **Fertig!** Beim nächsten Windows-Start läuft alles automatisch.

---

### Methode 2: Über Task Scheduler (FORTGESCHRITTEN)

**Vorteile:**
- Script läuft unsichtbar im Hintergrund
- Startet automatisch, auch bei verzögertem Login
- Kann mit Admin-Rechten laufen (falls nötig)

**Schritte:**
1. Drücke `Win + R` → tippe `taskschd.msc` → Enter
2. Rechts: "Einfache Aufgabe erstellen"
3. Name: `PC Monitor Autostart`
4. Trigger: `Bei Anmeldung`
5. Aktion: `Programm starten`
6. Programm/Skript:
   ```
   powershell.exe
   ```
7. Argumente hinzufügen:
   ```
   -WindowStyle Hidden -File "C:\Users\richa\Desktop\pc-monitor-poc\start_pc_monitor.ps1"
   ```
8. Fertigstellen
9. **Wichtig:** Doppelklick auf die neue Aufgabe → Haken bei "Mit höchsten Privilegien ausführen" (falls Open Hardware Monitor Admin braucht)

---

## Open Hardware Monitor Pfad anpassen

Falls Open Hardware Monitor **nicht** hier installiert ist:
```
C:\Program Files\OpenHardwareMonitor\OpenHardwareMonitor.exe
```

**Dann:**
1. Öffne `start_pc_monitor.bat` oder `start_pc_monitor.ps1` mit einem Texteditor
2. Ändere Zeile 12 bzw. 11:
   ```
   set "OHM_PATH=C:\Dein\Eigener\Pfad\OpenHardwareMonitor.exe"
   ```
3. Speichern

---

## Troubleshooting

### ❌ "Open Hardware Monitor nicht gefunden"
→ Passe den Pfad im Script an (siehe oben)

### ❌ "Python wurde nicht gefunden"
→ Installiere Python: https://www.python.org/downloads/
→ Haken bei "Add Python to PATH" setzen!

### ❌ PowerShell Script wird blockiert
→ PowerShell als Admin öffnen:
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### ❌ ESP32 wird nicht erkannt
→ Prüfe ob ESP32 mit dem richtigen USB-Kabel verbunden ist (USB-Port, nicht UART)
→ VS Code Monitor muss geschlossen sein (sonst blockiert es COM4)

---

## Was passiert beim Start?

1. ✅ Open Hardware Monitor wird gestartet (minimiert im Hintergrund)
2. ✅ Python-Script verbindet sich mit ESP32 (COM4)
3. ✅ Daten werden alle 1 Sekunde gesendet
4. ✅ Displays zeigen Live-Daten an

---

## Beenden

**Strg + C** im PowerShell/CMD-Fenster

**WICHTIG:**
- Das Python-Script wird beendet
- Open Hardware Monitor läuft **weiter** im Hintergrund
- Falls du OHM auch beenden willst: Task-Manager → OpenHardwareMonitor.exe beenden

---

## Optional: Minimized Start

Falls du möchtest, dass das Fenster **sofort minimiert** wird:

**Variante 1: NirCmd (kostenlos)**
1. Download: https://www.nirsoft.net/utils/nircmd.html
2. Erstelle neue Verknüpfung:
   ```
   C:\path\to\nircmd.exe exec hide "C:\Users\richa\Desktop\pc-monitor-poc\start_pc_monitor.bat"
   ```

**Variante 2: VBS-Script**
Erstelle `start_pc_monitor_hidden.vbs`:
```vbs
Set WshShell = CreateObject("WScript.Shell")
WshShell.Run chr(34) & "C:\Users\richa\Desktop\pc-monitor-poc\start_pc_monitor.bat" & chr(34), 0
Set WshShell = Nothing
```
→ Dieses VBS-Script in den Autostart legen

---

## Viel Spaß mit deinem PC Monitor! 🎉
