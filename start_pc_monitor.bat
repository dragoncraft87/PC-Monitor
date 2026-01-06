@echo off
REM ============================================================================
REM PC Monitor Autostart Script (Batch Version)
REM Startet Open Hardware Monitor + Python Script
REM ============================================================================

echo ========================================
echo    PC Monitor - Autostart Script
echo ========================================
echo.

REM Pfade (passe diese bei Bedarf an!)
set "PYTHON_SCRIPT=%~dp0python\pc_monitor_bidirectional.pyw"
set "OHM_PATH=C:\Program Files\OpenHardwareMonitor\OpenHardwareMonitor.exe"

REM ========================================================================
REM STEP 1: Starte Open Hardware Monitor (falls nicht läuft)
REM ========================================================================
tasklist /FI "IMAGENAME eq OpenHardwareMonitor.exe" 2>NUL | find /I /N "OpenHardwareMonitor.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo [OK] Open Hardware Monitor laeuft bereits
) else (
    if exist "%OHM_PATH%" (
        echo [STARTING] Open Hardware Monitor...
        start "" /MIN "%OHM_PATH%"
        echo [OK] Open Hardware Monitor gestartet
        timeout /t 2 /nobreak >nul
    ) else (
        echo [WARNING] Open Hardware Monitor nicht gefunden!
        echo           Erwartet bei: %OHM_PATH%
        echo           Du kannst den Pfad im Script anpassen (Zeile 12)
        echo.
        pause
    )
)

REM ========================================================================
REM STEP 2: Starte Python Script
REM ========================================================================
echo [STARTING] Python PC Monitor Script...
echo.
echo ========================================
echo    System laeuft!
echo ========================================
echo Druecke Strg+C zum Beenden
echo.

pythonw "%PYTHON_SCRIPT%"

echo.
echo [INFO] PC Monitor Script beendet
echo [INFO] Open Hardware Monitor laeuft weiter im Hintergrund
echo.
pause
