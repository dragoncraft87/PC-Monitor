#!/usr/bin/env python3
"""
PC Monitor - Bidirectional (sends AND receives logs)
For ESP32-S3 USB Serial JTAG
Thread-safe version for integration with tray app
"""

import time
import psutil
import serial
import serial.tools.list_ports
import sys
import threading

try:
    import wmi
    WMI_AVAILABLE = True
except ImportError:
    WMI_AVAILABLE = False

# Try LibreHardwareMonitor for better temperature readings
try:
    import clr
    import os
    from pathlib import Path

    # Handle both regular script and PyInstaller bundle
    if getattr(sys, 'frozen', False):
        # Running as PyInstaller bundle
        base_path = Path(sys._MEIPASS)
    else:
        # Running as script
        base_path = Path(__file__).parent

    dll_path = base_path / "LibreHardwareMonitorLib.dll"
    if dll_path.exists():
        clr.AddReference(str(dll_path))
        from LibreHardwareMonitor import Hardware
        OHM_AVAILABLE = True
    else:
        OHM_AVAILABLE = False
except:
    OHM_AVAILABLE = False


class PCMonitor:
    def __init__(self, port=None, silent=True, stop_event=None):
        self.serial_port = None
        self.wmi_interface = wmi.WMI() if WMI_AVAILABLE else None
        self.hardware = None
        self.running = True
        self.specified_port = port
        self.silent = silent  # For use with tray app (no prints)
        self.stop_event = stop_event  # threading.Event() to signal stop

        # Threads
        self.tx_thread = None
        self.rx_thread = None

        # Initialize LibreHardwareMonitor if available
        if OHM_AVAILABLE:
            try:
                self.hardware = Hardware.Computer()
                self.hardware.IsCpuEnabled = True
                self.hardware.IsGpuEnabled = True
                self.hardware.Open()
                self.log("Using LibreHardwareMonitor for CPU and GPU monitoring")
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
        """Get CPU temp via LibreHardwareMonitor"""
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

        # Try LibreHardwareMonitor first (most accurate)
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
        """GPU usage, temperature and VRAM via LibreHardwareMonitor"""
        gpu_percent = 0
        gpu_temp = 0.0
        vram_used = 0.0
        vram_total = 8.0  # Default fallback

        if not self.hardware:
            return gpu_percent, gpu_temp, vram_used, vram_total

        try:
            for hw in self.hardware.Hardware:
                # Check for GPU (Nvidia, AMD, or Intel)
                if hw.HardwareType in [Hardware.HardwareType.GpuNvidia,
                                       Hardware.HardwareType.GpuAmd,
                                       Hardware.HardwareType.GpuIntel]:
                    hw.Update()

                    for sensor in hw.Sensors:
                        sensor_name_lower = sensor.Name.lower()

                        # GPU Load/Usage
                        if sensor.SensorType == Hardware.SensorType.Load:
                            if "core" in sensor_name_lower or "gpu" in sensor_name_lower:
                                gpu_percent = int(sensor.Value or 0)

                        # GPU Temperature
                        elif sensor.SensorType == Hardware.SensorType.Temperature:
                            if "core" in sensor_name_lower or "gpu" in sensor_name_lower:
                                gpu_temp = float(sensor.Value or 0)

                        # VRAM Usage (Data type in GB)
                        elif sensor.SensorType == Hardware.SensorType.Data:
                            if "memory used" in sensor_name_lower or "used" in sensor_name_lower:
                                vram_used = float(sensor.Value or 0)
                            elif "memory total" in sensor_name_lower or "total" in sensor_name_lower:
                                vram_total = float(sensor.Value or 8.0)

                    # Found GPU, break
                    break
        except:
            pass

        return gpu_percent, gpu_temp, vram_used, vram_total

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
            # Check stop event
            if self.stop_event and self.stop_event.is_set():
                break

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
            # Check stop event
            if self.stop_event and self.stop_event.is_set():
                break

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

    def start(self):
        """Start monitoring (non-blocking, for tray app)"""
        self.log("=" * 60)
        self.log("PC Monitor - Bidirectional Mode")
        self.log("=" * 60)
        self.log("Searching for ESP32 USB Serial JTAG...")

        self.serial_port = self.find_esp32()
        if not self.serial_port:
            self.log("\nESP32 not found!")
            return False

        self.log("\nConnected!")
        self.log("=" * 60)
        self.log("Sending PC stats every second...")
        self.log("Showing ESP32 logs below...")
        self.log("=" * 60)

        # Start both threads
        self.running = True
        self.tx_thread = threading.Thread(target=self.send_data_thread, daemon=True)
        self.rx_thread = threading.Thread(target=self.receive_log_thread, daemon=True)

        self.tx_thread.start()
        self.rx_thread.start()

        return True

    def stop(self):
        """Stop monitoring (clean shutdown)"""
        self.log("\nStopping...")
        self.running = False

        # Wait for threads to finish (max 2 seconds)
        if self.tx_thread and self.tx_thread.is_alive():
            self.tx_thread.join(timeout=2)
        if self.rx_thread and self.rx_thread.is_alive():
            self.rx_thread.join(timeout=2)

        # Close serial port
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()

        # Close hardware monitor
        if self.hardware:
            self.hardware.Close()

        self.log("Stopped monitoring")

    def run(self):
        """Main loop (blocking, for standalone usage)"""
        if not self.start():
            sys.exit(1)

        try:
            while self.running:
                if self.stop_event and self.stop_event.is_set():
                    break
                time.sleep(0.1)
        except KeyboardInterrupt:
            pass
        finally:
            self.stop()


# Standalone function for tray app integration
def run_monitor(stop_event, port=None):
    """Run monitor in a thread (call this from tray app)"""
    monitor = PCMonitor(port=port, silent=True, stop_event=stop_event)
    monitor.run()


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description='PC Monitor - Bidirectional')
    parser.add_argument('--port', type=str, help='COM port (e.g., COM4)', default=None)
    parser.add_argument('--silent', action='store_true', help='Silent mode (no console output)')
    args = parser.parse_args()

    monitor = PCMonitor(port=args.port, silent=args.silent)
    monitor.run()
