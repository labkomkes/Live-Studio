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
    [string]$DepsDir = (Join-Path $PSScriptRoot "..\deps"),
    [switch]$SkipRedist,
    [switch]$SkipSelfTest,
    [switch]$SkipDepCheck
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

# --- DLL dependensi OBS (FFmpeg, x264, zlib, dll.) dari paket obs-deps ---
# 'cmake --install' OBS TIDAK menyalin DLL ini. Tanpa mereka LiveStudio.exe gagal start dengan
# "The code execution cannot proceed because avcodec-62.dll was not found".
$depsRoot = Join-Path $DepsDir "obs-studio\.deps"
$binDest = Join-Path $stage "bin\64bit"
$depDirs = @()
if (Test-Path $depsRoot) {
    $depDirs = @(Get-ChildItem $depsRoot -Directory | Where-Object { $_.Name -like "obs-deps-*" -and $_.Name -notlike "*qt6*" })
}
if ($depDirs.Count -eq 0) { throw "Paket obs-deps tidak ditemukan di $depsRoot. Jalankan scripts\build-obs.ps1 dahulu." }
Write-Host "== Menyalin DLL dependensi OBS"
$copied = 0
foreach ($d in $depDirs) {
    $depBin = Join-Path $d.FullName "bin"
    if (-not (Test-Path $depBin)) { continue }
    foreach ($f in Get-ChildItem $depBin -Filter "*.dll" -File) {
        $target = Join-Path $binDest $f.Name
        if (-not (Test-Path $target)) { Copy-Item $f.FullName $target; $copied++ }
    }
}
Write-Host "   $copied DLL disalin dari: $($depDirs.Name -join ', ')"

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

# --- Pemeriksaan dependensi DLL (mencegah "xxx.dll was not found" di PC pengguna) ---
function Find-Dumpbin {
    $c = Get-Command dumpbin -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $vsw = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsw) {
        $f = & $vsw -latest -products * -find "VC\Tools\MSVC\**\bin\Hostx64\x64\dumpbin.exe" | Select-Object -First 1
        if ($f) { return $f }
    }
    return $null
}
if (-not $SkipDepCheck) {
    $dumpbin = Find-Dumpbin
    if (-not $dumpbin) {
        Write-Warning "dumpbin.exe tidak ditemukan; pemeriksaan dependensi DLL dilewati."
    } else {
        Write-Host "== Pemeriksaan dependensi DLL"
        $files = @(Get-ChildItem $stage -Recurse -Include "*.dll", "*.exe" -File)
        $have = @{}
        foreach ($f in $files) { $have[$f.Name.ToLower()] = $true }
        $sys = Join-Path $env:SystemRoot "System32"
        $missingCore = @{}   # dibutuhkan file di bin\64bit -> aplikasi tidak bisa start
        $missingPlugin = @{} # dibutuhkan plugin -> plugin itu gagal dimuat
        foreach ($f in $files) {
            $inCore = $f.DirectoryName -ieq $binDest
            $out = & $dumpbin /nologo /dependents $f.FullName 2>$null
            $in = $false
            foreach ($line in $out) {
                if ($line -match "delay load") { $in = $false; continue }
                if ($line -match "Image has the following dependencies") { $in = $true; continue }
                if ($line -match "^\s*Summary") { break }
                if (-not $in) { continue }
                if ($line -notmatch "^\s+(\S+\.dll)\s*$") { continue }
                $dep = $Matches[1].ToLower()
                if ($dep -like "api-ms-win-*" -or $dep -like "ext-ms-win-*") { continue }
                if ($have.ContainsKey($dep)) { continue }
                if (Test-Path (Join-Path $sys $dep)) { continue }
                $tbl = if ($inCore) { $missingCore } else { $missingPlugin }
                if (-not $tbl.ContainsKey($dep)) { $tbl[$dep] = New-Object System.Collections.Generic.List[string] }
                $tbl[$dep].Add($f.Name)
            }
        }
        foreach ($k in $missingPlugin.Keys) { Write-Warning "Plugin butuh $k (dibutuhkan oleh: $(($missingPlugin[$k] | Select-Object -Unique) -join ', '))" }
        if ($missingCore.Count -gt 0) {
            foreach ($k in $missingCore.Keys) { Write-Host "HILANG: $k (dibutuhkan oleh: $(($missingCore[$k] | Select-Object -Unique) -join ', '))" }
            throw "DLL dependensi hilang di bin\64bit (daftar di atas). Installer tidak dibuat agar tidak rusak."
        }
        Write-Host "   semua dependensi bin\64bit terpenuhi"
    }
}

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
    # Matikan dialog error sistem ("DLL was not found") pada proses anak agar tidak menggantung di CI,
    # dan beri batas waktu 120 detik.
    Add-Type -Namespace LS -Name Native -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);'
    [void][LS.Native]::SetErrorMode(0x8003)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = "--selftest --quiet"
    $psi.WorkingDirectory = Split-Path $exe
    $psi.UseShellExecute = $false
    $p = [System.Diagnostics.Process]::Start($psi)
    $selfExit = -1
    if ($p.WaitForExit(120000)) { $selfExit = $p.ExitCode }
    else { Write-Warning "Uji mandiri tidak selesai dalam 120 detik; proses dihentikan."; try { $p.Kill() } catch {} }
    if (Test-Path $report) { Get-Content $report | Write-Host } else { Write-Warning "Laporan selftest tidak ditemukan ($report)." }
    if ($selfExit -ne 0) {
        Write-Warning "Uji mandiri melaporkan FAIL (exit code $selfExit). Periksa laporan di atas sebelum merilis."
        $logDir = Join-Path $env:APPDATA "LiveStudio\logs"
        $lastLog = Get-ChildItem $logDir -Filter "*.log" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1
        if ($lastLog) { Write-Host "--- $($lastLog.FullName) (40 baris terakhir) ---"; Get-Content $lastLog.FullName -Tail 40 | Write-Host }
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
