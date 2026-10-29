<#
.SYNOPSIS
  Membangun libobs + plugin OBS (TANPA UI Qt OBS) dari source OBS Studio, lalu meng-install
  ke deps\obs-install. Hasilnya dipakai oleh build-livestudio.ps1 dan package-windows.ps1.

.NOTES
  STATUS: skrip ini belum diuji di Windows oleh penulis. Lihat docs/BUILD_WINDOWS.md.
  Prasyarat: Visual Studio 2022 (workload "Desktop development with C++"), Windows SDK 10.0.22621+,
  CMake >= 3.28, Git, koneksi internet (preset OBS mengunduh obs-deps otomatis).
#>
[CmdletBinding()]
param(
    [string]$ObsTag = "32.2.2",
    [string]$DepsDir = (Join-Path $PSScriptRoot "..\deps"),
    [ValidateSet("Release", "RelWithDebInfo")][string]$Config = "RelWithDebInfo",
    # Preset 'windows-x64' di OBS 32.x mewajibkan Visual Studio 2026 + SDK 10.0.26100, sehingga gagal
    # di Visual Studio 2022 (mis. runner GitHub windows-2022). Karena itu generator diatur sendiri di sini.
    [string]$Generator = "Visual Studio 17 2022"
)
$ErrorActionPreference = "Stop"

function Require-Command([string]$name) {
    if (-not (Get-Command $name -ErrorAction SilentlyContinue)) {
        throw "Perintah '$name' tidak ditemukan di PATH. Lihat docs/BUILD_WINDOWS.md (Prasyarat)."
    }
}
function Check-Exit([string]$what) {
    if ($LASTEXITCODE -ne 0) { throw "$what gagal (exit code $LASTEXITCODE)." }
}

Require-Command git
Require-Command cmake

New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null
$DepsDir = (Resolve-Path $DepsDir).Path
$src = Join-Path $DepsDir "obs-studio"
$install = Join-Path $DepsDir "obs-install"

if (-not (Test-Path (Join-Path $src "CMakeLists.txt"))) {
    Write-Host "== Mengunduh OBS Studio $ObsTag (termasuk submodule) ..."
    git clone --branch $ObsTag --depth 1 --recurse-submodules --shallow-submodules `
        https://github.com/obsproject/obs-studio.git $src
    Check-Exit "git clone"
}

Push-Location $src
try {
    Write-Host "== Konfigurasi OBS ($Generator, tanpa frontend Qt) ..."
    # Opsi yang tidak dikenal versi OBS tertentu hanya menghasilkan peringatan CMake.
    # Nilai cache di bawah menyalin preset 'windows-x64' OBS (VIRTUALCAM_GUID, GPU_PRIORITY_VAL).
    cmake -S . -B build_x64 -G $Generator -A x64 `
        -DVIRTUALCAM_GUID=A3FCE0F5-3493-419F-958A-ABA1250EC20B `
        -DGPU_PRIORITY_VAL=0 `
        -DENABLE_FRONTEND=OFF `
        -DENABLE_BROWSER=OFF `
        -DENABLE_SCRIPTING=OFF `
        -DENABLE_WEBSOCKET=OFF `
        -DENABLE_VLC=OFF `
        -DENABLE_AJA=OFF `
        -DENABLE_VIRTUALCAM=OFF `
        -DENABLE_NEW_MPEGTS_OUTPUT=OFF
    Check-Exit "cmake configure OBS"

    Write-Host "== Kompilasi OBS ($Config). Ini bisa memakan waktu 15-40 menit ..."
    cmake --build build_x64 --config $Config --parallel
    Check-Exit "cmake build OBS"

    Write-Host "== Install ke $install ..."
    cmake --install build_x64 --config $Config --prefix $install
    Check-Exit "cmake install OBS"
}
finally {
    Pop-Location
}

Write-Host ""
Write-Host "Selesai. OBS_INSTALL_DIR = $install"
Write-Host "Langkah berikut: scripts\build-livestudio.ps1"
