#!/usr/bin/env python3
"""
PC Monitor - Sendet PC-Stats an ESP32 über USB Serial
Für 4x GC9A01 Display Setup
"""

import time
import psutil
import serial
import serial.tools.list_ports
import sys

try:
    import GPUtil
    GPU_AVAILABLE = True
except ImportError:
    GPU_AVAILABLE = False
    print("Warning: GPUtil not installed. GPU monitoring disabled.")
    print("Install with: pip install gputil")

try:
    import wmi
    WMI_AVAILABLE = True
except ImportError:
    WMI_AVAILABLE = False
    print("Warning: WMI not installed. Advanced features disabled.")
    print("Install with: pip install wmi")


class PCMonitor:
    def __init__(self):
        self.serial_port = None
        self.wmi_interface = wmi.WMI() if WMI_AVAILABLE else None
        
    def find_esp32(self):
        """Automatisch ESP32 USB Port finden"""
        ports = serial.tools.list_ports.comports()
        for port in ports:
            # ESP32-S3 zeigt sich typischerweise als "USB Serial Device"
            if "USB" in port.description or "Serial" in port.description:
                print(f"Found possible ESP32 at {port.device}: {port.description}")
                try:
                    ser = serial.Serial(port.device, 115200, timeout=1)
                    print(f"✅ Connected to {port.device}")
                    return ser
                except Exception as e:
                    print(f"❌ Could not connect to {port.device}: {e}")
        
        print("❌ ESP32 not found!")
        print("\nAvailable ports:")
        for port in ports:
            print(f"  {port.device}: {port.description}")
        return None
    
    def get_cpu_stats(self):
        """CPU Auslastung und Temperatur"""
        cpu_percent = int(psutil.cpu_percent(interval=0.1))
        
        # CPU Temperatur (Windows)
        cpu_temp = 0.0
        if WMI_AVAILABLE:
            try:
                temps = self.wmi_interface.MSAcpi_ThermalZoneTemperature()
                if temps:
                    # Konvertiere von Kelvin * 10 zu Celsius
                    cpu_temp = (temps[0].CurrentTemperature / 10.0) - 273.15
            except:
                cpu_temp = 60.0  # Fallback
        else:
            cpu_temp = 60.0  # Fallback wenn WMI nicht verfügbar
        
        return cpu_percent, cpu_temp
    
    def get_gpu_stats(self):
        """GPU Auslastung, Temperatur und VRAM"""
        if not GPU_AVAILABLE:
            return 0, 0.0, 0.0, 8.0
        
        try:
            gpus = GPUtil.getGPUs()
            if gpus:
                gpu = gpus[0]  # Erste GPU
                gpu_percent = int(gpu.load * 100)
                gpu_temp = gpu.temperature
                vram_used = gpu.memoryUsed / 1024  # MB to GB
                vram_total = gpu.memoryTotal / 1024
                return gpu_percent, gpu_temp, vram_used, vram_total
        except:
            pass
        
        return 0, 0.0, 0.0, 8.0
    
    def get_ram_stats(self):
        """RAM Nutzung"""
        ram = psutil.virtual_memory()
        ram_used_gb = ram.used / (1024**3)
        ram_total_gb = ram.total / (1024**3)
        return ram_used_gb, ram_total_gb
    
    def get_network_stats(self):
        """Netzwerk Status und Traffic"""
        # Connection Type
        net_type = "LAN"  # Default
        
        # Speed Detection (versuche aus Adapter-Info zu holen)
        net_speed = "1000 Mbps"  # Default
        
        # Network IO stats
        net_io = psutil.net_io_counters()
        
        # Berechne MB/s (Delta seit letztem Call)
        if not hasattr(self, 'last_net_io'):
            self.last_net_io = net_io
            self.last_net_time = time.time()
            return net_type, net_speed, 0.0, 0.0
        
        time_delta = time.time() - self.last_net_time
        bytes_sent_delta = net_io.bytes_sent - self.last_net_io.bytes_sent
        bytes_recv_delta = net_io.bytes_recv - self.last_net_io.bytes_recv
        
        # MB/s
        net_up_mbps = (bytes_sent_delta / time_delta) / (1024 * 1024)
        net_down_mbps = (bytes_recv_delta / time_delta) / (1024 * 1024)
        
        self.last_net_io = net_io
        self.last_net_time = time.time()
        
        return net_type, net_speed, net_down_mbps, net_up_mbps
    
    def send_data(self, ser):
        """Sammle alle Daten und sende sie"""
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
            print(f"📤 Sent: {data_str.strip()}")
            return True
        except Exception as e:
            print(f"❌ Error sending data: {e}")
            return False
    
    def run(self):
        """Hauptschleife"""
        print("🚀 PC Monitor starting...")
        print("🔍 Searching for ESP32...")
        
        self.serial_port = self.find_esp32()
        if not self.serial_port:
            print("\n❌ Please connect ESP32 and try again!")
            sys.exit(1)
        
        print("\n✅ Connected! Sending data every second...")
        print("Press Ctrl+C to stop\n")
        
        try:
            while True:
                if not self.send_data(self.serial_port):
                    print("⚠️ Lost connection, trying to reconnect...")
                    self.serial_port = self.find_esp32()
                    if not self.serial_port:
                        break
                
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n\n👋 Stopping...")
        finally:
            if self.serial_port:
                self.serial_port.close()


if __name__ == "__main__":
    monitor = PCMonitor()
    monitor.run()
