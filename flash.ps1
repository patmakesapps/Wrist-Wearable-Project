# Build + upload one environment, then (optionally) open the serial monitor.
#   .\flash.ps1                                -> controller (the wearable) on COM3
#   .\flash.ps1 rover -Port COM5               -> rover board on another port
#   .\flash.ps1 stage4_espnow_link -Port COM5  -> a bring-up stage
#   .\flash.ps1 controller -Monitor            -> upload, then open the monitor (Ctrl+C to exit)
#
# Sets UTF-8 for the pio process first: esptool's Unicode progress bar otherwise
# crashes pio's console echo thread on a cp1252 Windows console (the upload itself
# still completes, but the output stops mid-way).
param(
    [string]$Env = "controller",
    [string]$Port = "COM3",
    [switch]$Monitor
)
$env:PYTHONIOENCODING = "utf-8"
$env:PYTHONUTF8 = "1"
pio run -e $Env -t upload --upload-port $Port
if ($LASTEXITCODE -eq 0 -and $Monitor) {
    pio device monitor --port $Port
}
