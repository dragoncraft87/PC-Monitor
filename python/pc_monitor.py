#!/usr/bin/env python3
"""
PC Monitor - Desert-Spec Phase 2 Edition
For ESP32-S3 USB Serial JTAG

Features:
- Infinite reconnect loop (never crashes)
- CLI arguments: --silent, --tray, --port
- LibreHardwareMonitor with WMI/psutil fallbacks
- Thread-safe for tray app integration
- Robust error handling and logging
- CRITICAL: flush() after every write to prevent data stalls

Usage:
    python pc_monitor.py              # Normal mode with console output
    python pc_monitor.py --silent     # Silent mode (no console)
    python pc_monitor.py --tray       # System tray mode (minimized)
    python pc_monitor.py --port COM4  # Specify port manually
"""

import time
import sys
import os
import threading
import argparse
import logging
from datetime import datetime

# Setup logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%H:%M:%S'
)
logger = logging.getLogger('PCMonitor')

# Optional imports with graceful fallbacks
try:
    import psutil
    PSUTIL_AVAILABLE = True
except ImportError:
    PSUTIL_AVAILABLE = False
    logger.warning("psutil not available - limited functionality")

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False
    logger.error("pyserial not available - cannot communicate with ESP32")

try:
    import wmi
    WMI_AVAILABLE = True
except ImportError:
    WMI_AVAILABLE = False
    logger.debug("WMI not available (optional)")

# LibreHardwareMonitor support (requires admin rights)
OHM_AVAILABLE = False
Hardware = None


def init_libre_hardware_monitor():
    """Initialize LibreHardwareMonitor with proper error handling"""
    global OHM_AVAILABLE, Hardware

    try:
        import clr
        from pathlib import Path

        # Handle both regular script and PyInstaller bundle
        if getattr(sys, 'frozen', False):
            base_path = Path(sys._MEIPASS)
        else:
            base_path = Path(__file__).parent

        dll_path = base_path / "LibreHardwareMonitorLib.dll"
        if dll_path.exists():
            clr.AddReference(str(dll_path))
            from LibreHardwareMonitor import Hardware as HW
            Hardware = HW
            OHM_AVAILABLE = True
            logger.info("LibreHardwareMonitor loaded successfully")
        else:
            logger.warning(f"LibreHardwareMonitorLib.dll not found at {dll_path}")
    except ImportError as e:
        logger.warning(f"pythonnet not available: {e}")
    except Exception as e:
        logger.warning(f"Failed to load LibreHardwareMonitor: {e}")
        logger.warning("Running without hardware monitor - temperature readings may be limited")


class PCMonitor:
    """
    PC Hardware Monitor with infinite reconnect capability.

    This class monitors CPU, GPU, RAM, and Network statistics and sends them
    to an ESP32 over USB Serial. It handles disconnections gracefully and
    automatically reconnects.
    """

    # Reconnect settings
    RECONNECT_DELAY = 2.0       # Seconds between reconnect attempts
    MAX_RECONNECT_LOG = 10      # Log every N reconnect attempts (reduce spam)

    def __init__(self, port=None, silent=False, tray_mode=False, stop_event=None):
        """
        Initialize PC Monitor.

        Args:
            port: Specific COM port (e.g., "COM4") or None for auto-detect
            silent: If True, suppress all console output
            tray_mode: If True, run in background/tray mode
            stop_event: threading.Event() to signal shutdown
        """
        self.serial_port = None
        self.hardware = None
        self.wmi_interface = None
        self.running = True
        self.connected = False
        self.specified_port = port
        self.silent = silent or tray_mode
        self.tray_mode = tray_mode
        self.stop_event = stop_event or threading.Event()

        # Statistics
        self.packets_sent = 0
        self.reconnect_count = 0
        self.last_error = None

        # Network tracking
        self.last_net_io = None
        self.last_net_time = None

        # Threads
        self.tx_thread = None
        self.rx_thread = None
        self.reconnect_thread = None

        # Configure logging level based on mode
        if self.silent:
            logger.setLevel(logging.WARNING)

        # Initialize WMI (fallback for temperatures)
        if WMI_AVAILABLE:
            try:
                self.wmi_interface = wmi.WMI()
                logger.debug("WMI initialized")
            except Exception as e:
                logger.warning(f"WMI initialization failed: {e}")

        # Initialize LibreHardwareMonitor
        self._init_hardware_monitor()

    def _init_hardware_monitor(self):
        """Initialize LibreHardwareMonitor for accurate temperature readings"""
        if not OHM_AVAILABLE:
            return

        try:
            self.hardware = Hardware.Computer()
            self.hardware.IsCpuEnabled = True
            self.hardware.IsGpuEnabled = True
            self.hardware.Open()
            logger.info("Hardware monitor initialized (CPU + GPU)")
        except Exception as e:
            logger.warning(f"Hardware monitor init failed: {e}")
            logger.warning("Temperatures may be estimated - consider running as Administrator")
            self.hardware = None

    def log(self, msg, level=logging.INFO):
        """Log message if not in silent mode"""
        if not self.silent:
            logger.log(level, msg)

    def find_esp32(self):
        """
        Find and connect to ESP32 USB Serial JTAG port.

        Returns:
            serial.Serial object or None if not found
        """
        if not SERIAL_AVAILABLE:
            self.log("Serial library not available!", logging.ERROR)
            return None

        ports = list(serial.tools.list_ports.comports())

        # Try specified port first
        if self.specified_port:
            try:
                ser = serial.Serial(self.specified_port, 115200, timeout=0.1)
                self.log(f"Connected to specified port: {self.specified_port}")
                return ser
            except Exception as e:
                self.log(f"Failed to connect to {self.specified_port}: {e}", logging.WARNING)

        # Auto-detect ESP32 USB Serial JTAG
        for port in ports:
            desc_upper = port.description.upper()

            # Match ESP32 USB Serial JTAG patterns
            is_usb_serial = (
                ("USB" in desc_upper and "SERIAL" in desc_upper) or
                ("USB" in desc_upper and "SERIELL" in desc_upper) or
                ("SERIELLES USB" in desc_upper) or
                ("JTAG" in desc_upper)
            )

            # Exclude known non-ESP32 devices
            if is_usb_serial and "CH343" not in port.description:
                try:
                    ser = serial.Serial(port.device, 115200, timeout=0.1)
                    self.log(f"Connected to {port.device}: {port.description}")
                    return ser
                except Exception as e:
                    self.log(f"Failed to connect to {port.device}: {e}", logging.DEBUG)

        # Log available ports for debugging
        if ports:
            self.log("ESP32 not found. Available ports:", logging.DEBUG)
            for port in ports:
                self.log(f"  {port.device}: {port.description}", logging.DEBUG)

        return None

    # =========================================================================
    # SENSOR READING METHODS
    # =========================================================================

    def get_cpu_temp_ohm(self):
        """Get CPU temperature via LibreHardwareMonitor"""
        if not self.hardware:
            return None

        try:
            for hw in self.hardware.Hardware:
                if hw.HardwareType == Hardware.HardwareType.Cpu:
                    hw.Update()
                    for sensor in hw.Sensors:
                        if sensor.SensorType == Hardware.SensorType.Temperature:
                            name_lower = sensor.Name.lower()
                            if "package" in name_lower or "average" in name_lower or "core" in name_lower:
                                if sensor.Value is not None:
                                    return float(sensor.Value)
        except Exception as e:
            self.log(f"OHM CPU temp error: {e}", logging.DEBUG)

        return None

    def get_cpu_stats(self):
        """Get CPU usage and temperature with multiple fallbacks"""
        cpu_percent = 0
        cpu_temp = None

        # CPU usage via psutil
        if PSUTIL_AVAILABLE:
            try:
                cpu_percent = int(psutil.cpu_percent(interval=0.1))
            except Exception:
                pass

        # Temperature: Try LibreHardwareMonitor first
        cpu_temp = self.get_cpu_temp_ohm()

        # Fallback: WMI thermal zone
        if cpu_temp is None and WMI_AVAILABLE and self.wmi_interface:
            try:
                temps = self.wmi_interface.MSAcpi_ThermalZoneTemperature()
                if temps:
                    # Convert from decikelvin to Celsius
                    cpu_temp = (temps[0].CurrentTemperature / 10.0) - 273.15
            except Exception:
                pass

        # Fallback: psutil sensors (Linux/Mac)
        if cpu_temp is None and PSUTIL_AVAILABLE:
            try:
                temps = psutil.sensors_temperatures()
                if temps:
                    for name in ['coretemp', 'k10temp', 'cpu-thermal', 'cpu_thermal']:
                        if name in temps and temps[name]:
                            cpu_temp = temps[name][0].current
                            break
            except Exception:
                pass

        # Last resort: Estimate based on load
        if cpu_temp is None:
            cpu_temp = 40.0 + (cpu_percent * 0.4)  # Rough estimate

        return cpu_percent, cpu_temp

    def get_gpu_stats(self):
        """Get GPU usage, temperature and VRAM via LibreHardwareMonitor"""
        gpu_percent = 0
        gpu_temp = 0.0
        vram_used = 0.0
        vram_total = 12.0  # Default for RTX 3080 Ti

        if not self.hardware:
            return gpu_percent, gpu_temp, vram_used, vram_total

        try:
            for hw in self.hardware.Hardware:
                # Check for any GPU type
                if hw.HardwareType in [Hardware.HardwareType.GpuNvidia,
                                       Hardware.HardwareType.GpuAmd,
                                       Hardware.HardwareType.GpuIntel]:
                    hw.Update()

                    for sensor in hw.Sensors:
                        name_lower = sensor.Name.lower()

                        # GPU Load/Usage
                        if sensor.SensorType == Hardware.SensorType.Load:
                            if "core" in name_lower or "gpu" in name_lower:
                                if sensor.Value is not None:
                                    gpu_percent = int(sensor.Value)

                        # GPU Temperature
                        elif sensor.SensorType == Hardware.SensorType.Temperature:
                            if "core" in name_lower or "gpu" in name_lower or "hot spot" in name_lower:
                                if sensor.Value is not None:
                                    gpu_temp = float(sensor.Value)

                        # VRAM Usage (Data type, in GB)
                        elif sensor.SensorType == Hardware.SensorType.Data:
                            if "memory used" in name_lower or "used" in name_lower:
                                if sensor.Value is not None:
                                    vram_used = float(sensor.Value)
                            elif "memory total" in name_lower or "total" in name_lower:
                                if sensor.Value is not None:
                                    vram_total = float(sensor.Value)

                        # VRAM in SmallData type (MB -> GB conversion needed)
                        elif sensor.SensorType == Hardware.SensorType.SmallData:
                            if "memory used" in name_lower:
                                if sensor.Value is not None:
                                    vram_used = float(sensor.Value) / 1024.0
                            elif "memory total" in name_lower:
                                if sensor.Value is not None:
                                    vram_total = float(sensor.Value) / 1024.0

                    break  # Found GPU, stop searching

        except Exception as e:
            self.log(f"GPU stats error: {e}", logging.DEBUG)

        return gpu_percent, gpu_temp, vram_used, vram_total

    def get_ram_stats(self):
        """Get RAM usage in GB"""
        if not PSUTIL_AVAILABLE:
            return 0.0, 64.0

        try:
            ram = psutil.virtual_memory()
            ram_used_gb = ram.used / (1024**3)
            ram_total_gb = ram.total / (1024**3)
            return ram_used_gb, ram_total_gb
        except Exception:
            return 0.0, 64.0

    def get_network_stats(self):
        """Get network type and traffic statistics"""
        net_type = "LAN"
        net_speed = "1000 Mbps"
        net_down_mbps = 0.0
        net_up_mbps = 0.0

        if not PSUTIL_AVAILABLE:
            return net_type, net_speed, net_down_mbps, net_up_mbps

        try:
            net_io = psutil.net_io_counters()

            # First call - initialize baseline
            if self.last_net_io is None:
                self.last_net_io = net_io
                self.last_net_time = time.time()
                return net_type, net_speed, 0.0, 0.0

            # Calculate delta
            time_delta = time.time() - self.last_net_time
            if time_delta > 0:
                bytes_sent_delta = net_io.bytes_sent - self.last_net_io.bytes_sent
                bytes_recv_delta = net_io.bytes_recv - self.last_net_io.bytes_recv

                # Convert to MB/s
                net_up_mbps = (bytes_sent_delta / time_delta) / (1024 * 1024)
                net_down_mbps = (bytes_recv_delta / time_delta) / (1024 * 1024)

            self.last_net_io = net_io
            self.last_net_time = time.time()

        except Exception as e:
            self.log(f"Network stats error: {e}", logging.DEBUG)

        return net_type, net_speed, net_down_mbps, net_up_mbps

    # =========================================================================
    # DATA RECEIVE THREAD (for ESP32 logs)
    # =========================================================================

    def receive_log_thread(self):
        """Thread: Receives and logs messages from ESP32"""
        self.log("RX Thread started")

        while self.running and not self.stop_event.is_set():
            if not self.connected or not self.serial_port:
                time.sleep(0.5)
                continue

            try:
                if self.serial_port.in_waiting > 0:
                    line = self.serial_port.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        self.log(f"[ESP32] {line}", logging.DEBUG)
            except serial.SerialException:
                self.connected = False
            except Exception as e:
                self.log(f"RX error: {e}", logging.DEBUG)

            time.sleep(0.1)

        self.log("RX Thread stopped")

    # =========================================================================
    # DATA TRANSMISSION THREAD
    # =========================================================================

    def send_data_thread(self):
        """Thread: Sends PC stats every second with error recovery"""
        self.log("TX Thread started")

        while self.running and not self.stop_event.is_set():
            if not self.connected or not self.serial_port:
                time.sleep(0.5)
                continue

            try:
                # Gather all stats
                cpu_percent, cpu_temp = self.get_cpu_stats()
                gpu_percent, gpu_temp, vram_used, vram_total = self.get_gpu_stats()
                ram_used, ram_total = self.get_ram_stats()
                net_type, net_speed, net_down, net_up = self.get_network_stats()

                # Format data string
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

                # Send to ESP32 with CRITICAL flush()
                if self.serial_port and self.serial_port.is_open:
                    self.serial_port.write(data_str.encode('utf-8'))
                    self.serial_port.flush()  # DESERT-SPEC: Prevents data stalls!
                    self.packets_sent += 1

                # Debug output (not silent)
                if not self.silent and self.packets_sent % 5 == 0:
                    print(f"\rTX #{self.packets_sent}: CPU={cpu_percent}% ({cpu_temp:.0f}C) "
                          f"GPU={gpu_percent}% ({gpu_temp:.0f}C) "
                          f"RAM={ram_used:.1f}GB", end='', flush=True)

            except serial.SerialException as e:
                self.log(f"Serial error: {e}", logging.WARNING)
                self.connected = False
                self.last_error = str(e)
                # Close port for clean reconnect
                try:
                    self.serial_port.close()
                except Exception:
                    pass

            except Exception as e:
                self.log(f"TX error: {e}", logging.WARNING)
                self.last_error = str(e)

            time.sleep(1)

        self.log("TX Thread stopped")

    # =========================================================================
    # RECONNECTION THREAD
    # =========================================================================

    def reconnect_thread_func(self):
        """Thread: Handles automatic reconnection"""
        self.log("Reconnect Thread started")

        while self.running and not self.stop_event.is_set():
            # Check if we need to reconnect
            if not self.connected:
                self.reconnect_count += 1

                # Log reconnect attempts (but not spam)
                if self.reconnect_count <= 3 or self.reconnect_count % self.MAX_RECONNECT_LOG == 0:
                    self.log(f"Attempting to reconnect... (attempt #{self.reconnect_count})")

                # Close old connection if exists
                if self.serial_port:
                    try:
                        self.serial_port.close()
                    except Exception:
                        pass
                    self.serial_port = None

                # Try to reconnect
                self.serial_port = self.find_esp32()

                if self.serial_port:
                    self.connected = True
                    self.log(f"Reconnected successfully after {self.reconnect_count} attempts!")
                    self.reconnect_count = 0
                else:
                    time.sleep(self.RECONNECT_DELAY)
            else:
                time.sleep(1)

        self.log("Reconnect Thread stopped")

    # =========================================================================
    # MAIN CONTROL METHODS
    # =========================================================================

    def start(self):
        """
        Start monitoring (non-blocking).

        Returns:
            True if initial connection succeeded, False otherwise
        """
        if not self.silent:
            print("=" * 60)
            print("PC Monitor - Desert-Spec Phase 2 Edition")
            print("=" * 60)

        # Initial connection attempt
        self.log("Searching for ESP32 USB Serial JTAG...")
        self.serial_port = self.find_esp32()

        if self.serial_port:
            self.connected = True
            self.log("Initial connection established!")
        else:
            self.log("ESP32 not found - will keep trying in background", logging.WARNING)
            self.connected = False

        # Start all threads
        self.running = True

        self.tx_thread = threading.Thread(target=self.send_data_thread, daemon=True, name="TX")
        self.rx_thread = threading.Thread(target=self.receive_log_thread, daemon=True, name="RX")
        self.reconnect_thread = threading.Thread(target=self.reconnect_thread_func, daemon=True, name="Reconnect")

        self.tx_thread.start()
        self.rx_thread.start()
        self.reconnect_thread.start()

        if not self.silent:
            print("=" * 60)
            print("Monitoring started. Press Ctrl+C to stop.")
            print("=" * 60)

        return self.connected

    def stop(self):
        """Stop monitoring and clean up resources"""
        self.log("Stopping PC Monitor...")
        self.running = False
        self.stop_event.set()

        # Wait for threads to finish
        for thread, name in [(self.tx_thread, "TX"),
                             (self.rx_thread, "RX"),
                             (self.reconnect_thread, "Reconnect")]:
            if thread and thread.is_alive():
                thread.join(timeout=2)
                if thread.is_alive():
                    self.log(f"{name} thread did not stop cleanly", logging.WARNING)

        # Close serial port
        if self.serial_port:
            try:
                self.serial_port.close()
            except Exception:
                pass

        # Close hardware monitor
        if self.hardware:
            try:
                self.hardware.Close()
            except Exception:
                pass

        self.log(f"Stopped. Total packets sent: {self.packets_sent}")

    def run(self):
        """
        Main loop (blocking).

        Call this for standalone usage. The method blocks until stopped.
        """
        self.start()

        try:
            while self.running and not self.stop_event.is_set():
                time.sleep(0.5)
        except KeyboardInterrupt:
            print("\n\nShutdown requested...")
        finally:
            self.stop()

    def get_status(self):
        """
        Get current monitor status (for tray app).

        Returns:
            dict with connection status, packets sent, etc.
        """
        return {
            'connected': self.connected,
            'packets_sent': self.packets_sent,
            'reconnect_count': self.reconnect_count,
            'last_error': self.last_error,
            'port': self.serial_port.port if self.serial_port else None
        }


# =============================================================================
# STANDALONE ENTRY POINT
# =============================================================================

def run_monitor(stop_event=None, port=None, silent=True):
    """
    Run monitor in a thread (for tray app integration).

    Args:
        stop_event: threading.Event() to signal stop
        port: Specific COM port or None for auto-detect
        silent: Suppress console output
    """
    monitor = PCMonitor(port=port, silent=silent, stop_event=stop_event)
    monitor.run()


def main():
    """Main entry point with CLI argument parsing"""
    parser = argparse.ArgumentParser(
        description='PC Monitor - Desert-Spec Phase 2 Edition',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    python pc_monitor.py              # Normal mode
    python pc_monitor.py --silent     # No console output
    python pc_monitor.py --tray       # System tray mode
    python pc_monitor.py --port COM4  # Specific port

Note: Run as Administrator for accurate temperature readings.
        """
    )

    parser.add_argument(
        '--port', '-p',
        type=str,
        default=None,
        help='COM port (e.g., COM4). Auto-detect if not specified.'
    )

    parser.add_argument(
        '--silent', '-s',
        action='store_true',
        help='Silent mode - no console output'
    )

    parser.add_argument(
        '--tray', '-t',
        action='store_true',
        help='System tray mode - run minimized without console'
    )

    parser.add_argument(
        '--debug', '-d',
        action='store_true',
        help='Enable debug logging'
    )

    args = parser.parse_args()

    # Configure debug logging
    if args.debug:
        logger.setLevel(logging.DEBUG)

    # Initialize LibreHardwareMonitor
    init_libre_hardware_monitor()

    # Check for required libraries
    if not SERIAL_AVAILABLE:
        print("ERROR: pyserial is required. Install with: pip install pyserial")
        sys.exit(1)

    if not PSUTIL_AVAILABLE:
        print("WARNING: psutil not available. Install with: pip install psutil")

    # Create and run monitor
    monitor = PCMonitor(
        port=args.port,
        silent=args.silent,
        tray_mode=args.tray
    )

    monitor.run()


if __name__ == "__main__":
    main()
