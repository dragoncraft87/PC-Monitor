# Scarab Monitor - Desert Spec Release Builder
# Erstellt ein portables, versioniertes ZIP-Paket

$ErrorActionPreference = "Stop"
$ScriptPath = $PSScriptRoot
$ProjectPath = Join-Path $ScriptPath "PCMonitorClient"
$DistPath = Join-Path $ScriptPath "Dist"
$Timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm"
$Version = "v2.4"
$ZipName = "Scarab_Monitor_${Version}_$Timestamp.zip"
# ESP32-Firmware-Image (idf.py build) - wird als firmware\ ins Paket gelegt,
# damit Geraete direkt aus der App heraus geflasht werden koennen
$FirmwareBin = Join-Path (Split-Path $ScriptPath -Parent) "build\pc-monitor-poc.bin"

Write-Host ">>> [1/5] Initialisiere Build-Umgebung..." -ForegroundColor Cyan
# Alte Builds bereinigen
if (Test-Path $DistPath) { Remove-Item $DistPath -Recurse -Force }
New-Item -ItemType Directory -Path $DistPath | Out-Null

Write-Host ">>> [2/5] Kompiliere im Release-Modus..." -ForegroundColor Cyan
# Restore und Build
dotnet restore "$ProjectPath\PCMonitorClient.csproj"
dotnet build "$ProjectPath\PCMonitorClient.csproj" -c Release -o "$DistPath\Scarab_Monitor"

if ($LASTEXITCODE -ne 0) {
    Write-Error "Build fehlgeschlagen!"
}

Write-Host ">>> [3/5] Bereinige Paket (Desert-Spec Cleanup)..." -ForegroundColor Cyan
# Unnötige Dateien entfernen (Debug-Symbole, XML-Doku)
Get-ChildItem "$DistPath\Scarab_Monitor" -Include *.pdb,*.xml -Recurse | Remove-Item

# Prüfen ob wichtige DLLs da sind
$CriticalFiles = @("PCMonitorClient.exe", "SixLabors.ImageSharp.dll", "LibreHardwareMonitorLib.dll", "install_autostart.ps1")
foreach ($file in $CriticalFiles) {
    if (-not (Test-Path "$DistPath\Scarab_Monitor\$file")) {
        Write-Warning "Kritische Datei fehlt: $file"
    }
}

# ESP32-Firmware beilegen (falls gebaut)
if (Test-Path $FirmwareBin) {
    New-Item -ItemType Directory -Path "$DistPath\Scarab_Monitor\firmware" | Out-Null
    Copy-Item $FirmwareBin "$DistPath\Scarab_Monitor\firmware\scarab_firmware_$Version.bin"
    Write-Host "    ESP32-Firmware beigelegt: firmware\scarab_firmware_$Version.bin" -ForegroundColor Green
} else {
    Write-Warning "ESP32-Firmware nicht gefunden ($FirmwareBin) - Paket ohne firmware\ erstellt. Vorher 'idf.py build' ausfuehren!"
}

Write-Host ">>> [4/5] Erstelle README..." -ForegroundColor Cyan
$ReadmeContent = @"
SCARAB MONITOR - DESERT SPEC EDITION
Version: $Version (Build $Timestamp)
------------------------------------

NEUERUNGEN v2.4:
- Firmware-Update direkt aus der App (Settings -> Firmware-Tab)
  Kein Kabel-Flashen mehr noetig - Image liegt unter firmware\ bei.
  ACHTUNG: Geraete mit Firmware < 2.4 brauchen EINMALIG ein Kabel-Update
  (neue OTA-Partitionstabelle, siehe Projekt-README).
- Uploads ueberleben verlorene ACKs (automatischer Resync)
- LittleFS-Daten (Bilder/Farben) ueberleben Firmware-Updates
- Hardware-Namen erscheinen sofort ohne Geraete-Neustart

NEUERUNGEN v2.3:
- Dynamische Screensaver-Hintergrundfarben pro Slot
- Transparente PNGs werden über konfigurierbare Farbe gerendert
- Farben bleiben nach Neustart erhalten (LittleFS)

NEUERUNGEN v2.2:
- SCARAB Bildformat mit Header und CRC32 Verifikation
- RGB565A8 PLANAR Format für LVGL v9
- Pause/Resume für manuelle Übertragungssteuerung

INSTALLATION:
1. Entpacken Sie diesen Ordner an einen beliebigen Ort (z.B. C:\Tools\Scarab).
2. Starten Sie 'install_autostart.ps1' als Administrator, um den Autostart einzurichten.

START:
- Starten Sie 'PCMonitorClient.exe'.
- Das Programm benötigt Administrator-Rechte für Hardware-Zugriff (LHM).

WICHTIG:
- Drag & Drop funktioniert im Admin-Modus nicht (Windows Sicherheitsrichtlinie).
- Klicken Sie auf die Kreise im Settings-Menü, um Bilder zu laden.
"@
Set-Content -Path "$DistPath\Scarab_Monitor\README.txt" -Value $ReadmeContent

Write-Host ">>> [5/5] Schnüre das Paket (ZIP)..." -ForegroundColor Cyan
$ZipPath = Join-Path ([Environment]::GetFolderPath("Desktop")) $ZipName
Compress-Archive -Path "$DistPath\Scarab_Monitor" -DestinationPath $ZipPath -Force

Write-Host "==========================================" -ForegroundColor Green
Write-Host " SUCCESS! Paket bereit auf dem Desktop:" -ForegroundColor Green
Write-Host " $ZipPath" -ForegroundColor White
Write-Host "==========================================" -ForegroundColor Green