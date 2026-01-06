#!/usr/bin/env python3
"""
PC Monitor - System Tray Application
Verwaltet PC Monitor und OpenHardwareMonitor via Taskleisten-Icon
"""

import os
import sys
import subprocess
import json
import threading
import time
from pathlib import Path
import pystray
from PIL import Image, ImageDraw
import tkinter as tk
from tkinter import filedialog, messagebox

# Konfigurationsdatei
CONFIG_FILE = Path(__file__).parent / "pc_monitor_config.json"
DEFAULT_OHM_PATHS = [
    r"C:\Program Files\OpenHardwareMonitor\OpenHardwareMonitor.exe",
    r"C:\Program Files (x86)\OpenHardwareMonitor\OpenHardwareMonitor.exe",
]


class PCMonitorTray:
    def __init__(self):
        self.icon = None
        self.monitor_process = None
        self.ohm_process = None
        self.running = False
        self.config = self.load_config()

        # Batch-Script Pfad
        self.batch_script = Path(__file__).parent / "start_pc_monitor.bat"

    def load_config(self):
        """Lädt Konfiguration aus JSON"""
        if CONFIG_FILE.exists():
            try:
                with open(CONFIG_FILE, 'r', encoding='utf-8') as f:
                    return json.load(f)
            except:
                pass

        # Default-Konfiguration
        default_config = {
            "ohm_path": self.find_ohm_path(),
            "auto_start": False
        }
        self.save_config(default_config)
        return default_config

    def save_config(self, config=None):
        """Speichert Konfiguration"""
        if config:
            self.config = config

        with open(CONFIG_FILE, 'w', encoding='utf-8') as f:
            json.dump(self.config, f, indent=4)

    def find_ohm_path(self):
        """Sucht OpenHardwareMonitor.exe"""
        for path in DEFAULT_OHM_PATHS:
            if os.path.exists(path):
                return path
        return ""

    def create_icon_image(self):
        """Erstellt Taskleisten-Icon (grün=aktiv, rot=inaktiv)"""
        width = 64
        height = 64
        color = (0, 255, 0) if self.running else (255, 0, 0)

        image = Image.new('RGB', (width, height), (0, 0, 0))
        dc = ImageDraw.Draw(image)

        # Einfaches "M" für Monitor
        dc.rectangle([10, 15, 25, 50], fill=color)
        dc.rectangle([40, 15, 55, 50], fill=color)
        dc.polygon([25, 15, 32, 30, 40, 15], fill=color)

        return image

    def update_icon(self):
        """Aktualisiert Icon-Farbe"""
        if self.icon:
            self.icon.icon = self.create_icon_image()

    def configure_ohm_path(self, icon, item):
        """Dialog zum Setzen des OpenHardwareMonitor Pfads"""
        root = tk.Tk()
        root.withdraw()  # Verstecke Haupt-Fenster

        file_path = filedialog.askopenfilename(
            title="OpenHardwareMonitor.exe auswählen",
            initialdir=self.config.get("ohm_path", "C:\\Program Files"),
            filetypes=[("Executable", "*.exe"), ("All files", "*.*")]
        )

        if file_path:
            self.config["ohm_path"] = file_path
            self.save_config()
            messagebox.showinfo("Erfolg", f"Pfad gespeichert:\n{file_path}")

        root.destroy()

    def start_monitoring(self, icon=None, item=None):
        """Startet OpenHardwareMonitor + PC Monitor Script"""
        if self.running:
            if icon:
                messagebox.showwarning("Warnung", "PC Monitor läuft bereits!")
            return

        try:
            # 1. Starte OpenHardwareMonitor (falls konfiguriert)
            ohm_path = self.config.get("ohm_path", "")
            if ohm_path and os.path.exists(ohm_path):
                # Prüfe ob bereits läuft
                result = subprocess.run(
                    ['tasklist', '/FI', 'IMAGENAME eq OpenHardwareMonitor.exe'],
                    capture_output=True,
                    text=True
                )

                if "OpenHardwareMonitor.exe" not in result.stdout:
                    self.ohm_process = subprocess.Popen(
                        [ohm_path],
                        creationflags=subprocess.CREATE_NO_WINDOW
                    )
                    time.sleep(2)  # Warte auf Initialisierung

            # 2. Starte PC Monitor Script via Batch
            if self.batch_script.exists():
                self.monitor_process = subprocess.Popen(
                    [str(self.batch_script)],
                    creationflags=subprocess.CREATE_NO_WINDOW,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE
                )
            else:
                # Fallback: Direkt Python-Script starten
                pyw_script = Path(__file__).parent / "python" / "pc_monitor_bidirectional.pyw"
                if pyw_script.exists():
                    self.monitor_process = subprocess.Popen(
                        ["pythonw", str(pyw_script)],
                        creationflags=subprocess.CREATE_NO_WINDOW
                    )

            self.running = True
            self.update_icon()

            if icon:
                messagebox.showinfo("Erfolg", "PC Monitor gestartet!")

        except Exception as e:
            if icon:
                messagebox.showerror("Fehler", f"Konnte nicht starten:\n{e}")

    def stop_monitoring(self, icon=None, item=None):
        """Stoppt PC Monitor (und optional OpenHardwareMonitor)"""
        if not self.running:
            if icon:
                messagebox.showwarning("Warnung", "PC Monitor läuft nicht!")
            return

        try:
            # Stoppe Monitor-Process
            if self.monitor_process:
                self.monitor_process.terminate()
                try:
                    self.monitor_process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.monitor_process.kill()
                self.monitor_process = None

            # Optional: OpenHardwareMonitor lassen wir weiterlaufen
            # (kann manuell geschlossen werden)

            self.running = False
            self.update_icon()

            if icon:
                messagebox.showinfo("Erfolg", "PC Monitor gestoppt!")

        except Exception as e:
            if icon:
                messagebox.showerror("Fehler", f"Konnte nicht stoppen:\n{e}")

    def quit_app(self, icon, item):
        """Beendet alles und schließt die App"""
        # Stoppe Monitor
        if self.running:
            self.stop_monitoring()

        # Stoppe Tray-Icon
        if icon:
            icon.stop()

        sys.exit(0)

    def create_menu(self):
        """Erstellt Rechtsklick-Menü"""
        return pystray.Menu(
            pystray.MenuItem(
                "OpenHardwareMonitor Pfad definieren",
                self.configure_ohm_path
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "Start",
                self.start_monitoring,
                enabled=lambda item: not self.running
            ),
            pystray.MenuItem(
                "Stop",
                self.stop_monitoring,
                enabled=lambda item: self.running
            ),
            pystray.Menu.SEPARATOR,
            pystray.MenuItem(
                "Alles beenden",
                self.quit_app
            )
        )

    def run(self):
        """Startet System Tray Icon"""
        # Erstelle Icon
        self.icon = pystray.Icon(
            "pc_monitor",
            self.create_icon_image(),
            "PC Monitor",
            self.create_menu()
        )

        # Starte Icon (blockiert)
        self.icon.run()


if __name__ == "__main__":
    app = PCMonitorTray()
    app.run()
