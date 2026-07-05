param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found: $vswhere"
}

$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ([string]::IsNullOrWhiteSpace($vsInstall)) {
    throw "Visual Studio C++ x64 build tools not found."
}

$vcvars = Join-Path $vsInstall 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found: $vcvars"
}

$cache = Join-Path $buildDir 'CMakeCache.txt'
if (Test-Path $cache) {
    $cacheText = Get-Content -Raw $cache
    if ($cacheText -notmatch 'CMAKE_GENERATOR:INTERNAL=Ninja') {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
}

$configure = "call `"$vcvars`" >nul && cmake -S `"$repoRoot`" -B `"$buildDir`" -G Ninja -DCMAKE_BUILD_TYPE=$Configuration"
$build = "call `"$vcvars`" >nul && cmake --build `"$buildDir`" --parallel"
$test = "call `"$vcvars`" >nul && ctest --test-dir `"$buildDir`" --output-on-failure"

cmd /c $configure
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
cmd /c $build
if ($LASTEXITCODE -ne 0) { throw "CMake build failed." }
cmd /c $test
if ($LASTEXITCODE -ne 0) { throw "CTest failed." }

$dll = Join-Path $buildDir "bin\$Configuration\NppMarkdownFeatures.dll"
if (-not (Test-Path $dll)) {
    $dll = Join-Path $buildDir 'NppMarkdownFeatures.dll'
}
if (-not (Test-Path $dll)) {
    throw "Plugin DLL not found: $dll"
}

Write-Host "Built: $dll"
