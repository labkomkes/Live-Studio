<#
.SYNOPSIS
  Mengonfigurasi dan membangun Live Studio (LiveStudio.exe) terhadap libobs hasil build-obs.ps1.

.PARAMETER QtPrefix
  Folder Qt 6 untuk MSVC 2022 64-bit, mis. C:\Qt\6.8.3\msvc2022_64. Bila kosong dicoba
  variabel lingkungan QT_ROOT_DIR / Qt6_DIR.

.NOTES
  STATUS: belum diuji di Windows oleh penulis. Lihat docs/BUILD_WINDOWS.md (Pemecahan masalah).
#>
[CmdletBinding()]
param(
    [string]$QtPrefix = "",
    [string]$DepsDir = (Join-Path $PSScriptRoot "..\deps"),
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build"),
    [ValidateSet("Release", "RelWithDebInfo", "Debug")][string]$Config = "Release",
    [switch]$WithTests
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$DepsDir = (Resolve-Path $DepsDir).Path
$obsInstall = Join-Path $DepsDir "obs-install"
$obsSrc = Join-Path $DepsDir "obs-studio"

if (-not (Test-Path $obsInstall)) { throw "OBS belum di-install: jalankan scripts\build-obs.ps1 dahulu ($obsInstall tidak ada)." }

if (-not $QtPrefix) { $QtPrefix = $env:QT_ROOT_DIR }
if (-not $QtPrefix -and $env:Qt6_DIR) { $QtPrefix = (Resolve-Path (Join-Path $env:Qt6_DIR "..\..\..")).Path }
if (-not $QtPrefix -or -not (Test-Path $QtPrefix)) {
    throw "Qt 6 tidak ditemukan. Berikan -QtPrefix C:\Qt\6.x.y\msvc2022_64 (lihat docs/BUILD_WINDOWS.md)."
}

# Prefix pencarian paket: Qt + hasil install OBS + obs-deps yang diunduh preset OBS (w32-pthreads, SIMDe, FFmpeg, ...)
$prefixes = New-Object System.Collections.Generic.List[string]
$prefixes.Add($QtPrefix); $prefixes.Add($obsInstall)
$depsRoot = Join-Path $obsSrc ".deps"
if (Test-Path $depsRoot) {
    Get-ChildItem $depsRoot -Directory | Where-Object { $_.Name -like "obs-deps-*" -and $_.Name -notlike "*qt6*" } |
        ForEach-Object { $prefixes.Add($_.FullName) }
}
$prefixPath = ($prefixes | ForEach-Object { $_ -replace "\\", "/" }) -join ";"

$cmakeArgs = @(
    "-S", $root, "-B", $BuildDir, "-G", "Visual Studio 17 2022", "-A", "x64",
    "-DCMAKE_PREFIX_PATH=$prefixPath",
    "-DOBS_INSTALL_DIR=$($obsInstall -replace '\\','/')",
    "-DLIVESTUDIO_OBS_RUNTIME_DIR=$($obsInstall -replace '\\','/')"
)
$cfgFile = Get-ChildItem $obsInstall -Recurse -Filter "libobsConfig.cmake" -ErrorAction SilentlyContinue | Select-Object -First 1
if ($cfgFile) { $cmakeArgs += "-Dlibobs_DIR=$($cfgFile.DirectoryName -replace '\\','/')" }
if ($WithTests) { $cmakeArgs += "-DLIVESTUDIO_BUILD_TESTS=ON" }

Write-Host "== Konfigurasi Live Studio ..."
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "cmake configure gagal. Lihat docs/BUILD_WINDOWS.md > Pemecahan masalah." }

Write-Host "== Kompilasi ($Config) ..."
cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "cmake build gagal." }

Write-Host ""
Write-Host "Selesai. Berikutnya: scripts\package-windows.ps1 -Config $Config"
