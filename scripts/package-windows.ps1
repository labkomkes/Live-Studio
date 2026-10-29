<#
.SYNOPSIS
  Menyusun folder distribusi (LiveStudio + runtime OBS + Qt), menjalankan uji mandiri, lalu
  membuat installer .exe dengan Inno Setup.

.NOTES
  STATUS: belum diuji di Windows oleh penulis. Prasyarat: hasil build-livestudio.ps1,
  windeployqt (dari Qt), Inno Setup 6 (iscc.exe). Output: dist\LiveStudio-Setup-<versi>.exe
#>
[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot "..\build"),
    [ValidateSet("Release", "RelWithDebInfo", "Debug")][string]$Config = "Release",
    [string]$QtPrefix = "",
    [string]$DistDir = (Join-Path $PSScriptRoot "..\dist"),
    [switch]$SkipRedist,
    [switch]$SkipSelfTest
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

# Versi diambil dari CMakeLists.txt agar satu sumber kebenaran.
$cm = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cm -notmatch 'project\(LiveStudio VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw "Versi tidak ditemukan di CMakeLists.txt" }
$version = $Matches[1]

New-Item -ItemType Directory -Force -Path $DistDir | Out-Null
$DistDir = (Resolve-Path $DistDir).Path
$stage = Join-Path $DistDir "LiveStudio"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }

Write-Host "== cmake --install -> $stage"
cmake --install $BuildDir --config $Config --prefix $stage
if ($LASTEXITCODE -ne 0) { throw "cmake --install gagal." }

$exe = Join-Path $stage "bin\64bit\LiveStudio.exe"
if (-not (Test-Path $exe)) { throw "LiveStudio.exe tidak ada di $stage\bin\64bit" }
if (-not (Test-Path (Join-Path $stage "bin\64bit\obs.dll"))) {
    throw "obs.dll tidak ada: runtime OBS tidak ikut ter-install. Konfigurasi ulang dengan -DLIVESTUDIO_OBS_RUNTIME_DIR=<deps\obs-install>."
}

# --- Qt ---
if (-not $QtPrefix) { $QtPrefix = $env:QT_ROOT_DIR }
$windeployqt = $null
if ($QtPrefix) { $windeployqt = Join-Path $QtPrefix "bin\windeployqt.exe" }
if (-not $windeployqt -or -not (Test-Path $windeployqt)) {
    $cmd = Get-Command windeployqt -ErrorAction SilentlyContinue
    if ($cmd) { $windeployqt = $cmd.Source } else { throw "windeployqt.exe tidak ditemukan. Berikan -QtPrefix." }
}
Write-Host "== windeployqt"
$qtFlag = if ($Config -eq "Debug") { "--debug" } else { "--release" }
& $windeployqt $qtFlag --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime $exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt gagal." }

# --- Lisensi komponen ---
New-Item -ItemType Directory -Force -Path (Join-Path $stage "licenses") | Out-Null
foreach ($f in @("LICENSE.txt", "THIRD_PARTY_NOTICES.md")) {
    $p = Join-Path $root $f
    if (Test-Path $p) { Copy-Item $p (Join-Path $stage "licenses") -Force }
}
if ($QtPrefix) {
    $qtLic = Join-Path $QtPrefix "..\..\Licenses"
    if (Test-Path $qtLic) { Copy-Item "$qtLic\*" (Join-Path $stage "licenses") -Recurse -Force -ErrorAction SilentlyContinue }
}

# --- VC++ Redistributable (dipasang installer bila belum ada) ---
$redistDir = Join-Path $DistDir "redist"
$redist = Join-Path $redistDir "vc_redist.x64.exe"
if (-not $SkipRedist -and -not (Test-Path $redist)) {
    Write-Host "== Mengunduh vc_redist.x64.exe"
    New-Item -ItemType Directory -Force -Path $redistDir | Out-Null
    Invoke-WebRequest -Uri "https://aka.ms/vs/17/release/vc_redist.x64.exe" -OutFile $redist
}

# --- Uji mandiri pada folder hasil susunan ---
if (-not $SkipSelfTest) {
    Write-Host "== Uji mandiri (LiveStudio.exe --selftest --quiet)"
    $report = Join-Path $env:APPDATA "LiveStudio\selftest.txt"
    if (Test-Path $report) { Remove-Item $report -Force }
    $p = Start-Process -FilePath $exe -ArgumentList "--selftest", "--quiet" -Wait -PassThru
    if (Test-Path $report) { Get-Content $report | Write-Host } else { Write-Warning "Laporan selftest tidak ditemukan ($report)." }
    if ($p.ExitCode -ne 0) {
        Write-Warning "Uji mandiri melaporkan FAIL (exit code $($p.ExitCode)). Periksa laporan di atas sebelum merilis."
    }
}

# --- Installer ---
$iscc = Get-Command iscc -ErrorAction SilentlyContinue
if (-not $iscc) {
    $candidates = @("$env:ProgramFiles\Inno Setup 6\ISCC.exe", "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe")
    $found = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($found) { $iscc = @{ Source = $found } } else { throw "Inno Setup 6 (ISCC.exe) tidak ditemukan. Pasang dari https://jrsoftware.org/isdl.php" }
}
Write-Host "== Inno Setup"
$redistArg = if (Test-Path $redist) { "/DRedistFile=$redist" } else { "/DRedistFile=" }
& $iscc.Source "/DAppVersion=$version" "/DStageDir=$stage" "/DOutputDir=$DistDir" $redistArg (Join-Path $root "installer\LiveStudio.iss")
if ($LASTEXITCODE -ne 0) { throw "Inno Setup gagal." }

Write-Host ""
Write-Host "Installer: $(Join-Path $DistDir "LiveStudio-Setup-$version.exe")"
