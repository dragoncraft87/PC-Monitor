# Scarab Monitor

**Desert-Spec Hardware Monitor** for ESP32-S3 with 4 round GC9A01 displays.

Real-time visualization of PC hardware stats: CPU, GPU, RAM, and Network.

## Features

### Visualization
- **CPU Screen**: Load percentage (arc), temperature with color coding
- **GPU Screen**: Load, temperature, VRAM usage
- **RAM Screen**: Used/Total with visual bar
- **Network Screen**: Type (LAN/WLAN), speed, upload/download rates

### Operating Modes

| Mode | Requirements | Features |
|------|--------------|----------|
| **Full Mode** | Admin rights | Ring-0 access via LibreHardwareMonitor, exact CPU/GPU temperatures |
| **Lite Mode** | User rights | WMI/PerformanceCounter fallback, CPU temp shows "N/A" |

### Desert-Spec Stability
- **Handshake Protocol**: `WHO_ARE_YOU?` → `SCARAB_CLIENT_OK`
- **Infinite Reconnect**: Auto-reconnects on disconnect
- **Graceful Shutdown**: Clean exit, no zombie threads
- **Smart Port Discovery**: Skips JTAG/Debug ports automatically
- **Screensaver**: Retro game icons after 30s idle

## Hardware Setup

### Components
- **MCU**: ESP32-S3 (with PSRAM recommended)
- **Displays**: 4x GC9A01 240x240 Round LCD (SPI)

### Pinout (from code)

All displays share the SPI bus on **SPI2_HOST**:

| Signal | GPIO | Description |
|--------|------|-------------|
| **SCK** | 4 | SPI Clock (shared) |
| **MOSI** | 5 | SPI Data (shared) |

Individual display pins:

| Display | CS | DC | RST |
|---------|-----|-----|-----|
| **CPU** | 12 | 11 | 13 |
| **GPU** | 9 | 46 | 10 |
| **RAM** | 8 | 18 | 3 |
| **Network** | 16 | 15 | 17 |

### Wiring Diagram

```
ESP32-S3                    GC9A01 Displays
─────────                   ───────────────
GPIO 4  (SCK)  ─────────┬── SCK (all 4 displays)
GPIO 5  (MOSI) ─────────┴── SDA (all 4 displays)

GPIO 12 (CS)   ───────────── CS  (CPU)
GPIO 11 (DC)   ───────────── DC  (CPU)
GPIO 13 (RST)  ───────────── RST (CPU)

GPIO 9  (CS)   ───────────── CS  (GPU)
GPIO 46 (DC)   ───────────── DC  (GPU)
GPIO 10 (RST)  ───────────── RST (GPU)

GPIO 8  (CS)   ───────────── CS  (RAM)
GPIO 18 (DC)   ───────────── DC  (RAM)
GPIO 3  (RST)  ───────────── RST (RAM)

GPIO 16 (CS)   ───────────── CS  (Network)
GPIO 15 (DC)   ───────────── DC  (Network)
GPIO 17 (RST)  ───────────── RST (Network)

3.3V ──────────┬── VCC (all displays)
GND  ──────────┴── GND (all displays)
```

## Installation (Windows Client)

### Portable Installation

1. Download the latest release ZIP
2. Extract to desired location (e.g., `C:\Tools\ScarabMonitor\`)
3. Run `install_autostart.ps1` (Right-click → "Run with PowerShell")
4. Choose installation mode:
   - **[1] Full Mode**: Task Scheduler with Admin (recommended)
   - **[2] Lite Mode**: Registry Run without Admin

### Manual Start

```powershell
# Full Mode (as Administrator)
.\PCMonitorClient.exe

# Lite Mode (as User)
.\PCMonitorClient.exe
```

### Tray Icon Status

| Color | Meaning |
|-------|---------|
| **Red (blinking)** | Initializing / Searching |
| **Red (solid)** | Disconnected |
| **Yellow** | Connected (Lite Mode) |
| **Green** | Connected (Full Mode) |

## Development

### Windows Client

**Requirements:**
- Visual Studio 2022 or VS Code
- .NET Framework 4.7.2 SDK

**Build:**
```powershell
cd PCMonitorClient
dotnet build -c Release
```

**Output:** `PCMonitorClient\bin\Release\net472\`

### ESP32 Firmware

**Requirements:**
- ESP-IDF 5.x
- LVGL 9.x (via managed components)

**Build & Flash:**
```bash
idf.py build
idf.py -p COM3 flash monitor
```

## Project Structure

```
scarab-monitor/
├── main/                      # ESP32 Firmware
│   ├── main_lvgl.c           # Main application
│   ├── lvgl_gc9a01_driver.*  # Display driver
│   ├── screens/              # LVGL screen implementations
│   └── images/               # Screensaver assets
├── PCMonitorClient/          # Windows Tray Client
│   └── PCMonitorClient/
│       ├── Program.cs        # Main + TrayContext
│       ├── HardwareCollector.cs
│       ├── StatusForm.cs
│       └── install_autostart.ps1
├── CMakeLists.txt            # ESP-IDF build
├── lv_conf.h                 # LVGL configuration
└── sdkconfig                 # ESP-IDF settings
```

## Serial Protocol

Data format sent from PC to ESP32 (115200 baud):

```
CPU:<load>,CPUT:<temp>,GPU:<load>,GPUT:<temp>,VRAM:<used>/<total>,RAM:<used>/<total>,NET:<type>,SPEED:<speed>,DOWN:<mbps>,UP:<mbps>\n
```

Example:
```
CPU:45,CPUT:62.5,GPU:30,GPUT:55.0,VRAM:4.2/12.0,RAM:16.5/32.0,NET:LAN,SPEED:1000 Mbps,DOWN:125.5,UP:10.2
```

Special values:
- `-1` = Sensor error / Not available (displays "N/A")

## License

MIT License - See LICENSE file for details.

---

*Desert-Spec Edition - Built for reliability in harsh conditions.*
