#!/usr/bin/env python3
"""
PC Monitor - Sends PC stats to ESP32 via USB Serial
Uses LibreHardwareMonitor for accurate hardware monitoring
"""

import time
import sys
import serial
import serial.tools.list_ports
import clr
import os
from pathlib import Path

# Load LibreHardwareMonitor DLL
try:
    dll_path = Path(__file__).parent / "LibreHardwareMonitorLib.dll"
    if not dll_path.exists():
        print(f"ERROR: LibreHardwareMonitorLib.dll not found at {dll_path}")
        print("Please download from: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor")
        sys.exit(1)

    clr.AddReference(str(dll_path))
    from LibreHardwareMonitor import Hardware
    HARDWARE_MONITOR_AVAILABLE = True
except Exception as e:
    print(f"ERROR: Failed to load LibreHardwareMonitor: {e}")
    HARDWARE_MONITOR_AVAILABLE = False


class PCMonitor:
    def __init__(self):
        self.serial_port = None
        self.hardware = None

        if HARDWARE_MONITOR_AVAILABLE:
            try:
                self.hardware = Hardware.Computer()
                self.hardware.IsCpuEnabled = True
                self.hardware.IsGpuEnabled = True
                self.hardware.IsMemoryEnabled = True
                self.hardware.IsNetworkEnabled = True
                self.hardware.Open()
                print("✅ LibreHardwareMonitor initialized")
            except Exception as e:
                print(f"⚠️ Failed to initialize hardware monitoring: {e}")
                self.hardware = None

    def find_esp32(self):
        """Auto-detect ESP32 USB port"""
        ports = serial.tools.list_ports.comports()

        for port in ports:
            # ESP32-S3 USB Serial JTAG shows as "USB Serial Device"
            desc_upper = port.description.upper()

            is_usb_serial = (
                ("USB" in desc_upper and "SERIAL" in desc_upper) or
                ("SERIELL" in desc_upper and "USB" in desc_upper)
            )

            if is_usb_serial and "CH343" not in port.description:
                print(f"🔍 Found ESP32 at {port.device}: {port.description}")
                try:
                    ser = serial.Serial(port.device, 115200, timeout=0.1)
                    print(f"✅ Connected to {port.device}")
                    return ser
                except Exception as e:
                    print(f"❌ Could not connect to {port.device}: {e}")

        print("\n❌ ESP32 USB Serial JTAG not found!")
        print("\nAvailable ports:")
        for port in ports:
            print(f"  {port.device}: {port.description}")
        return None

    def get_sensor_value(self, hardware_item, sensor_type, name_contains=None):
        """Get sensor value from LibreHardwareMonitor"""
        try:
            hardware_item.Update()
            for sensor in hardware_item.Sensors:
                if sensor.SensorType == sensor_type:
                    if name_contains is None or name_contains.lower() in sensor.Name.lower():
                        if sensor.Value is not None:
                            return float(sensor.Value)
        except:
            pass
        return 0.0

    def get_cpu_stats(self):
        """CPU usage and temperature via LibreHardwareMonitor"""
        cpu_percent = 0.0
        cpu_temp = 0.0

        if self.hardware:
            for hw in self.hardware.Hardware:
                if hw.HardwareType == Hardware.HardwareType.Cpu:
                    hw.Update()

                    # CPU Load
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Load:
                            if "total" in sensor.Name.lower():
                                cpu_percent = float(sensor.Value or 0)
                                break

                    # CPU Temperature (Package or average)
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Temperature:
                            if "package" in sensor.Name.lower() or "average" in sensor.Name.lower():
                                cpu_temp = float(sensor.Value or 0)
                                break

                    break

        return int(cpu_percent), cpu_temp

    def get_gpu_stats(self):
        """GPU usage, temperature and VRAM via LibreHardwareMonitor"""
        gpu_percent = 0.0
        gpu_temp = 0.0
        vram_used = 0.0
        vram_total = 8.0

        if self.hardware:
            for hw in self.hardware.Hardware:
                if hw.HardwareType in [Hardware.HardwareType.GpuNvidia,
                                       Hardware.HardwareType.GpuAmd,
                                       Hardware.HardwareType.GpuIntel]:
                    hw.Update()

                    # GPU Load
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Load:
                            if "core" in sensor.Name.lower() or "gpu" in sensor.Name.lower():
                                gpu_percent = float(sensor.Value or 0)
                                break

                    # GPU Temperature
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Temperature:
                            if "core" in sensor.Name.lower() or "gpu" in sensor.Name.lower():
                                gpu_temp = float(sensor.Value or 0)
                                break

                    # VRAM
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.SmallData:
                            name_lower = sensor.Name.lower()
                            if "memory used" in name_lower:
                                vram_used = float(sensor.Value or 0) / 1024  # MB to GB
                            elif "memory total" in name_lower:
                                vram_total = float(sensor.Value or 0) / 1024

                    break

        return int(gpu_percent), gpu_temp, vram_used, vram_total

    def get_ram_stats(self):
        """RAM usage via LibreHardwareMonitor"""
        ram_used = 0.0
        ram_total = 16.0

        if self.hardware:
            for hw in self.hardware.Hardware:
                if hw.HardwareType == Hardware.HardwareType.Memory:
                    hw.Update()

                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Data:
                            name_lower = sensor.Name.lower()
                            if "used" in name_lower:
                                ram_used = float(sensor.Value or 0)
                            elif "available" in name_lower or "total" in name_lower:
                                ram_total = float(sensor.Value or 0)

                    break

        return ram_used, ram_total

    def get_network_stats(self):
        """Network stats via LibreHardwareMonitor"""
        net_type = "LAN"
        net_speed = "1000 Mbps"
        net_down = 0.0
        net_up = 0.0

        if self.hardware:
            for hw in self.hardware.Hardware:
                if hw.HardwareType == Hardware.HardwareType.Network:
                    hw.Update()

                    # Find active adapter (non-zero throughput)
                    has_activity = False
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Throughput:
                            if sensor.Value and sensor.Value > 0.01:
                                has_activity = True
                                break

                    if not has_activity:
                        continue

                    # Get throughput
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Throughput:
                            name_lower = sensor.Name.lower()
                            if "download" in name_lower or "receive" in name_lower:
                                net_down = float(sensor.Value or 0)
                            elif "upload" in name_lower or "send" in name_lower:
                                net_up = float(sensor.Value or 0)

                    # Detect connection type
                    hw_name_lower = hw.Name.lower()
                    if "wi-fi" in hw_name_lower or "wireless" in hw_name_lower:
                        net_type = "WiFi"
                    elif "ethernet" in hw_name_lower or "lan" in hw_name_lower:
                        net_type = "LAN"

                    break

        return net_type, net_speed, net_down, net_up

    def send_data(self, ser):
        """Collect and send all data"""
        cpu_percent, cpu_temp = self.get_cpu_stats()
        gpu_percent, gpu_temp, vram_used, vram_total = self.get_gpu_stats()
        ram_used, ram_total = self.get_ram_stats()
        net_type, net_speed, net_down, net_up = self.get_network_stats()

        # Format: CPU:45,CPUT:62.5,GPU:72,GPUT:68.3,VRAM:4.2/8.0,RAM:10.4/16.0,NET:LAN,SPEED:1000,DOWN:12.4,UP:2.1
        data_str = (
            f"CPU:{cpu_percent},"
            f"CPUT:{cpu_temp:.1f},"
            f"GPU:{gpu_percent},"
            f"GPUT:{gpu_temp:.1f},"
            f"VRAM:{vram_used:.1f}/{vram_total:.1f},"
            f"RAM:{ram_used:.1f}/{ram_total:.1f},"
            f"NET:{net_type},"
            f"SPEED:{net_speed},"
            f"DOWN:{net_down:.1f},"
            f"UP:{net_up:.1f}\n"
        )

        try:
            ser.write(data_str.encode('utf-8'))
            print(f"📤 TX: CPU={cpu_percent}% ({cpu_temp:.1f}°C) GPU={gpu_percent}% RAM={ram_used:.1f}GB", end='\r')
            return True
        except Exception as e:
            print(f"\n❌ Error sending data: {e}")
            return False

    def run(self):
        """Main loop"""
        print("=" * 60)
        print("🚀 PC Monitor - LibreHardwareMonitor Edition")
        print("=" * 60)
        print("🔍 Searching for ESP32...")

        self.serial_port = self.find_esp32()
        if not self.serial_port:
            print("\n❌ Please connect ESP32 via USB and try again!")
            sys.exit(1)

        print("\n✅ Connected! Sending data every second...")
        print("Press Ctrl+C to stop\n")

        try:
            while True:
                if not self.send_data(self.serial_port):
                    print("\n⚠️ Lost connection, trying to reconnect...")
                    self.serial_port = self.find_esp32()
                    if not self.serial_port:
                        break

                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\n👋 Stopping...")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            if self.hardware:
                self.hardware.Close()
            print("✅ Closed connection")


if __name__ == "__main__":
    if not HARDWARE_MONITOR_AVAILABLE:
        print("\n❌ LibreHardwareMonitor not available!")
        print("Please ensure LibreHardwareMonitorLib.dll is in the same directory.")
        sys.exit(1)

    monitor = PCMonitor()
    monitor.run()
