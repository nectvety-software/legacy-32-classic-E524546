param(
    [string]$RetroGoPath = "D:\Program\arduino\_reference_retro-go"
)

$ErrorActionPreference = "Stop"
$portRoot = Join-Path $PSScriptRoot "pochita-e524546"
$targetsRoot = Join-Path $RetroGoPath "components\retro-go\targets"
$targetRoot = Join-Path $targetsRoot "pochita-e524546"
$baseTarget = Join-Path $targetsRoot "esp32-s3-devkit"
$rootConfig = Join-Path $RetroGoPath "components\retro-go\config.h"

if (-not (Test-Path (Join-Path $RetroGoPath ".git"))) {
    throw "Retro-Go repository was not found at: $RetroGoPath"
}

New-Item -ItemType Directory -Force -Path $targetRoot | Out-Null
Copy-Item -Force (Join-Path $portRoot "config.h") (Join-Path $targetRoot "config.h")
Copy-Item -Force (Join-Path $portRoot "env.py") (Join-Path $targetRoot "env.py")
Copy-Item -Force (Join-Path $baseTarget "sdkconfig") (Join-Path $targetRoot "sdkconfig")

$content = Get-Content -Raw $rootConfig
if ($content -notmatch "RG_TARGET_POCHITA_E524546") {
    $pattern = '(?m)^#else\r?\n#warning "No target defined\. Defaulting to ODROID-GO\."'
    $replacement = "#elif defined(RG_TARGET_POCHITA_E524546)`n#include `"targets/pochita-e524546/config.h`"`n#else`n#warning `"No target defined. Defaulting to ODROID-GO.`""
    $updated = [regex]::Replace($content, $pattern, $replacement, 1)
    if ($updated -eq $content) {
        throw "Retro-Go target selector layout has changed; config.h was not modified."
    }
    Set-Content -NoNewline -Encoding utf8 $rootConfig $updated
}

Write-Host "POCHITA E524546 target installed into $RetroGoPath"
Write-Host "Build: python rg_tool.py --target=pochita-e524546 build-img"
Write-Host "Flash: python rg_tool.py --target=pochita-e524546 --port=COM5 install"
