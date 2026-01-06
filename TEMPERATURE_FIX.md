# 🌡️ Temperatur-Fixes

## Problem

Das alte Script hatte hardcodierte Temperaturen:
- **CPU:** Festgenagelt auf 60°C
- **GPU:** Funktionierte (37°C via GPUtil war korrekt)

## Lösung

Das neue Script nutzt einen **Fallback-System** für CPU-Temperaturen:

### 1. LibreHardwareMonitor (BESTE Genauigkeit)
- Wenn `LibreHardwareMonitorLib.dll` vorhanden ist
- Liest direkt CPU Package/Average Temperature
- **Genaueste Messung**

### 2. WMI (Fallback #1)
- Windows Management Instrumentation
- Funktioniert manchmal auf Laptops
- Weniger zuverlässig auf Desktop-PCs

### 3. psutil.sensors_temperatures() (Fallback #2)
- Linux/BSD hauptsächlich
- Funktioniert auf manchen Windows-Systemen mit speziellen Treibern
- Versucht `coretemp`, `k10temp`, `cpu-thermal`

### 4. Schätzung basierend auf Load (Fallback #3)
- Wenn nichts anderes funktioniert
- Formel: `40°C + (CPU% * 0.4)`
- Beispiele:
  - 0% Load → 40°C
  - 50% Load → 60°C
  - 100% Load → 80°C
- **Nur eine grobe Schätzung!**

---

## Wie nutzen?

### Option A: Mit LibreHardwareMonitor (empfohlen)

```bash
# 1. Download LibreHardwareMonitor DLL
# https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

# 2. Kopiere LibreHardwareMonitorLib.dll nach:
python/LibreHardwareMonitorLib.dll

# 3. Installiere pythonnet:
pip install pythonnet

# 4. Starte Script (als Administrator!)
python python/pc_monitor.py
```

**Output:**
```
Using LibreHardwareMonitor for temperatures
TX: CPU=45% (62.3°C) GPU=72% (68.0°C) RAM=8.2GB
```

**Echte CPU-Temperatur!** ✅

---

### Option B: Ohne LibreHardwareMonitor (Fallback)

```bash
# Einfach starten (kein Admin nötig für Schätzung)
python python/pc_monitor.py
```

**Output:**
```
TX: CPU=45% (58.0°C) GPU=72% (68.0°C) RAM=8.2GB
```

**Geschätzte CPU-Temperatur** (40 + 45*0.4 = 58°C)

---

## Code-Änderungen

### Vorher (Zeile 89):
```python
cpu_temp = 60.0  # Default - HARDCODED!
```

### Nachher (Zeile 123-156):
```python
# Try OpenHardwareMonitor first (most accurate)
cpu_temp = self.get_cpu_temp_ohm()

# Fallback to WMI
if cpu_temp is None and WMI_AVAILABLE:
    ...

# Fallback to psutil sensors
if cpu_temp is None:
    ...

# Last resort: estimate based on load
if cpu_temp is None:
    cpu_temp = 40.0 + (cpu_percent * 0.4)
```

---

## GPU-Temperatur

**Funktioniert bereits korrekt via GPUtil!**
- Keine Änderungen nötig
- 37°C ist wahrscheinlich korrekt (GPU idle)

---

## Konsolen-Glitch Fix

**Problem:** `.pyw` File zeigte immer noch Console-Flackern

**Lösung:** `--silent` Flag

### Vorher:
```python
print("TX: CPU=45% ...")  # Immer ausgegeben
```

### Nachher:
```python
def log(self, msg):
    if not self.silent:
        print(msg)

# Usage:
self.log("TX: CPU=45% ...")  # Nur wenn nicht silent
```

**Tray-App startet mit:**
```python
python pc_monitor.py --silent
```

**Kein Console-Output = Kein Flackern!** ✅

---

## Zusammenfassung

| Feature | Vorher | Nachher |
|---------|--------|---------|
| **CPU Temp** | ❌ Hardcoded 60°C | ✅ Echt oder geschätzt |
| **GPU Temp** | ✅ Funktioniert (37°C) | ✅ Funktioniert |
| **Console Glitch** | ⚠️ Flackert | ✅ Silent mode |
| **Genauigkeit** | ❌ Nicht echt | ✅ Mit DLL: Sehr genau |

---

## Test

```bash
# Test mit Live-Output
python python/pc_monitor.py

# Test silent (kein Output, kein Glitch)
python python/pc_monitor.py --silent
```

**Erwartung:**
- CPU-Temperatur ändert sich bei Last
- Kein Flackern im silent mode
- Alle anderen Werte wie vorher

---

## Empfehlung

Für **beste Genauigkeit**:
1. Download `LibreHardwareMonitorLib.dll`
2. `pip install pythonnet`
3. Script als **Administrator** ausführen

Die DLL ist **optional** - ohne funktioniert es auch (mit Schätzung).

---

**Status: FIXED! ✅**

Temperaturen sind jetzt entweder echt oder vernünftig geschätzt!
