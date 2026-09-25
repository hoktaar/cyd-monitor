# CYD-Monitor Host: schickt Daten vom PC an das ESP32-2432S028 - per USB (CH340 erkannt) oder per WLAN.
# Jeder Provider liefert ein Hashtable "schluessel -> wert"; gesendet werden nur Aenderungen.
# Neue Anzeige: Provider hier ergaenzen + Modul in der Firmware.
#
# Start:  powershell -ExecutionPolicy Bypass -File cyd-host.ps1 [-Url http://192.168.x.y] [-Port COM3] [-NoUsb]
# Hinweis: Im USB-Modus ist der COM-Port belegt (zum Flashen vorher beenden).

param(
    [string]$Url = 'http://cyd-monitor.local',
    [string]$Port = '',
    [switch]$NoUsb,
    [int]$Baud = 115200
)

function Get-UnixTime { [DateTimeOffset]::UtcNow.ToUnixTimeSeconds() }

$Providers = @(
    @{ Name = 'time'; Interval = 30; Get = { @{ time = Get-UnixTime } } }
)

# Sammelt alle faelligen Aenderungen als Zeilen "k=v".
$script:sent = @{}
$script:next = @{}
function Get-PendingLines {
    $now = [DateTime]::UtcNow
    $lines = @()
    foreach ($p in $Providers) {
        if ($script:next[$p.Name] -and $now -lt $script:next[$p.Name]) { continue }
        $script:next[$p.Name] = $now.AddSeconds($p.Interval)
        try { $values = & $p.Get } catch { Write-Warning "$($p.Name): $_"; continue }
        foreach ($k in $values.Keys) {
            $v = "$($values[$k])"
            if ($script:sent[$k] -ne $v) { $lines += "$k=$v"; $script:sent[$k] = $v }
        }
    }
    $lines
}
function Reset-Sent { $script:sent = @{}; $script:next = @{} }

function Find-UsbPort {
    if ($NoUsb) { return }
    if ($Port) { return $Port }
    $dev = Get-CimInstance Win32_PnPEntity -Filter "Name LIKE '%CH340%'" | Select-Object -First 1
    if ($dev -and $dev.Name -match '\((COM\d+)\)') { return $Matches[1] }
}

function Invoke-Usb([string]$name) {
    $sp = New-Object System.IO.Ports.SerialPort $name, $Baud
    $sp.NewLine = "`n"; $sp.DtrEnable = $false; $sp.RtsEnable = $false; $sp.WriteTimeout = 1000
    try {
        $sp.Open()
        Write-Host "USB: verbunden mit $name"
        Reset-Sent
        $buf = ''
        while ($sp.IsOpen) {
            if ($sp.BytesToRead -gt 0) {
                $buf += $sp.ReadExisting()
                while (($i = $buf.IndexOf("`n")) -ge 0) {
                    $line = $buf.Substring(0, $i).Trim(); $buf = $buf.Substring($i + 1)
                    if (-not $line) { continue }
                    Write-Host "< $line"
                    if ($line -like 'hello=*') { Reset-Sent }  # Board neu gestartet: alles erneut senden
                }
            }
            foreach ($l in Get-PendingLines) { $sp.WriteLine($l); Write-Host "> $l" }
            Start-Sleep -Milliseconds 200
        }
    } catch {
        Write-Warning "USB $name getrennt: $($_.Exception.Message)"
    } finally {
        if ($sp.IsOpen) { $sp.Close() }
        $sp.Dispose()
    }
}

# Ein WLAN-Durchlauf; gibt $false zurueck, wenn das Board nicht erreichbar ist.
$script:bootId = ''
function Invoke-Wifi {
    $lines = @(Get-PendingLines)
    if (-not $lines.Count) { return $true }
    try {
        $r = Invoke-WebRequest -Uri "$Url/api/data" -Method Post -Body ($lines -join "`n") -ContentType 'text/plain' -UseBasicParsing -TimeoutSec 5
        foreach ($l in $lines) { Write-Host "> $l" }
        if ($r.Content -ne $script:bootId) {
            if ($script:bootId) { Write-Host 'WLAN: Board neu gestartet, sende alles erneut' ; Reset-Sent }
            else { Write-Host "WLAN: verbunden mit $Url" }
            $script:bootId = $r.Content
        }
        return $true
    } catch {
        Reset-Sent  # beim naechsten Versuch alles neu senden
        $script:bootId = ''
        return $false
    }
}

while ($true) {
    $usb = Find-UsbPort
    if ($usb) { Invoke-Usb $usb; Start-Sleep 2; continue }
    if (-not (Invoke-Wifi)) {
        Write-Host "Board nicht erreichbar ($Url, kein USB) - neuer Versuch in 10 s"
        Start-Sleep 10
        continue
    }
    Start-Sleep 1
}
