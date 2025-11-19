# ⚡ QUICK START

## Schritt 1: Hardware verkabeln (5 Minuten)

Siehe `WIRING.md` - Alle Displays an ESP32-S3:

```
Alle 4 Displays teilen: SCK (GPIO 18), MOSI (GPIO 23), VCC, GND
Jedes Display eigene:   CS, DC, RST Pins (siehe WIRING.md)
```

---

## Schritt 2: ESP32 flashen (2 Minuten)

```bash
cd pc-monitor-4displays

# Build & Flash
idf.py build flash

# Monitor
idf.py monitor
```

**Erwarte:** "All displays initialized!" im Log

---

## Schritt 3: Python starten (1 Minute)

```bash
cd python

# Install
pip install -r requirements.txt

# Run
python pc_monitor.py
```

**Erwarte:** "✅ Connected!" und Daten-Übertragung

---

## ✅ Fertig!

Jetzt sollten alle 4 Displays deine PC-Stats anzeigen! 🎉

---

## 🐛 Probleme?

### ESP32 nicht gefunden?
```bash
# Linux/Mac
ls /dev/tty*

# Windows: Geräte-Manager → Anschlüsse (COM & LPT)
```

### Displays zeigen nichts?
1. Checke Verkabelung (WIRING.md)
2. Prüfe 3.3V & GND
3. Monitor mit: `idf.py monitor`

### Python findet ESP32 nicht?
- USB-Kabel wechseln
- Anderer USB-Port
- CP210x Treiber installieren

---

**Viel Erfolg! 🚀**
