# Scarab Monitor

**High-Performance Hardware Monitor (Desert-Spec Edition)**

Real-time visualization of PC hardware metrics on 4 round GC9A01 displays, powered by ESP32-S3.

---

## Why "Desert-Spec"?

This isn't marketing—it's engineering philosophy. Every feature exists because production environments demand it.

| Feature | Implementation | Why It Matters |
|---------|----------------|----------------|
| **Self-Healing Firmware** | ESP32 Task Watchdog Timer (5s hard-reset on freeze) + Mutex-locking for thread-safety | The system recovers from any firmware hang without user intervention |
| **Fail-Safe Client** | Automatic fallback to "Lite Mode" (WMI) when admin drivers are blocked | Works in corporate environments with restricted driver policies |
| **Zero-Config** | Automatic handshake + COM port scanning | No manual port selection, no config files, just plug and play |
| **Data Integrity** | CRC32 checksums for identity data and image uploads | Corrupt packets are detected and rejected |

---

## Features

### Display Screens

- **CPU**: Load percentage (arc gauge), temperature with color coding
- **GPU**: Load, temperature, VRAM usage
- **RAM**: Used/Total with visual progress bar
- **Network**: Connection type (LAN/WLAN), link speed, live upload/download rates

### Operating Modes

| Mode | Requirements | Capabilities |
|------|--------------|--------------|
| **Full Mode** | Admin rights | Ring-0 hardware access via LibreHardwareMonitor, accurate CPU/GPU temperatures |
| **Lite Mode** | Standard user | WMI/PerformanceCounter fallback, CPU temp displays "N/A" |

### Reliability Features

- **Infinite Reconnect**: Auto-reconnects on USB disconnect
- **Graceful Shutdown**: Clean thread termination, no zombie processes
- **Smart Port Discovery**: Automatically skips JTAG/Debug COM ports
- **Screensaver**: Retro game icons after 30s idle

---

## Communication Protocol

### Physical Layer

| Parameter | Value |
|-----------|-------|
| Interface | USB Serial (UART) |
| Baud Rate | 115200 |
| Data Bits | 8 |
| Parity | None |
| Stop Bits | 1 |

### Handshake Sequence

```
PC  → ESP32:  WHO_ARE_YOU?
ESP32 → PC:   SCARAB_CLIENT_OK
```

The client scans all COM ports, sends `WHO_ARE_YOU?`, and waits for the correct response. No manual port configuration required.

### Data Format

ASCII-based, newline-terminated (`\n`):

```
CPU:<load>,CPUT:<temp>,GPU:<load>,GPUT:<temp>,VRAM:<used>/<total>,RAM:<used>/<total>,NET:<type>,SPEED:<speed>,DOWN:<mbps>,UP:<mbps>\n
```

**Example:**
```
CPU:45,CPUT:62.5,GPU:30,GPUT:55.0,VRAM:4.2/12.0,RAM:16.5/32.0,NET:LAN,SPEED:1000 Mbps,DOWN:125.5,UP:10.2
```

**Special Values:**
- `-1` = Sensor unavailable (displays "N/A" on screen)

---

## Hardware

### Components

| Component | Specification |
|-----------|---------------|
| MCU | ESP32-S3-DevKitC (8MB PSRAM required) |
| Displays | 4x GC9A01 240x240 Round LCD (SPI) |
| Connection | USB-C to PC |

### Pin Assignment

All displays share the SPI bus:

| Signal | GPIO |
|--------|------|
| SCK | 4 |
| MOSI | 5 |

Individual control pins:

| Display | CS | DC | RST |
|---------|-----|-----|-----|
| CPU | 12 | 11 | 13 |
| GPU | 9 | 46 | 10 |
| RAM | 8 | 18 | 3 |
| Network | 16 | 15 | 17 |

For detailed wiring instructions, see [docs/HARDWARE.md](docs/HARDWARE.md).

---

## Installation

### Windows Client

1. Go to the **Releases** section on the right sidebar
2. Download the latest `Scarab_Monitor_vX.X.zip`
3. Extract to desired location (e.g., `C:\Tools\ScarabMonitor\`)
4. Run `install_autostart.ps1` (Right-click → "Run with PowerShell")
5. Choose installation mode:
   - **[1] Full Mode**: Task Scheduler with Admin (recommended)
   - **[2] Lite Mode**: Registry Run without Admin

### Manual Start

```powershell
# Run from extracted directory
.\PCMonitorClient.exe
```

### Tray Icon Status

| Color | Meaning |
|-------|---------|
| Red (blinking) | Initializing / Scanning ports |
| Red (solid) | Disconnected |
| Yellow | Connected (Lite Mode) |
| Green | Connected (Full Mode) |

---

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
- LVGL 9.x (managed via idf_component.yml)

**Build & Flash:**
```bash
idf.py build
idf.py -p COM3 flash monitor
```

---

## Project Structure

```
scarab-monitor/
├── main/                      # ESP32 Firmware
│   ├── main_lvgl.c           # Application entry point
│   ├── lvgl_gc9a01_driver.*  # Display driver
│   ├── screens/              # LVGL screen implementations
│   └── images/               # Screensaver assets
├── PCMonitorClient/          # Windows Tray Client
│   └── PCMonitorClient/
│       ├── Program.cs        # Main + TrayContext
│       ├── HardwareCollector.cs
│       ├── StatusForm.cs
│       └── install_autostart.ps1
├── docs/                     # Documentation
│   └── HARDWARE.md          # Assembly guide
├── CMakeLists.txt           # ESP-IDF build config
├── lv_conf.h                # LVGL configuration
└── sdkconfig                # ESP-IDF settings
```

---

## License

MIT License - See [LICENSE](LICENSE) for details.

---

*Scarab Monitor - Desert-Spec Edition*
