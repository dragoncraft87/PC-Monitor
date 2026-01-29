# SCARAB MONITOR - UNIVERSAL INSTALLER (V1.3 Nuclear Option)
# Uses native schtasks.exe to bypass PowerShell parameter issues

$exeName = "PCMonitorClient.exe"
$taskName = "ScarabMonitor_Autostart"
$scriptPath = $PSScriptRoot
$exePath = "$scriptPath\$exeName"

# --- Admin Check ---
if (!([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "Fordere Admin-Rechte an..." -ForegroundColor Yellow
    Start-Process powershell.exe "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

Clear-Host
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  SCARAB MONITOR - Installer v1.3 (Nuclear) " -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Path: $exePath"
Write-Host ""
Write-Host "[1] Install Full Mode (Admin Task)"
Write-Host "[2] Install Lite Mode (Registry Run)"
Write-Host "[3] Uninstall All"
Write-Host "[Q] Quit"
Write-Host ""

$choice = Read-Host "Select option"

# Cleanup
Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "ScarabMonitor" -ErrorAction SilentlyContinue

if ($choice -eq "1") {
    Write-Host "Installing Full Mode..." -ForegroundColor Yellow
    
    # SCHRITT 1: Task mit schtasks.exe erstellen (Robuster als PowerShell)
    # /SC ONLOGON = Bei Anmeldung
    # /RL HIGHEST = Admin Rechte
    # /F = Erzwingen
    $cmdArgs = "/Create /TN ""$taskName"" /TR ""'""$exePath""'"" /SC ONLOGON /RL HIGHEST /F"
    
    Start-Process "schtasks.exe" -ArgumentList $cmdArgs -Wait -NoNewWindow
    
    # Check ob Task existiert
    if (Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue) {
        Write-Host "Basic Task created." -ForegroundColor Gray
        
        # SCHRITT 2: Laptop-Settings nachpatchen (Batterie erlauben)
        try {
            $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -Compatibility Win8
            Set-ScheduledTask -TaskName $taskName -Settings $settings -ErrorAction Stop
            Write-Host "Battery settings applied." -ForegroundColor Gray
            
            Write-Host ""
            Write-Host "SUCCESS: Full Mode installed correctly!" -ForegroundColor Green
        }
        catch {
            Write-Host "WARNING: Task created, but battery settings could not be applied." -ForegroundColor Yellow
            Write-Host "The App will run, but might verify battery status strict."
        }
    }
    else {
        Write-Host ""
        Write-Host "CRITICAL ERROR: schtasks.exe failed!" -ForegroundColor Red
    }
}
elseif ($choice -eq "2") {
    try {
        Set-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "ScarabMonitor" -Value $exePath -ErrorAction Stop
        Write-Host ""
        Write-Host "SUCCESS: Lite Mode installed!" -ForegroundColor Green
    }
    catch {
        Write-Host "ERROR: Registry Write Failed." -ForegroundColor Red
    }
}
elseif ($choice -eq "3") {
    Write-Host "Uninstalled successfully." -ForegroundColor Green
}

Write-Host ""
Read-Host "Press Enter to exit"