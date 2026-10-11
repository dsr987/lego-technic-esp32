param(
    [Parameter(Mandatory = $true)]
    [string]$Sketchbook
)

$ErrorActionPreference = 'Stop'
$header = Join-Path $Sketchbook 'libraries\ElegantOTA\src\ElegantOTA.h'

if (-not (Test-Path -LiteralPath $header)) {
    throw "ElegantOTA.h was not found at '$header'. The library installation may have failed."
}

$content = Get-Content -LiteralPath $header -Raw
$pattern = '(?m)^(\s*#\s*define\s+ELEGANTOTA_USE_ASYNC_WEBSERVER\s+)0(\s*(?://.*)?$)'
$updated = [regex]::Replace($content, $pattern, '${1}1${2}')

if ($updated -ne $content) {
    Set-Content -LiteralPath $header -Value $updated -NoNewline -Encoding utf8
    Write-Host 'Enabled ELEGANTOTA_USE_ASYNC_WEBSERVER=1.'
    exit 0
}

if ($content -match '(?m)^\s*#\s*define\s+ELEGANTOTA_USE_ASYNC_WEBSERVER\s+1\b') {
    Write-Host 'ElegantOTA async mode was already enabled.'
    exit 0
}

throw 'Could not locate the expected ELEGANTOTA_USE_ASYNC_WEBSERVER macro. Refusing to continue with unknown library configuration.'
