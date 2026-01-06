#!/usr/bin/env python3
"""
PC Monitor - Bidirectional (sends AND receives logs)
For ESP32-S3 USB Serial JTAG
"""

import time
import psutil
import serial
import serial.tools.list_ports
import sys
import threading

try:
    import GPUtil
    GPU_AVAILABLE = True
except ImportError:
    GPU_AVAILABLE = False

try:
    import wmi
    WMI_AVAILABLE = True
except ImportError:
    WMI_AVAILABLE = False

# Try OpenHardwareMonitor for better temperature readings
try:
    import clr
    import os
    from pathlib import Path

    dll_path = Path(__file__).parent / "LibreHardwareMonitorLib.dll"
    if dll_path.exists():
        clr.AddReference(str(dll_path))
        from LibreHardwareMonitor import Hardware
        OHM_AVAILABLE = True
    else:
        OHM_AVAILABLE = False
except:
    OHM_AVAILABLE = False


class PCMonitor:
    def __init__(self, port=None, silent=False):
        self.serial_port = None
        self.wmi_interface = wmi.WMI() if WMI_AVAILABLE else None
        self.hardware = None
        self.running = True
        self.specified_port = port
        self.silent = silent  # For use with tray app (no prints)

        # Initialize OpenHardwareMonitor if available
        if OHM_AVAILABLE:
            try:
                self.hardware = Hardware.Computer()
                self.hardware.IsCpuEnabled = True
                self.hardware.IsGpuEnabled = True
                self.hardware.Open()
                self.log("Using LibreHardwareMonitor for temperatures")
            except:
                self.hardware = None

    def log(self, msg):
        """Print only if not silent"""
        if not self.silent:
            print(msg)

    def find_esp32(self):
        """Find ESP32 USB Serial JTAG port"""
        ports = serial.tools.list_ports.comports()

        if self.specified_port:
            self.log(f"Trying specified port: {self.specified_port}")
            try:
                ser = serial.Serial(self.specified_port, 115200, timeout=0.1)
                self.log(f"Connected to {self.specified_port}")
                return ser
            except Exception as e:
                self.log(f"Could not connect to {self.specified_port}: {e}")
                return None

        for port in ports:
            desc_upper = port.description.upper()

            is_usb_serial = (
                ("USB" in desc_upper and "SERIAL" in desc_upper) or
                ("USB" in desc_upper and "SERIELL" in desc_upper) or
                ("SERIELLES USB" in desc_upper)
            )

            if is_usb_serial and "CH343" not in port.description:
                self.log(f"Found ESP32 at {port.device}: {port.description}")
                try:
                    ser = serial.Serial(port.device, 115200, timeout=0.1)
                    self.log(f"Connected to {port.device}")
                    return ser
                except Exception as e:
                    self.log(f"Could not connect to {port.device}: {e}")

        self.log("\nESP32 USB Serial JTAG not found!")
        self.log("\nAvailable ports:")
        for port in ports:
            self.log(f"  {port.device}: {port.description}")
        return None

    def get_cpu_temp_ohm(self):
        """Get CPU temp via OpenHardwareMonitor"""
        if not self.hardware:
            return None

        try:
            for hw in self.hardware.Hardware:
                if hw.HardwareType == Hardware.HardwareType.Cpu:
                    hw.Update()
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Temperature:
                            if "package" in sensor.Name.lower() or "average" in sensor.Name.lower():
                                return float(sensor.Value or 0)
        except:
            pass
        return None

    def get_cpu_stats(self):
        """CPU usage and temperature"""
        cpu_percent = int(psutil.cpu_percent(interval=0.1))

        # Try OpenHardwareMonitor first (most accurate)
        cpu_temp = self.get_cpu_temp_ohm()

        # Fallback to WMI
        if cpu_temp is None and WMI_AVAILABLE and self.wmi_interface:
            try:
                temps = self.wmi_interface.MSAcpi_ThermalZoneTemperature()
                if temps:
                    cpu_temp = (temps[0].CurrentTemperature / 10.0) - 273.15
            except:
                pass

        # Fallback to psutil sensors (if available)
        if cpu_temp is None:
            try:
                temps = psutil.sensors_temperatures()
                if temps:
                    # Try different sensor names
                    for name in ['coretemp', 'k10temp', 'cpu-thermal']:
                        if name in temps:
                            cpu_temp = temps[name][0].current
                            break
            except:
                pass

        # Last resort: estimate based on load
        if cpu_temp is None:
            # Rough estimate: idle=40°C, full load=80°C
            cpu_temp = 40.0 + (cpu_percent * 0.4)

        return cpu_percent, cpu_temp

    def get_gpu_stats(self):
        """GPU usage, temperature and VRAM"""
        if not GPU_AVAILABLE:
            return 0, 0.0, 0.0, 8.0

        try:
            gpus = GPUtil.getGPUs()
            if gpus:
                gpu = gpus[0]
                gpu_percent = int(gpu.load * 100)
                gpu_temp = gpu.temperature
                vram_used = gpu.memoryUsed / 1024
                vram_total = gpu.memoryTotal / 1024
                return gpu_percent, gpu_temp, vram_used, vram_total
        except:
            pass

        return 0, 0.0, 0.0, 8.0

    def get_ram_stats(self):
        """RAM usage"""
        ram = psutil.virtual_memory()
        ram_used_gb = ram.used / (1024**3)
        ram_total_gb = ram.total / (1024**3)
        return ram_used_gb, ram_total_gb

    def get_network_stats(self):
        """Network status and traffic"""
        net_type = "LAN"
        net_speed = "1000 Mbps"

        net_io = psutil.net_io_counters()

        if not hasattr(self, 'last_net_io'):
            self.last_net_io = net_io
            self.last_net_time = time.time()
            return net_type, net_speed, 0.0, 0.0

        time_delta = time.time() - self.last_net_time
        if time_delta > 0:
            bytes_sent_delta = net_io.bytes_sent - self.last_net_io.bytes_sent
            bytes_recv_delta = net_io.bytes_recv - self.last_net_io.bytes_recv

            net_up_mbps = (bytes_sent_delta / time_delta) / (1024 * 1024)
            net_down_mbps = (bytes_recv_delta / time_delta) / (1024 * 1024)
        else:
            net_up_mbps = 0.0
            net_down_mbps = 0.0

        self.last_net_io = net_io
        self.last_net_time = time.time()

        return net_type, net_speed, net_down_mbps, net_up_mbps

    def send_data_thread(self):
        """Thread: Sends data every second"""
        self.log("TX Thread started - sending data every second...")

        while self.running:
            try:
                cpu_percent, cpu_temp = self.get_cpu_stats()
                gpu_percent, gpu_temp, vram_used, vram_total = self.get_gpu_stats()
                ram_used, ram_total = self.get_ram_stats()
                net_type, net_speed, net_down, net_up = self.get_network_stats()

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

                self.serial_port.write(data_str.encode('utf-8'))
                if not self.silent:
                    print(f"TX: CPU={cpu_percent}% ({cpu_temp:.1f}°C) GPU={gpu_percent}% ({gpu_temp:.1f}°C) RAM={ram_used:.1f}GB", end='\r')

            except Exception as e:
                self.log(f"\nTX Error: {e}")
                self.running = False
                break

            time.sleep(1)

    def receive_log_thread(self):
        """Thread: Receives and displays ESP32 logs"""
        self.log("RX Thread started - showing ESP32 logs...\n")

        buffer = ""
        while self.running:
            try:
                if self.serial_port.in_waiting > 0:
                    chunk = self.serial_port.read(self.serial_port.in_waiting).decode('utf-8', errors='ignore')
                    buffer += chunk

                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        if line.strip():
                            self.log(f"[ESP32] {line}")

            except Exception as e:
                if self.running:
                    self.log(f"\nRX Error: {e}")
                break

            time.sleep(0.01)

    def run(self):
        """Main loop"""
        self.log("=" * 60)
        self.log("PC Monitor - Bidirectional Mode")
        self.log("=" * 60)
        self.log("Searching for ESP32 USB Serial JTAG...")

        self.serial_port = self.find_esp32()
        if not self.serial_port:
            self.log("\nPlease connect ESP32-S3 via USB and try again!")
            sys.exit(1)

        self.log("\nConnected!")
        self.log("=" * 60)
        self.log("Sending PC stats every second...")
        self.log("Showing ESP32 logs below...")
        self.log("Press Ctrl+C to stop\n")
        self.log("=" * 60)

        # Start both threads
        tx_thread = threading.Thread(target=self.send_data_thread, daemon=True)
        rx_thread = threading.Thread(target=self.receive_log_thread, daemon=True)

        tx_thread.start()
        rx_thread.start()

        try:
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            self.log("\n\nStopping...")
            self.running = False
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            if self.hardware:
                self.hardware.Close()
            self.log("Closed connection")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description='PC Monitor - Bidirectional')
    parser.add_argument('--port', type=str, help='COM port (e.g., COM4)', default=None)
    parser.add_argument('--silent', action='store_true', help='Silent mode (no console output)')
    args = parser.parse_args()

    monitor = PCMonitor(port=args.port, silent=args.silent)
    monitor.run()
