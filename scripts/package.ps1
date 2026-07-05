param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$version = '0.1.0'
$dll = Join-Path $repoRoot "build\bin\$Configuration\NppMarkdownFeatures.dll"
if (-not (Test-Path $dll)) {
    $dll = Join-Path $repoRoot 'build\NppMarkdownFeatures.dll'
}
if (-not (Test-Path $dll)) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration
    $dll = Join-Path $repoRoot "build\bin\$Configuration\NppMarkdownFeatures.dll"
    if (-not (Test-Path $dll)) {
        $dll = Join-Path $repoRoot 'build\NppMarkdownFeatures.dll'
    }
}

$dist = Join-Path $repoRoot 'dist'
$stage = Join-Path $dist "NppMarkdownFeatures-v$version-win-x64"
$pluginStage = Join-Path $stage 'plugins\NppMarkdownFeatures'
New-Item -ItemType Directory -Force -Path $pluginStage | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $pluginStage 'NppMarkdownFeatures.dll') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $stage -Force

$zip = Join-Path $dist "NppMarkdownFeatures-v$version-win-x64.zip"
if (Test-Path $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Write-Host "Packaged: $zip"
