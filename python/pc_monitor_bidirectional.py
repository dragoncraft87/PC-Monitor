#!/usr/bin/env python3
"""
PC Monitor - Bidirektional (sendet UND empfängt Logs)
Für ESP32-S3 USB Serial JTAG (COM4)
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
    print("Warning: GPUtil not installed. GPU monitoring disabled.")
    print("Install with: pip install gputil")

try:
    import wmi
    WMI_AVAILABLE = True
except ImportError:
    WMI_AVAILABLE = False


class PCMonitor:
    def __init__(self, port=None):
        self.serial_port = None
        self.wmi_interface = wmi.WMI() if WMI_AVAILABLE else None
        self.running = True
        self.specified_port = port

    def find_esp32(self):
        """ESP32 USB Serial JTAG Port finden (typischerweise COM4)"""
        ports = serial.tools.list_ports.comports()

        # Wenn Port spezifiziert wurde, versuche den direkt
        if self.specified_port:
            print(f"🔍 Trying specified port: {self.specified_port}")
            try:
                ser = serial.Serial(self.specified_port, 115200, timeout=0.1)
                print(f"✅ Connected to {self.specified_port}")
                return ser
            except Exception as e:
                print(f"❌ Could not connect to {self.specified_port}: {e}")
                return None

        # Auto-Detect: Suche nach USB Serial JTAG (ESP32-S3)
        for port in ports:
            # ESP32-S3 USB Serial JTAG zeigt sich als "USB Serial Device" oder "USB-SERIAL"
            if "USB" in port.description.upper() and "SERIAL" in port.description.upper():
                # NICHT CH343 (das ist der externe Chip)
                if "CH343" not in port.description:
                    print(f"🔍 Found ESP32 USB Serial JTAG at {port.device}: {port.description}")
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
        print("\n💡 Tip: ESP32-S3 USB Serial JTAG is usually 'Serielles USB-Gerät' (COM4)")
        print("        NOT the 'CH343' device!")
        return None

    def get_cpu_stats(self):
        """CPU Auslastung und Temperatur"""
        cpu_percent = int(psutil.cpu_percent(interval=0.1))

        # CPU Temperatur (Windows)
        cpu_temp = 60.0  # Default
        if WMI_AVAILABLE:
            try:
                temps = self.wmi_interface.MSAcpi_ThermalZoneTemperature()
                if temps:
                    cpu_temp = (temps[0].CurrentTemperature / 10.0) - 273.15
            except:
                pass

        return cpu_percent, cpu_temp

    def get_gpu_stats(self):
        """GPU Auslastung, Temperatur und VRAM"""
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
        """RAM Nutzung"""
        ram = psutil.virtual_memory()
        ram_used_gb = ram.used / (1024**3)
        ram_total_gb = ram.total / (1024**3)
        return ram_used_gb, ram_total_gb

    def get_network_stats(self):
        """Netzwerk Status und Traffic"""
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
        """Thread: Sendet Daten jede Sekunde"""
        print("📤 TX Thread started - sending data every second...")

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
                print(f"📤 TX: CPU={cpu_percent}% RAM={ram_used:.1f}GB", end='\r')

            except Exception as e:
                print(f"\n❌ TX Error: {e}")
                self.running = False
                break

            time.sleep(1)

    def receive_log_thread(self):
        """Thread: Empfängt und zeigt ESP32 Logs"""
        print("📥 RX Thread started - showing ESP32 logs...\n")

        buffer = ""
        while self.running:
            try:
                if self.serial_port.in_waiting > 0:
                    chunk = self.serial_port.read(self.serial_port.in_waiting).decode('utf-8', errors='ignore')
                    buffer += chunk

                    # Zeile für Zeile ausgeben
                    while '\n' in buffer:
                        line, buffer = buffer.split('\n', 1)
                        if line.strip():
                            # ESP32 Logs mit Prefix anzeigen
                            print(f"\n[ESP32] {line}")

            except Exception as e:
                if self.running:  # Nur wenn nicht absichtlich gestoppt
                    print(f"\n❌ RX Error: {e}")
                break

            time.sleep(0.01)  # 10ms

    def run(self):
        """Hauptschleife"""
        print("=" * 60)
        print("🚀 PC Monitor - Bidirectional Mode")
        print("=" * 60)
        print("🔍 Searching for ESP32 USB Serial JTAG...")

        self.serial_port = self.find_esp32()
        if not self.serial_port:
            print("\n❌ Please connect ESP32-S3 via USB and try again!")
            print("💡 Make sure to use the USB port labeled 'USB' (not 'UART')")
            sys.exit(1)

        print("\n✅ Connected!")
        print("=" * 60)
        print("📤 Sending PC stats every second...")
        print("📥 Showing ESP32 logs below...")
        print("Press Ctrl+C to stop\n")
        print("=" * 60)

        # Starte beide Threads
        tx_thread = threading.Thread(target=self.send_data_thread, daemon=True)
        rx_thread = threading.Thread(target=self.receive_log_thread, daemon=True)

        tx_thread.start()
        rx_thread.start()

        try:
            # Warte auf Threads
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n\n👋 Stopping...")
            self.running = False
        finally:
            if self.serial_port and self.serial_port.is_open:
                self.serial_port.close()
            print("✅ Closed connection")


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description='PC Monitor - Bidirectional')
    parser.add_argument('--port', type=str, help='COM port (e.g., COM4)', default=None)
    args = parser.parse_args()

    monitor = PCMonitor(port=args.port)
    monitor.run()
