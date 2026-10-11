$ErrorActionPreference = 'Stop'

$installDir = Join-Path $env:LOCALAPPDATA 'LegoTechnicESP32'
$targetExe = Join-Path $installDir 'arduino-cli.exe'
$zipPath = Join-Path $env:TEMP 'arduino-cli-windows-x64.zip'
$extractDir = Join-Path $env:TEMP 'arduino-cli-windows-x64-extracted'
$url = 'https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip'

New-Item -ItemType Directory -Force -Path $installDir | Out-Null
if (Test-Path $extractDir) {
    Remove-Item -LiteralPath $extractDir -Recurse -Force
}

Write-Host "Downloading Arduino CLI from the official Arduino download server..."
Invoke-WebRequest -Uri $url -OutFile $zipPath
Expand-Archive -LiteralPath $zipPath -DestinationPath $extractDir -Force

$cli = Get-ChildItem -LiteralPath $extractDir -Filter 'arduino-cli.exe' -File -Recurse |
    Select-Object -First 1
if (-not $cli) {
    throw 'arduino-cli.exe was not found in the downloaded archive.'
}

Copy-Item -LiteralPath $cli.FullName -Destination $targetExe -Force
Remove-Item -LiteralPath $zipPath -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $extractDir -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Arduino CLI installed to: $targetExe"
& $targetExe version
if ($LASTEXITCODE -ne 0) {
    throw 'Arduino CLI did not start successfully.'
}
