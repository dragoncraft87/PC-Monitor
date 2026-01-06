#!/usr/bin/env python3
"""
Erstellt ein einfaches Icon für die Tray-App
"""

from PIL import Image, ImageDraw

def create_icon():
    """Erstellt ein 256x256 Icon mit 'M' Symbol"""
    size = 256
    image = Image.new('RGB', (size, size), (30, 30, 30))
    dc = ImageDraw.Draw(image)

    # Zeichne großes "M" in der Mitte (grün)
    color = (0, 200, 100)

    # Linker Balken
    dc.rectangle([50, 60, 90, 200], fill=color)

    # Rechter Balken
    dc.rectangle([165, 60, 205, 200], fill=color)

    # Mittleres V
    dc.polygon([90, 60, 127, 120, 165, 60], fill=color)

    # Speichere als ICO
    image.save('icon.ico', format='ICO', sizes=[(256, 256), (128, 128), (64, 64), (32, 32), (16, 16)])
    print("[OK] icon.ico erstellt!")

if __name__ == "__main__":
    create_icon()
