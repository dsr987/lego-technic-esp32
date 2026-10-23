param(
    [Parameter(Mandatory = $true)]
    [string]$Sketchbook
)

$ErrorActionPreference = 'Stop'
$header = Join-Path $Sketchbook 'libraries\ElegantOTA\src\ElegantOTA.h'

if (-not (Test-Path -LiteralPath $header)) {
    throw "Файл ElegantOTA.h не найден по пути '$header'. Возможно, библиотека установлена с ошибкой."
}

$content = Get-Content -LiteralPath $header -Raw
$pattern = '(?m)^(\s*#\s*define\s+ELEGANTOTA_USE_ASYNC_WEBSERVER\s+)0(\s*(?://.*)?$)'
$updated = [regex]::Replace($content, $pattern, '${1}1${2}')

if ($updated -ne $content) {
    Set-Content -LiteralPath $header -Value $updated -NoNewline -Encoding utf8
    Write-Host 'Включён параметр ELEGANTOTA_USE_ASYNC_WEBSERVER=1.'
    exit 0
}

if ($content -match '(?m)^\s*#\s*define\s+ELEGANTOTA_USE_ASYNC_WEBSERVER\s+1\b') {
    Write-Host 'Асинхронный режим ElegantOTA уже включён.'
    exit 0
}

throw 'Не найден ожидаемый макрос ELEGANTOTA_USE_ASYNC_WEBSERVER. Работа остановлена: конфигурация библиотеки неизвестна.'
