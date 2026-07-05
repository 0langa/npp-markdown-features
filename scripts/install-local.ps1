param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$NotepadPlusPlusDir = 'C:\Program Files\Notepad++'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDll = Join-Path $repoRoot "build\bin\$Configuration\NppMarkdownFeatures.dll"
if (-not (Test-Path $sourceDll)) {
    $sourceDll = Join-Path $repoRoot 'build\NppMarkdownFeatures.dll'
}
$targetDir = Join-Path $NotepadPlusPlusDir 'plugins\NppMarkdownFeatures'
$targetDll = Join-Path $targetDir 'NppMarkdownFeatures.dll'

if (-not (Test-Path $sourceDll)) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Configuration $Configuration
}

if (-not (Test-Path (Join-Path $NotepadPlusPlusDir 'notepad++.exe'))) {
    throw "Notepad++ not found at $NotepadPlusPlusDir"
}

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    $command = "`$ErrorActionPreference='Stop'; New-Item -ItemType Directory -Force -Path `"$targetDir`" | Out-Null; Copy-Item -LiteralPath `"$sourceDll`" -Destination `"$targetDll`" -Force"
    Write-Host "Admin rights required to copy into Program Files."
    Write-Host "Elevated command:"
    Write-Host $command
    Write-Host "Launching elevated install copy..."
    $process = Start-Process powershell -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', $command)
    if ($process.ExitCode -ne 0) {
        Write-Warning "Elevated copy process exited with code $($process.ExitCode)."
    }
} else {
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
    Copy-Item -LiteralPath $sourceDll -Destination $targetDll -Force
}

if ((Test-Path $targetDll) -and ((Get-FileHash -Algorithm SHA256 -LiteralPath $sourceDll).Hash -eq (Get-FileHash -Algorithm SHA256 -LiteralPath $targetDll).Hash)) {
    Write-Host "Installed: $targetDll"
} else {
    Write-Warning "Install copy was not verified. If UAC was cancelled or Program Files remains protected, run the elevated command shown above from an Administrator PowerShell."
    exit 1
}
