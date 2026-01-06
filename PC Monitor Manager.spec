# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec file for PC Monitor Manager
# Builds standalone EXE with UAC admin rights

block_cipher = None

a = Analysis(
    ['pc_monitor_tray.py'],
    pathex=[],
    binaries=[],
    datas=[
        ('python/pc_monitor.py', 'python'),
        ('icon.ico', '.'),
    ],
    hiddenimports=[
        'pystray',
        'PIL',
        'PIL.Image',
        'PIL.ImageDraw',
        'winreg',
        'subprocess',
        'threading',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='PC Monitor Manager',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,  # No console window
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon='icon.ico',
    uac_admin=True,  # Request administrator privileges (required for LibreHardwareMonitor)
    uac_uiaccess=False,
)
