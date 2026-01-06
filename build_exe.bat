@echo off
REM ============================================================================
REM PC Monitor Tray - EXE Builder
REM Erstellt eine standalone EXE mit PyInstaller
REM ============================================================================

echo ========================================
echo    PC Monitor Tray - EXE Builder
echo ========================================
echo.

REM Prüfe ob PyInstaller installiert ist
python -m PyInstaller --version >nul 2>&1
if errorlevel 1 (
    echo [INFO] PyInstaller nicht gefunden, installiere...
    pip install pyinstaller
    echo.
)

REM Prüfe ob Dependencies installiert sind
echo [INFO] Installiere Dependencies...
pip install -r requirements_tray.txt
echo.

REM Erstelle Icon (falls nicht vorhanden)
if not exist "icon.ico" (
    echo [INFO] Erstelle icon.ico...
    python create_icon.py
    echo.
)

REM Build EXE
echo [INFO] Erstelle EXE mit PyInstaller...
echo.

pyinstaller --name="PC Monitor Tray" ^
    --onefile ^
    --windowed ^
    --icon=icon.ico ^
    --add-data="start_pc_monitor.bat;." ^
    --add-data="python/pc_monitor_bidirectional.pyw;python" ^
    --hidden-import=pystray ^
    --hidden-import=PIL ^
    --hidden-import=tkinter ^
    --clean ^
    pc_monitor_tray.py

echo.
if exist "dist\PC Monitor Tray.exe" (
    echo ========================================
    echo    BUILD ERFOLGREICH!
    echo ========================================
    echo.
    echo Die EXE befindet sich in: dist\PC Monitor Tray.exe
    echo.
    echo Du kannst sie nun kopieren und ausfuehren!
    echo.
) else (
    echo ========================================
    echo    BUILD FEHLGESCHLAGEN!
    echo ========================================
    echo Bitte pruefe die Fehler oben.
)

pause
