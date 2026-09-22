param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$cmakeLists = Get-Content -LiteralPath (Join-Path $repoRoot 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'project\(NppMarkdownFeatures VERSION (\d+\.\d+\.\d+)') {
    throw 'Could not read project version from CMakeLists.txt'
}
$version = $Matches[1]
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
# Plugins Admin requires the DLL at the zip root and a file version equal to the listed version.
$fileVersion = (Get-Item -LiteralPath $dll).VersionInfo.FileVersionRaw
if ("$fileVersion" -ne "$version.0") {
    throw "DLL file version '$fileVersion' does not match project version $version.0"
}
if (Test-Path $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage 'NppMarkdownFeatures.dll') -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'README.md') -Destination $stage -Force
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE') -Destination $stage -Force

$zip = Join-Path $dist "NppMarkdownFeatures-v$version-win-x64.zip"
if (Test-Path $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Write-Host "Packaged: $zip"
