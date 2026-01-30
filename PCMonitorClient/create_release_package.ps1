# Scarab Monitor - Desert Spec Release Builder
# Erstellt ein portables, versioniertes ZIP-Paket

$ErrorActionPreference = "Stop"
$ScriptPath = $PSScriptRoot
$ProjectPath = Join-Path $ScriptPath "PCMonitorClient"
$DistPath = Join-Path $ScriptPath "Dist"
$Timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm"
$ZipName = "Scarab_Monitor_v2.2_$Timestamp.zip"

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

Write-Host ">>> [4/5] Erstelle README..." -ForegroundColor Cyan
$ReadmeContent = @"
SCARAB MONITOR - DESERT SPEC EDITION
Version: v2.2 (Build $Timestamp)
------------------------------------

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