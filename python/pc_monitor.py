#!/usr/bin/env python3
"""
PC Monitor - Sends PC stats to ESP32 via USB Serial
Simple version using psutil and GPUtil (works without admin rights for basic monitoring)
"""

import time
import sys
import serial
import serial.tools.list_ports
import psutil

try:
    import GPUtil
    GPU_AVAILABLE = True
except ImportError:
    GPU_AVAILABLE = False
    print("Warning: GPUtil not installed. GPU monitoring disabled.")

try:
    import wmi
    WMI_AVAILABLE = True
except ImportError:
    WMI_AVAILABLE = False
    print("Warning: WMI not installed. Temperature monitoring limited.")


class PCMonitor:
    def __init__(self):
        self.serial_port = None
        self.wmi_interface = wmi.WMI() if WMI_AVAILABLE else None

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
                print(f"Found ESP32 at {port.device}: {port.description}")
                try:
                    ser = serial.Serial(port.device, 115200, timeout=0.1)
                    print(f"Connected to {port.device}")
                    return ser
                except Exception as e:
                    print(f"Could not connect to {port.device}: {e}")

        print("\nESP32 USB Serial JTAG not found!")
        print("\nAvailable ports:")
        for port in ports:
            print(f"  {port.device}: {port.description}")
        return None

    def get_cpu_stats(self):
        """CPU usage and temperature"""
        cpu_percent = int(psutil.cpu_percent(interval=0.1))
        cpu_temp = 60.0  # Default

        if WMI_AVAILABLE and self.wmi_interface:
            try:
                temps = self.wmi_interface.MSAcpi_ThermalZoneTemperature()
                if temps:
                    cpu_temp = (temps[0].CurrentTemperature / 10.0) - 273.15
            except:
                pass

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
                vram_used = gpu.memoryUsed / 1024  # MB to GB
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
        """Network stats"""
        net_type = "LAN"
        net_speed = "1000 Mbps"
        net_down = 0.0
        net_up = 0.0

        net_io = psutil.net_io_counters()

        if not hasattr(self, 'last_net_io'):
            self.last_net_io = net_io
            self.last_net_time = time.time()
            return net_type, net_speed, 0.0, 0.0

        time_delta = time.time() - self.last_net_time
        if time_delta > 0:
            bytes_sent_delta = net_io.bytes_sent - self.last_net_io.bytes_sent
            bytes_recv_delta = net_io.bytes_recv - self.last_net_io.bytes_recv

            net_up = (bytes_sent_delta / time_delta) / (1024 * 1024)
            net_down = (bytes_recv_delta / time_delta) / (1024 * 1024)

        self.last_net_io = net_io
        self.last_net_time = time.time()

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
            # Silent mode - no print to avoid console popup
            return True
        except Exception as e:
            return False

    def run(self):
        """Main loop"""
        print("PC Monitor starting...")
        print("Searching for ESP32...")

        self.serial_port = self.find_esp32()
        if not self.serial_port:
            print("\nPlease connect ESP32 via USB and try again!")
            sys.exit(1)

        print("\nConnected! Sending data every second...")
        print("Press Ctrl+C to stop\n")

        try:
            while True:
                if not self.send_data(self.serial_port):
                    print("Lost connection, trying to reconnect...")
                    self.serial_port = self.find_esp32()
                    if not self.serial_port:
                        break

                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\nStopping...")
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            print("Closed connection")


if __name__ == "__main__":
    monitor = PCMonitor()
    monitor.run()
