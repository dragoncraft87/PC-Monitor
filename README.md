# 🖥️ PC Monitor - 4x Display System

A real-time PC monitoring system with 4 round GC9A01 displays showing CPU, GPU, RAM, and Network stats. Powered by ESP32-S3 and LVGL.

![PC Monitor Setup](docs/images/setup.jpg)
*Photo placeholder - add your build here!*

## ✨ Features

### Hardware
- **4x GC9A01 Round Displays** (240x240px each)
- **ESP32-S3-DevKitC N16R8** (16MB Flash, 8MB PSRAM)
- **Real-time Updates** (1 second refresh rate)
- **USB Powered** (no external PSU needed)

### Software
- **Windows System Tray Manager** - Start/stop monitoring with one click
- **LibreHardwareMonitor Integration** - Accurate sensor readings without external dependencies
- **Auto-detection** - Finds ESP32 automatically
- **Autostart Support** - Add to Windows startup easily

## 🎯 What It Shows

| Display | Info Displayed |
|---------|---------------|
| **Display 1** | CPU Usage (%) + Temperature (°C) |
| **Display 2** | GPU Usage (%) + Temperature + VRAM |
| **Display 3** | RAM Usage (Used / Total GB) |
| **Display 4** | Network Traffic (Download / Upload MB/s) |

## 📸 Screenshots

*Add your screenshots here!*

```
[CPU Display]  [GPU Display]  [RAM Display]  [Network Display]
     45%            72%          8.2/16 GB      ↓2.3 ↑0.5 MB/s
    62.5°C         68.3°C
```

## 🚀 Quick Start

### For End Users (Just Want to Use It)

1. **Download** the latest `PC Monitor Manager.exe` from releases
2. **Download** `LibreHardwareMonitorLib.dll` from [LibreHardwareMonitor releases](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases)
3. **Place DLL** in the same folder as `python/pc_monitor.py`
4. **Run** `PC Monitor Manager.exe` as Administrator
5. **Right-click** the tray icon → "Start Monitoring"
6. Done! Your displays should light up

### For Developers (Want to Build It)

See **[docs/SOFTWARE.md](docs/SOFTWARE.md)** for detailed installation and build instructions.

## 📦 What's Included

```
pc-monitor-poc/
├── python/
│   ├── pc_monitor.py                # Main monitoring script
│   └── LibreHardwareMonitorLib.dll  # (Download separately)
├── main/
│   ├── main_lvgl.c                  # ESP32 firmware
│   └── lv_conf.h                    # LVGL configuration
├── docs/
│   ├── SOFTWARE.md                  # Software setup guide
│   └── HARDWARE.md                  # Hardware build guide
├── pc_monitor_tray.py               # System tray manager
├── PC Monitor Manager.spec          # PyInstaller build spec
├── requirements.txt                 # Python dependencies
└── README.md                        # This file
```

## 🛠️ Building From Source

### Step 1: Hardware Assembly

Follow **[docs/HARDWARE.md](docs/HARDWARE.md)** for:
- Wiring diagrams
- Pin connections
- Power requirements
- Assembly tips

### Step 2: Flash ESP32 Firmware

```bash
# Install ESP-IDF (if not already installed)
# See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/

# Navigate to project
cd pc-monitor-poc

# Configure and build
idf.py build

# Flash to ESP32
idf.py -p COM3 flash monitor
```

### Step 3: Install Windows Manager

See **[docs/SOFTWARE.md](docs/SOFTWARE.md)** for:
- Python environment setup
- Downloading LibreHardwareMonitor DLL
- Building the EXE
- Installation instructions

## 📋 Requirements

### Hardware
- ESP32-S3-DevKitC N16R8 (with 8MB PSRAM!)
- 4x GC9A01 240x240 Round Displays
- USB-C cable
- Breadboard + jumper wires (or custom PCB)

### Software
- **ESP32**: ESP-IDF v5.0+
- **Windows**: Windows 10/11 (64-bit) with Administrator rights
- **Python**: 3.8+ (for development only)

## 🎮 Usage

### System Tray Manager

Right-click the tray icon for options:

```
┌─────────────────────────────┐
│ Start Monitoring           │ ← Start sending data to ESP32
│ Stop Monitoring            │ ← Stop monitoring
├─────────────────────────────┤
│ Add to Autostart           │ ← Run at Windows startup
│ Remove from Autostart      │
├─────────────────────────────┤
│ Quit                       │ ← Close manager
└─────────────────────────────┘
```

**Icon Color:**
- 🔴 Red = Monitoring stopped
- 🟢 Green = Monitoring active

## 🔧 Configuration

### Display Layout

Displays are connected to ESP32 via SPI:

| Display | GPIO CS | GPIO DC | GPIO RST |
|---------|---------|---------|----------|
| CPU     | 11      | 12      | 13       |
| GPU     | 10      | 9       | 46       |
| RAM     | 3       | 8       | 18       |
| Network | 15      | 16      | 17       |

Shared pins (all displays):
- **SCK**: GPIO 4
- **MOSI**: GPIO 5
- **VCC**: 3.3V
- **GND**: GND

### Customization

Want to change what's displayed?

1. **ESP32 Firmware**: Edit `main/main_lvgl.c`
2. **Data Format**: Edit `python/pc_monitor.py`
3. **Display Colors**: Modify LVGL styles in firmware

## 📚 Documentation

- **[SOFTWARE.md](docs/SOFTWARE.md)** - Software installation, building EXE, usage
- **[HARDWARE.md](docs/HARDWARE.md)** - Wiring, assembly, 3D printing files

## 🐛 Troubleshooting

### Displays stay black
→ Check power connections (VCC + GND to all displays)

### ESP32 not detected
→ Check USB cable, try different port, verify COM port in Device Manager

### "LibreHardwareMonitor DLL not found"
→ Download from [LibreHardwareMonitor releases](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases) and place `LibreHardwareMonitorLib.dll` in `python/` folder

### Monitoring won't start
→ Run `PC Monitor Manager.exe` as Administrator (required for hardware sensors)

### "Access Denied" or sensor errors
→ LibreHardwareMonitor requires admin rights to read hardware sensors

## 🤝 Contributing

Contributions welcome! Please:

1. Fork the repo
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License.

## 🙏 Acknowledgments

- [LVGL](https://lvgl.io/) - Embedded GUI library
- [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) - Hardware monitoring
- [ESP-IDF](https://github.com/espressif/esp-idf) - ESP32 framework
- GC9A01 display drivers

## 💬 Support

- **Issues**: [GitHub Issues](https://github.com/yourname/pc-monitor/issues)
- **Discussions**: [GitHub Discussions](https://github.com/yourname/pc-monitor/discussions)

---

**Made with ❤️ for PC hardware enthusiasts**
