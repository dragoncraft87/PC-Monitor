#Requires -Version 5.1
<#
.SYNOPSIS
    Creates a portable release package (ZIP) of Scarab Monitor for distribution.

.DESCRIPTION
    This script performs the following steps:
    1. Cleans old build artifacts (bin, obj folders)
    2. Builds the project in Release configuration
    3. Creates a staging folder with all required files
    4. Removes unnecessary files (*.pdb, *.xml)
    5. Packages everything into a ZIP file

.EXAMPLE
    .\create_release_package.ps1

.NOTES
    Version: 2.2
    Author: Scarab Monitor Team
#>

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# === CONFIGURATION ===
$Version = "2.2"
$ProductName = "Scarab_Monitor"
$ReleaseName = "${ProductName}_v${Version}"

# Aktueller Ort des Skripts
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# EXPLIZITE PFADLOGIK:
# Wir suchen die .csproj Datei im selben Ordner oder im Unterordner
$csprojPath = Get-ChildItem -Path $ScriptDir -Filter "PCMonitorClient.csproj" -Recurse | Select-Object -First 1 -ExpandProperty FullName

if (-not $csprojPath) {
    Write-Error "Projektdatei (.csproj) konnte nicht gefunden werden!"
    exit 1
}

# Das Projektverzeichnis ist dort, wo die .csproj liegt
$ProjectDir = Split-Path -Parent $csprojPath
$OutputDir = Join-Path $ProjectDir "bin\Release\net472"

# Das ZIP soll direkt dort landen, wo das SKRIPT liegt (nicht eine Ebene drüber)
$ZipOutputDir = $ScriptDir 
$ZipName = "${ReleaseName}.zip"
$ZipPath = Join-Path $ZipOutputDir $ZipName

# Staging Ordner (temporär)
$StagingDir = Join-Path $ScriptDir "STAGING_$ReleaseName"

# === FUNCTIONS ===

function Write-Step {
    param([string]$Message)
    Write-Host "`n[$([char]0x2192)] $Message" -ForegroundColor Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Write-Info {
    param([string]$Message)
    Write-Host "    $Message" -ForegroundColor Gray
}

# === MAIN SCRIPT ===

Write-Host ""
Write-Host "=======================================" -ForegroundColor Yellow
Write-Host "  Scarab Monitor Release Builder v$Version" -ForegroundColor Yellow
Write-Host "=======================================" -ForegroundColor Yellow

# --- Step 1: Cleanup ---
Write-Step "Cleaning old build artifacts..."

# Remove bin folder
$binPath = Join-Path $ProjectDir "bin"
if (Test-Path $binPath) {
    Remove-Item -Path $binPath -Recurse -Force
    Write-Info "Removed: bin/"
}

# Remove obj folder
$objPath = Join-Path $ProjectDir "obj"
if (Test-Path $objPath) {
    Remove-Item -Path $objPath -Recurse -Force
    Write-Info "Removed: obj/"
}

# Remove old staging folder
if (Test-Path $StagingDir) {
    Remove-Item -Path $StagingDir -Recurse -Force
    Write-Info "Removed: $ReleaseName/"
}

# Remove old ZIP files
$oldZips = Get-ChildItem -Path $ScriptDir -Filter "*.zip" -ErrorAction SilentlyContinue
foreach ($zip in $oldZips) {
    Remove-Item -Path $zip.FullName -Force
    Write-Info "Removed: $($zip.Name)"
}

# Also check output dir
if (Test-Path $ZipPath) {
    Remove-Item -Path $ZipPath -Force
    Write-Info "Removed: $ZipName (output dir)"
}

Write-Success "Cleanup complete"

# --- Step 2: Build ---
Write-Step "Building project in Release configuration..."

$csprojPath = Join-Path $ProjectDir "PCMonitorClient.csproj"
if (-not (Test-Path $csprojPath)) {
    Write-Error "Project file not found: $csprojPath"
    exit 1
}

# Run dotnet build
$buildArgs = @(
    "build",
    "`"$csprojPath`"",
    "-c", "Release",
    "--nologo",
    "-v", "minimal"
)

Write-Info "Running: dotnet $($buildArgs -join ' ')"

$buildProcess = Start-Process -FilePath "dotnet" -ArgumentList $buildArgs -Wait -PassThru -NoNewWindow
if ($buildProcess.ExitCode -ne 0) {
    Write-Error "Build failed with exit code $($buildProcess.ExitCode)"
    exit 1
}

# Verify output exists
if (-not (Test-Path $OutputDir)) {
    Write-Error "Build output not found: $OutputDir"
    exit 1
}

Write-Success "Build successful"

# --- Step 3: Staging ---
Write-Step "Creating staging folder..."

# Create staging directory
New-Item -ItemType Directory -Path $StagingDir -Force | Out-Null
Write-Info "Created: $ReleaseName/"

# Copy all files from Release output
Write-Info "Copying files from build output..."
Copy-Item -Path "$OutputDir\*" -Destination $StagingDir -Recurse -Force

# List of required files to verify
$requiredFiles = @(
    "PCMonitorClient.exe",
    "PCMonitorClient.exe.config",
    "LibreHardwareMonitorLib.dll",
    "HidSharp.dll",
    "SixLabors.ImageSharp.dll",
    "System.IO.Ports.dll",
    "icon.ico",
    "install_autostart.ps1"
)

# Verify required files
$missingFiles = @()
foreach ($file in $requiredFiles) {
    $filePath = Join-Path $StagingDir $file
    if (Test-Path $filePath) {
        Write-Info "  [+] $file"
    } else {
        $missingFiles += $file
        Write-Host "  [-] $file (MISSING!)" -ForegroundColor Red
    }
}

if ($missingFiles.Count -gt 0) {
    Write-Host ""
    Write-Host "WARNING: Some files are missing. The package may not work correctly." -ForegroundColor Yellow
    Write-Host "Missing: $($missingFiles -join ', ')" -ForegroundColor Yellow
}

# --- Step 4: Remove unnecessary files ---
Write-Step "Removing unnecessary files..."

# Remove PDB files (debug symbols)
$pdbFiles = Get-ChildItem -Path $StagingDir -Filter "*.pdb" -Recurse -ErrorAction SilentlyContinue
foreach ($pdb in $pdbFiles) {
    Remove-Item -Path $pdb.FullName -Force
    Write-Info "Removed: $($pdb.Name)"
}

# Remove XML documentation files
$xmlFiles = Get-ChildItem -Path $StagingDir -Filter "*.xml" -Recurse -ErrorAction SilentlyContinue
foreach ($xml in $xmlFiles) {
    # Keep config files, only remove documentation
    if ($xml.Name -notlike "*.exe.config" -and $xml.Name -notlike "*.dll.config") {
        Remove-Item -Path $xml.FullName -Force
        Write-Info "Removed: $($xml.Name)"
    }
}

# Remove any test/dev files
$devFiles = @("*.deps.json", "*.runtimeconfig.json", "*.runtimeconfig.dev.json")
foreach ($pattern in $devFiles) {
    $files = Get-ChildItem -Path $StagingDir -Filter $pattern -ErrorAction SilentlyContinue
    foreach ($file in $files) {
        Remove-Item -Path $file.FullName -Force
        Write-Info "Removed: $($file.Name)"
    }
}

# Remove crash logs (runtime artifacts)
$crashLogs = Get-ChildItem -Path $StagingDir -Filter "crash_log*.txt" -ErrorAction SilentlyContinue
foreach ($log in $crashLogs) {
    Remove-Item -Path $log.FullName -Force
    Write-Info "Removed: $($log.Name)"
}

# Remove libs subfolder (contains duplicate DLLs)
$libsPath = Join-Path $StagingDir "libs"
if (Test-Path $libsPath) {
    Remove-Item -Path $libsPath -Recurse -Force
    Write-Info "Removed: libs/ (duplicate DLLs)"
}

Write-Success "Cleanup complete"

# --- Step 5: Create ZIP ---
Write-Step "Creating ZIP package..."

# Calculate staging folder size
$folderSize = (Get-ChildItem -Path $StagingDir -Recurse | Measure-Object -Property Length -Sum).Sum
$folderSizeMB = [math]::Round($folderSize / 1MB, 2)
Write-Info "Staging folder size: $folderSizeMB MB"

# Create ZIP
Write-Info "Compressing to: $ZipPath"

try {
    # Use .NET compression (faster and more reliable than Compress-Archive for large files)
    Add-Type -Assembly "System.IO.Compression.FileSystem"

    # Remove existing ZIP if it exists
    if (Test-Path $ZipPath) {
        Remove-Item -Path $ZipPath -Force
    }

    [System.IO.Compression.ZipFile]::CreateFromDirectory(
        $StagingDir,
        $ZipPath,
        [System.IO.Compression.CompressionLevel]::Optimal,
        $true  # Include base directory name in ZIP
    )
}
catch {
    # Fallback to Compress-Archive
    Write-Info "Using fallback compression method..."
    Compress-Archive -Path $StagingDir -DestinationPath $ZipPath -Force
}

# Verify ZIP was created
if (-not (Test-Path $ZipPath)) {
    Write-Error "Failed to create ZIP file!"
    exit 1
}

# Get ZIP size
$zipSize = (Get-Item $ZipPath).Length
$zipSizeMB = [math]::Round($zipSize / 1MB, 2)

Write-Success "ZIP created successfully"

# --- Step 6: Cleanup staging ---
Write-Step "Cleaning up staging folder..."
Remove-Item -Path $StagingDir -Recurse -Force
Write-Success "Staging folder removed"

# --- Final Output ---
Write-Host ""
Write-Host "=======================================" -ForegroundColor Green
Write-Host "  BUILD COMPLETE!" -ForegroundColor Green
Write-Host "=======================================" -ForegroundColor Green
Write-Host ""
Write-Host "  Product:  $ProductName v$Version" -ForegroundColor White
Write-Host "  Size:     $zipSizeMB MB" -ForegroundColor White
Write-Host "  Output:   $ZipPath" -ForegroundColor Cyan
Write-Host ""
Write-Host "  Ready for distribution!" -ForegroundColor Green
Write-Host ""

# Open explorer to the ZIP location
$openExplorer = Read-Host "Open folder in Explorer? (Y/n)"
if ($openExplorer -eq "" -or $openExplorer -eq "Y" -or $openExplorer -eq "y") {
    Start-Process "explorer.exe" -ArgumentList "/select,`"$ZipPath`""
}
