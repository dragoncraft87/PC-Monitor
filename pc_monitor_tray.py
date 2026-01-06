#!/usr/bin/env python3
"""
PC Monitor Manager - System Tray Application
Manages PC Monitor script via taskbar icon
"""

import os
import sys
import subprocess
import winreg
from pathlib import Path
import pystray
from PIL import Image, ImageDraw
import threading
import time


class PCMonitorTray:
    def __init__(self):
        self.icon = None
        self.monitor_process = None
        self.running = False

        # Path to monitor script
        if getattr(sys, 'frozen', False):
            # Running as EXE - use bundled script
            base_path = Path(sys._MEIPASS)
            self.monitor_script = base_path / "python" / "pc_monitor.py"
        else:
            # Running as script
            self.monitor_script = Path(__file__).parent / "python" / "pc_monitor.py"

    def create_icon_image(self):
        """Create taskbar icon (green=running, red=stopped)"""
        width = 64
        height = 64
        color = (0, 255, 0) if self.running else (255, 0, 0)

        image = Image.new('RGB', (width, height), (0, 0, 0))
        dc = ImageDraw.Draw(image)

        # Simple "M" for Monitor
        dc.rectangle([10, 15, 25, 50], fill=color)
        dc.rectangle([40, 15, 55, 50], fill=color)
        dc.polygon([25, 15, 32, 30, 40, 15], fill=color)

        return image

    def update_icon(self):
        """Update icon color"""
        if self.icon:
            # Update existing icon instead of creating new one
            self.icon.icon = self.create_icon_image()
            # Force menu refresh to update enabled/disabled states
            if hasattr(self.icon, 'update_menu'):
                self.icon.update_menu()

    def start_monitoring(self, icon=None, item=None):
        """Start PC Monitor script"""
        if self.running:
            return

        try:
            if not self.monitor_script.exists():
                print(f"ERROR: Monitor script not found at {self.monitor_script}")
                return

            # Find pythonw.exe (for no console window)
            python_exe = sys.executable
            if python_exe.endswith('python.exe'):
                pythonw_exe = python_exe.replace('python.exe', 'pythonw.exe')
                if os.path.exists(pythonw_exe):
                    python_exe = pythonw_exe

            # Start monitor script with pythonw (no console window)
            startupinfo = subprocess.STARTUPINFO()
            startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startupinfo.wShowWindow = 0  # SW_HIDE

            self.monitor_process = subprocess.Popen(
                [python_exe, str(self.monitor_script)],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                stdin=subprocess.DEVNULL,
                startupinfo=startupinfo,
                creationflags=subprocess.CREATE_NO_WINDOW | subprocess.DETACHED_PROCESS
            )

            self.running = True
            self.update_icon()
            print("PC Monitor started")

        except Exception as e:
            print(f"ERROR: Failed to start monitoring: {e}")

    def stop_monitoring(self, icon=None, item=None):
        """Stop PC Monitor script"""
        if not self.running:
            return

        try:
            if self.monitor_process:
                self.monitor_process.terminate()
                try:
                    self.monitor_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.monitor_process.kill()
                self.monitor_process = None

            self.running = False
            self.update_icon()
            print("PC Monitor stopped")

        except Exception as e:
            print(f"ERROR: Failed to stop monitoring: {e}")

    def add_to_autostart(self, icon=None, item=None):
        """Add this app to Windows autostart"""
        try:
            # Get path to this executable/script
            if getattr(sys, 'frozen', False):
                # Running as EXE
                app_path = sys.executable
            else:
                # Running as script
                app_path = os.path.abspath(__file__)

            # Add to Windows registry (Run key)
            key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
            key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path, 0, winreg.KEY_SET_VALUE)
            winreg.SetValueEx(key, "PC Monitor Manager", 0, winreg.REG_SZ, f'"{app_path}"')
            winreg.CloseKey(key)

            print("Added to autostart successfully")

        except Exception as e:
            print(f"ERROR: Failed to add to autostart: {e}")

    def remove_from_autostart(self, icon=None, item=None):
        """Remove this app from Windows autostart"""
        try:
            key_path = r"Software\Microsoft\Windows\CurrentVersion\Run"
            key = winreg.OpenKey(winreg.HKEY_CURRENT_USER, key_path, 0, winreg.KEY_SET_VALUE)
            try:
                winreg.DeleteValue(key, "PC Monitor Manager")
                print("Removed from autostart successfully")
            except FileNotFoundError:
                print("Not in autostart")
            finally:
                winreg.CloseKey(key)

        except Exception as e:
            print(f"ERROR: Failed to remove from autostart: {e}")

    def quit_app(self, icon, item):
        """Stop monitoring and close app"""
        if self.running:
            self.stop_monitoring()

        if icon:
            icon.stop()

        sys.exit(0)

    def create_menu(self):
        """Create right-click menu"""
        return pystray.Menu(
            pystray.MenuItem(
                "Start Monitoring",
                self.start_monitoring,
                enabled=lambda item: not self.running,
                default=True
            ),
            pystray.MenuItem(
                "Stop Monitoring",
                self.stop_monitoring,
                enabled=lambda item: self.running
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "Add to Autostart",
                self.add_to_autostart
            ),
            pystray.MenuItem(
                "Remove from Autostart",
                self.remove_from_autostart
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "Quit",
                self.quit_app
            )
        )

    def monitor_process_thread(self):
        """Monitor the monitor process and update icon if it crashes"""
        while True:
            if self.running and self.monitor_process:
                if self.monitor_process.poll() is not None:
                    # Process exited
                    print("PC Monitor process exited unexpectedly")
                    self.running = False
                    self.update_icon()

            time.sleep(1)

    def run(self):
        """Start system tray icon"""
        # Start monitor thread
        monitor_thread = threading.Thread(target=self.monitor_process_thread, daemon=True)
        monitor_thread.start()

        # Create icon (only ONE icon)
        self.icon = pystray.Icon(
            "pc_monitor_manager",
            self.create_icon_image(),
            "PC Monitor Manager",
            self.create_menu()
        )

        # Run icon (blocking)
        self.icon.run()


if __name__ == "__main__":
    app = PCMonitorTray()
    app.run()
