# ============================================================================
# PC Monitor Autostart Script
# Startet Open Hardware Monitor + Python Script gleichzeitig
# ============================================================================

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   PC Monitor - Autostart Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Pfade (passe diese bei Bedarf an!)
$PYTHON_SCRIPT = "$PSScriptRoot\python\pc_monitor_bidirectional.pyw"
$OHM_PATH = "C:\Program Files\OpenHardwareMonitor\OpenHardwareMonitor.exe"

# Falls Open Hardware Monitor woanders installiert ist, suche alternative Pfade
if (-not (Test-Path $OHM_PATH)) {
    $OHM_ALT_PATHS = @(
        "C:\Program Files (x86)\OpenHardwareMonitor\OpenHardwareMonitor.exe",
        "$env:USERPROFILE\Downloads\OpenHardwareMonitor\OpenHardwareMonitor.exe",
        "$env:USERPROFILE\Desktop\OpenHardwareMonitor\OpenHardwareMonitor.exe"
    )

    foreach ($alt_path in $OHM_ALT_PATHS) {
        if (Test-Path $alt_path) {
            $OHM_PATH = $alt_path
            break
        }
    }
}

# ========================================================================
# STEP 1: Prüfe ob Open Hardware Monitor bereits läuft
# ========================================================================
$ohm_running = Get-Process -Name "OpenHardwareMonitor" -ErrorAction SilentlyContinue

if ($ohm_running) {
    Write-Host "[OK] Open Hardware Monitor läuft bereits" -ForegroundColor Green
} else {
    if (Test-Path $OHM_PATH) {
        Write-Host "[STARTING] Open Hardware Monitor..." -ForegroundColor Yellow
        Start-Process -FilePath $OHM_PATH -WindowStyle Minimized
        Write-Host "[OK] Open Hardware Monitor gestartet (minimiert)" -ForegroundColor Green
        Start-Sleep -Seconds 2  # Warte bis OHM initialisiert ist
    } else {
        Write-Host "[WARNING] Open Hardware Monitor nicht gefunden!" -ForegroundColor Red
        Write-Host "          Erwartet bei: $OHM_PATH" -ForegroundColor Yellow
        Write-Host "          Du kannst den Pfad im Script anpassen (Zeile 11)" -ForegroundColor Yellow
        Write-Host ""
        $continue = Read-Host "Trotzdem fortfahren? (j/n)"
        if ($continue -ne "j") {
            exit 1
        }
    }
}

# ========================================================================
# STEP 2: Starte Python Script
# ========================================================================
Write-Host "[STARTING] Python PC Monitor Script..." -ForegroundColor Yellow

if (-not (Test-Path $PYTHON_SCRIPT)) {
    Write-Host "[ERROR] Python Script nicht gefunden: $PYTHON_SCRIPT" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   System läuft!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Drücke Strg+C zum Beenden" -ForegroundColor Yellow
Write-Host ""

# Starte Python Script (blockiert bis es beendet wird)
try {
    pythonw $PYTHON_SCRIPT
} catch {
    Write-Host ""
    Write-Host "[ERROR] Python Script Fehler: $_" -ForegroundColor Red
} finally {
    Write-Host ""
    Write-Host "[INFO] PC Monitor Script beendet" -ForegroundColor Yellow
    Write-Host "[INFO] Open Hardware Monitor läuft weiter im Hintergrund" -ForegroundColor Yellow
    Write-Host ""
}
