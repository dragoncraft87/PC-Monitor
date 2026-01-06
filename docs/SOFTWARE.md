# 💻 Software Setup Guide

Complete guide for installing, building, and using the PC Monitor software.

---

## 📋 Table of Contents

1. [Quick Start (End Users)](#-quick-start-end-users)
2. [ESP32 Firmware Setup](#-esp32-firmware-setup)
3. [Windows Manager Setup](#-windows-manager-setup)
4. [Building the EXE](#-building-the-exe)
5. [Usage](#-usage)
6. [Troubleshooting](#-troubleshooting)

---

## 🚀 Quick Start (End Users)

If you just want to use the pre-built exe:

### Prerequisites

- Windows 10/11 (64-bit)
- Administrator rights
- ESP32 connected via USB
- Displays assembled (see [HARDWARE.md](HARDWARE.md))

### Installation Steps

1. **Download the release package**
   - Get `PC-Monitor-vX.X.X.zip` from releases
   - Extract to a folder (e.g., `C:\PC-Monitor`)

2. **Download LibreHardwareMonitor DLL**
   - Go to https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases
   - Download the latest release ZIP
   - Extract and find `LibreHardwareMonitorLib.dll`
   - Copy it to `C:\PC-Monitor\python\` folder

3. **Run the manager**
   - Double-click `PC Monitor Manager.exe`
   - Windows may show UAC prompt → Click "Yes" (required for hardware access)
   - Red icon appears in system tray

4. **Start monitoring**
   - Right-click tray icon
   - Click "Start Monitoring"
   - Icon turns green
   - Displays should light up! 🎉

---

## 🔧 ESP32 Firmware Setup

### Prerequisites

- ESP-IDF v5.0 or higher
- Python 3.8+
- Git
- USB cable

### Install ESP-IDF (Windows)

```powershell
# Download ESP-IDF installer
# https://dl.espressif.com/dl/esp-idf/

# Or use Chocolatey
choco install esp-idf

# Verify installation
idf.py --version
```

### Install ESP-IDF (Linux/macOS)

```bash
# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.1

# Install
./install.sh esp32s3

# Setup environment (add to ~/.bashrc)
. $HOME/esp/esp-idf/export.sh

# Verify
idf.py --version
```

### Build and Flash Firmware

```bash
# Navigate to project
cd pc-monitor-poc

# Clean build (recommended first time)
idf.py fullclean

# Configure (loads PSRAM settings)
idf.py reconfigure

# Build
idf.py build

# Find COM port
# Windows: Check Device Manager → Ports (COM & LPT)
# Linux: ls /dev/ttyUSB* or /dev/ttyACM*
# macOS: ls /dev/cu.usb*

# Flash (replace COM3 with your port)
idf.py -p COM3 flash

# Flash and monitor
idf.py -p COM3 flash monitor
```

### Expected Output

```
I (123) PC-MONITOR: === PC Monitor 4x Display with LVGL ===
I (234) PC-MONITOR: ESP32-S3 N16R8 with PSRAM
I (345) PC-MONITOR: Initializing SPI bus...
I (456) LVGL: Initializing GC9A01 displays...
I (567) LVGL: Display 1 (CPU) initialized
I (678) LVGL: Display 2 (GPU) initialized
I (789) LVGL: Display 3 (RAM) initialized
I (890) LVGL: Display 4 (Network) initialized
I (991) PC-MONITOR: === System ready! ===
```

Press `Ctrl + ]` to exit monitor.

---

## 💻 Windows Manager Setup

### For Development (Python Script)

#### Step 1: Install Python

```powershell
# Download Python 3.8+
# https://www.python.org/downloads/

# Verify installation
python --version
```

#### Step 2: Install Dependencies

```bash
# Navigate to project
cd pc-monitor-poc

# Install requirements
pip install -r requirements.txt
```

#### Step 3: Download LibreHardwareMonitor DLL

```bash
# Download from:
# https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

# Extract and copy LibreHardwareMonitorLib.dll to:
# pc-monitor-poc/python/LibreHardwareMonitorLib.dll
```

#### Step 4: Run Tray Manager

```bash
# Must run as Administrator!
python pc_monitor_tray.py
```

**Important:** Right-click → "Run as Administrator" is required for hardware sensor access.

---

## 🏗️ Building the EXE

### Option 1: Using the .spec File (Recommended)

```bash
# Install PyInstaller (if not already installed)
pip install pyinstaller

# Build using spec file
pyinstaller "PC Monitor Manager.spec"

# EXE will be in dist/ folder
# dist/PC Monitor Manager.exe
```

### Option 2: Manual Build

```bash
# Build with all parameters
pyinstaller ^
    --name="PC Monitor Manager" ^
    --onefile ^
    --windowed ^
    --icon=icon.ico ^
    --add-data="python/pc_monitor.py;python" ^
    --add-data="icon.ico;." ^
    --hidden-import=pystray ^
    --hidden-import=PIL ^
    --uac-admin ^
    pc_monitor_tray.py
```

### Build Output

```
dist/
└── PC Monitor Manager.exe    (~18-25 MB)
```

This EXE is standalone and can be copied to any Windows PC!

### Creating a Release Package

```bash
# Create release folder
mkdir PC-Monitor-Release
cd PC-Monitor-Release

# Copy files
copy "..\dist\PC Monitor Manager.exe" .
mkdir python
copy "..\python\pc_monitor.py" python\
copy "..\icon.ico" .

# Add README.txt with instructions

# Zip it
# PC-Monitor-v1.0.0.zip
```

**Note:** Users must download LibreHardwareMonitorLib.dll separately (license compliance).

---

## 🎮 Usage

### System Tray Manager

After running `PC Monitor Manager.exe`:

#### Tray Icon States

| Icon Color | Meaning |
|------------|---------|
| 🔴 Red | Monitoring stopped |
| 🟢 Green | Monitoring active |

#### Right-Click Menu

```
┌──────────────────────────────┐
│ Start Monitoring             │ ← Begin sending data
│ Stop Monitoring              │ ← Stop monitoring
├──────────────────────────────┤
│ Add to Autostart             │ ← Start with Windows
│ Remove from Autostart        │ ← Remove from startup
├──────────────────────────────┤
│ Quit                         │ ← Exit application
└──────────────────────────────┘
```

### Autostart Setup

**Method 1: Via Tray Menu (Easiest)**
1. Right-click tray icon
2. Click "Add to Autostart"
3. Done! Starts with Windows

**Method 2: Manual Registry**

The app adds itself to:
```
HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run
```

Key: `PC Monitor Manager`
Value: `"C:\path\to\PC Monitor Manager.exe"`

**Method 3: Startup Folder**

1. Press `Win + R`
2. Type `shell:startup`
3. Copy `PC Monitor Manager.exe` or create shortcut
4. Restart to test

---

## 🐛 Troubleshooting

### EXE Won't Start

**Problem:** Double-clicking does nothing

**Solutions:**
- Check Windows Event Viewer for errors
- Try running from command prompt to see errors
- Verify all DLLs are present (especially LibreHardwareMonitorLib.dll)

---

### "LibreHardwareMonitor DLL not found"

**Problem:** Error message about missing DLL

**Solution:**
1. Download DLL from: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases
2. Extract ZIP
3. Copy `LibreHardwareMonitorLib.dll` to `python/` folder
4. Restart app

---

### "Access Denied" or Sensor Errors

**Problem:** Hardware sensors return 0 or error

**Solutions:**
- Run as Administrator (UAC prompt)
- Check Windows Security isn't blocking
- Ensure no other hardware monitoring tools are running (conflicts)

---

### ESP32 Not Found

**Problem:** "ESP32 USB Serial JTAG not found"

**Solutions:**
1. **Check USB cable** - Data cable, not charge-only
2. **Check Device Manager** - Should see "USB Serial Device" or similar
3. **Install drivers** - Usually automatic on Windows 10/11
4. **Try different USB port** - Preferably USB 3.0
5. **Reset ESP32** - Press reset button

---

### Displays Show Wrong Data

**Problem:** CPU data appears on GPU display, etc.

**Solutions:**
- Check display wiring (see [HARDWARE.md](HARDWARE.md))
- CS pins might be swapped
- Verify pin assignments in firmware match wiring

---

### No Data Updating

**Problem:** Displays show 0% or don't update

**Solutions:**
1. **Check monitoring is started** - Icon should be green
2. **Check ESP32 monitor** - Run `idf.py monitor` to see incoming data
3. **Restart monitoring** - Stop and start again
4. **Check Python script** - Run `python python/pc_monitor.py` manually to debug

---

### Performance Issues

**Problem:** High CPU usage or lag

**Solutions:**
- Close other hardware monitoring tools (MSI Afterburner, HWInfo, etc.)
- Update to latest LibreHardwareMonitor DLL
- Increase update interval in `pc_monitor.py` (line 276: change `time.sleep(1)` to `time.sleep(2)`)

---

### Build Errors (PyInstaller)

**Problem:** `pyinstaller` command fails

**Solutions:**

**"Module not found":**
```bash
pip install --upgrade pip
pip install -r requirements.txt
pip install pyinstaller
```

**"Permission denied":**
```bash
# Run as Administrator
# Or disable antivirus temporarily
```

**"File not found" for icon:**
```bash
# Generate icon first
python create_icon.py

# Verify icon.ico exists
dir icon.ico
```

---

## 📊 Advanced Configuration

### Change Update Rate

Edit `python/pc_monitor.py` line 276:

```python
time.sleep(1)  # 1 second refresh
# Change to:
time.sleep(0.5)  # 500ms refresh (faster)
# Or:
time.sleep(2)  # 2 second refresh (slower, less CPU)
```

### Customize Data Format

Edit `python/pc_monitor.py` line 232-243 to change what data is sent:

```python
data_str = (
    f"CPU:{cpu_percent},"
    f"CPUT:{cpu_temp:.1f},"
    # Add more fields or remove existing ones
)
```

### Modify Display Layout

Edit ESP32 firmware `main/main_lvgl.c` to change display positions, colors, or layout.

---

## 🧪 Testing

### Test Monitor Script Standalone

```bash
# Run without tray manager
cd python
python pc_monitor.py

# Should output:
# LibreHardwareMonitor initialized
# Found ESP32 at COM3
# Connected!
# Sending data...
# TX: CPU=45% (62.5°C) GPU=72% RAM=8.2GB
```

Press `Ctrl+C` to stop.

### Test Tray Manager Without ESP32

```bash
# Test tray icon and menu
python pc_monitor_tray.py

# Right-click icon to test menu
# Won't send data without ESP32, but should not crash
```

---

## 📝 Release Checklist

Before creating a release:

- [ ] All displays working correctly
- [ ] ESP32 firmware flashed and tested
- [ ] Python script tested standalone
- [ ] Tray manager tested
- [ ] EXE built successfully
- [ ] EXE tested on clean Windows install
- [ ] Autostart function tested
- [ ] README.md updated
- [ ] Screenshots added
- [ ] Version number updated
- [ ] CHANGELOG.md updated

---

## 🚀 Next Steps

After setup:

1. Test all displays for 10+ minutes
2. Configure autostart if desired
3. Customize colors/layout to your preference
4. Share your build! (Add photos to README)
5. Consider 3D printing a case

---

**Need help? Open an issue on GitHub!**

See [README.md](../README.md) for more information.
