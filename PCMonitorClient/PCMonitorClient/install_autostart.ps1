#Requires -Version 5.1
<#
.SYNOPSIS
    Scarab Monitor - Autostart Installer
.DESCRIPTION
    Configures PCMonitorClient.exe to start automatically with Windows.
    Supports Full Mode (Admin Task Scheduler) and Lite Mode (Registry Run).
.NOTES
    Author: Desert-Spec Project
    Version: 1.0
#>

# ============================================================================
#  CONFIGURATION
# ============================================================================
$AppName = "ScarabMonitor"
$ExeName = "PCMonitorClient.exe"
$TaskName = "ScarabMonitor_Autostart"
$RegistryPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"

# ============================================================================
#  HELPER FUNCTIONS
# ============================================================================

function Write-ColorLine {
    param(
        [string]$Text,
        [ConsoleColor]$Color = [ConsoleColor]::White
    )
    Write-Host $Text -ForegroundColor $Color
}

function Write-Info    { param([string]$Text) Write-ColorLine $Text Cyan }
function Write-Success { param([string]$Text) Write-ColorLine $Text Green }
function Write-Warning { param([string]$Text) Write-ColorLine $Text Yellow }
function Write-Error   { param([string]$Text) Write-ColorLine $Text Red }

function Test-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Request-Elevation {
    Write-Warning "Admin rights required. Requesting elevation..."

    $scriptPath = $MyInvocation.PSCommandPath
    if (-not $scriptPath) {
        $scriptPath = $PSCommandPath
    }

    try {
        Start-Process powershell.exe -Verb RunAs -ArgumentList @(
            "-NoProfile",
            "-ExecutionPolicy", "Bypass",
            "-File", "`"$scriptPath`""
        )
        exit 0
    }
    catch {
        Write-Error "Failed to elevate. Please run as Administrator manually."
        pause
        exit 1
    }
}

function Get-ExePath {
    $scriptDir = Split-Path -Parent $MyInvocation.PSCommandPath
    if (-not $scriptDir) {
        $scriptDir = Split-Path -Parent $PSCommandPath
    }
    if (-not $scriptDir) {
        $scriptDir = Get-Location
    }

    $exePath = Join-Path $scriptDir $ExeName

    if (-not (Test-Path $exePath)) {
        # Try bin\Release\net472 subfolder
        $releasePath = Join-Path $scriptDir "bin\Release\net472\$ExeName"
        if (Test-Path $releasePath) {
            return $releasePath
        }

        Write-Error "ERROR: $ExeName not found!"
        Write-Info "Expected location: $exePath"
        Write-Info "Or: $releasePath"
        return $null
    }

    return $exePath
}

# ============================================================================
#  INSTALLATION FUNCTIONS
# ============================================================================

function Install-FullMode {
    param([string]$ExePath)

    Write-Info ""
    Write-Info "Installing Full Mode (Task Scheduler with Admin)..."
    Write-Info "Path: $ExePath"

    # Remove existing task if present
    $existingTask = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($existingTask) {
        Write-Warning "Removing existing task..."
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    }

    # Create task action
    $action = New-ScheduledTaskAction -Execute $ExePath -WorkingDirectory (Split-Path $ExePath)

    # Create trigger: At logon of any user
    $trigger = New-ScheduledTaskTrigger -AtLogOn

    # Principal: Run with highest privileges
    $principal = New-ScheduledTaskPrincipal -UserId "BUILTIN\Administrators" -RunLevel Highest -LogonType Group

    # Settings: Laptop-friendly (allow on battery, no stop on battery)
    $settings = New-ScheduledTaskSettingsSet `
        -AllowStartIfOnBatteries `
        -DontStopIfGoingOnBatteries `
        -StartWhenAvailable `
        -ExecutionTimeLimit (New-TimeSpan -Hours 0) `
        -RestartCount 3 `
        -RestartInterval (New-TimeSpan -Minutes 1)

    # Register the task
    try {
        Register-ScheduledTask `
            -TaskName $TaskName `
            -Action $action `
            -Trigger $trigger `
            -Principal $principal `
            -Settings $settings `
            -Description "Scarab Monitor - PC Hardware Monitor for ESP32 Display" `
            -Force | Out-Null

        Write-Success ""
        Write-Success "SUCCESS: Full Mode installed!"
        Write-Success "Task Name: $TaskName"
        Write-Info "The application will start with admin rights at every logon."
        Write-Info "Settings: Runs on battery, auto-restart on failure."
    }
    catch {
        Write-Error "FAILED to create scheduled task!"
        Write-Error $_.Exception.Message
        return $false
    }

    return $true
}

function Install-LiteMode {
    param([string]$ExePath)

    Write-Info ""
    Write-Info "Installing Lite Mode (Registry Run)..."
    Write-Info "Path: $ExePath"

    try {
        # Set registry value
        Set-ItemProperty -Path $RegistryPath -Name $AppName -Value "`"$ExePath`"" -Type String

        Write-Success ""
        Write-Success "SUCCESS: Lite Mode installed!"
        Write-Success "Registry: $RegistryPath\$AppName"
        Write-Warning "Note: Application will run WITHOUT admin rights."
        Write-Warning "CPU temperature will not be available (shows N/A)."
    }
    catch {
        Write-Error "FAILED to set registry key!"
        Write-Error $_.Exception.Message
        return $false
    }

    return $true
}

function Uninstall-All {
    Write-Info ""
    Write-Info "Uninstalling Scarab Monitor autostart..."

    $removedSomething = $false

    # Remove Task Scheduler entry
    $existingTask = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($existingTask) {
        Write-Info "Removing Task Scheduler entry..."
        try {
            Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
            Write-Success "Task removed: $TaskName"
            $removedSomething = $true
        }
        catch {
            Write-Error "Failed to remove task: $($_.Exception.Message)"
        }
    }
    else {
        Write-Warning "No Task Scheduler entry found."
    }

    # Remove Registry entry
    $regValue = Get-ItemProperty -Path $RegistryPath -Name $AppName -ErrorAction SilentlyContinue
    if ($regValue) {
        Write-Info "Removing Registry entry..."
        try {
            Remove-ItemProperty -Path $RegistryPath -Name $AppName -ErrorAction Stop
            Write-Success "Registry key removed: $AppName"
            $removedSomething = $true
        }
        catch {
            Write-Error "Failed to remove registry key: $($_.Exception.Message)"
        }
    }
    else {
        Write-Warning "No Registry entry found."
    }

    if ($removedSomething) {
        Write-Success ""
        Write-Success "Uninstall complete!"
    }
    else {
        Write-Warning ""
        Write-Warning "Nothing to uninstall - no autostart entries found."
    }

    return $true
}

function Show-Status {
    Write-Info ""
    Write-Info "Current Autostart Status:"
    Write-Info "========================="

    # Check Task Scheduler
    $task = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($task) {
        Write-Success "[X] Full Mode (Task Scheduler): INSTALLED"
        Write-Info "    State: $($task.State)"
    }
    else {
        Write-Warning "[ ] Full Mode (Task Scheduler): Not installed"
    }

    # Check Registry
    $regValue = Get-ItemProperty -Path $RegistryPath -Name $AppName -ErrorAction SilentlyContinue
    if ($regValue) {
        Write-Success "[X] Lite Mode (Registry Run): INSTALLED"
        Write-Info "    Path: $($regValue.$AppName)"
    }
    else {
        Write-Warning "[ ] Lite Mode (Registry Run): Not installed"
    }
}

# ============================================================================
#  MAIN MENU
# ============================================================================

function Show-Menu {
    Clear-Host
    Write-ColorLine "============================================" Cyan
    Write-ColorLine "  SCARAB MONITOR - Autostart Installer" Cyan
    Write-ColorLine "============================================" Cyan
    Write-Host ""

    Show-Status

    Write-Host ""
    Write-ColorLine "Options:" White
    Write-Host ""
    Write-ColorLine "  [1] Install Full Mode" Green
    Write-Info "      Task Scheduler with Admin rights"
    Write-Info "      CPU temp available, laptop-friendly"
    Write-Host ""
    Write-ColorLine "  [2] Install Lite Mode" Yellow
    Write-Info "      Registry Run (no Admin)"
    Write-Info "      CPU temp shows N/A"
    Write-Host ""
    Write-ColorLine "  [3] Uninstall All" Red
    Write-Info "      Remove Task + Registry entries"
    Write-Host ""
    Write-ColorLine "  [Q] Quit" White
    Write-Host ""
}

# ============================================================================
#  MAIN SCRIPT
# ============================================================================

# Check for admin (required for Full Mode installation)
if (-not (Test-Administrator)) {
    Write-Warning "Running without admin rights."
    Write-Warning "Full Mode installation requires elevation."
    Write-Host ""

    $response = Read-Host "Elevate to Administrator? (Y/N)"
    if ($response -eq 'Y' -or $response -eq 'y') {
        Request-Elevation
    }
}

# Find executable
$exePath = Get-ExePath
if (-not $exePath) {
    Write-Host ""
    pause
    exit 1
}

Write-Info "Found: $exePath"
Start-Sleep -Milliseconds 500

# Main loop
do {
    Show-Menu
    $choice = Read-Host "Select option"

    switch ($choice.ToUpper()) {
        '1' {
            if (-not (Test-Administrator)) {
                Write-Error "Full Mode requires Administrator rights!"
                Write-Info "Please restart the script as Administrator."
                pause
            }
            else {
                Install-FullMode -ExePath $exePath
                pause
            }
        }
        '2' {
            Install-LiteMode -ExePath $exePath
            pause
        }
        '3' {
            if (-not (Test-Administrator)) {
                Write-Warning "Note: Removing Task Scheduler entry requires Admin."
                Write-Warning "Only Registry entry will be removed."
            }
            Uninstall-All
            pause
        }
        'Q' {
            Write-Info "Goodbye!"
            break
        }
        default {
            Write-Warning "Invalid option. Please try again."
            Start-Sleep -Seconds 1
        }
    }
} while ($choice.ToUpper() -ne 'Q')
