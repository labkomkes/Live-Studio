# Live Studio

Aplikasi live production untuk **Windows** (webinar, kuliah, seminar, konferensi) berbasis **libobs**
(mesin OBS Studio) dengan antarmuka Qt 6 buatan sendiri. Dua mode: **Sederhana** (tombol besar,
sedikit panel) dan **Profesional** (semua panel).

> Baca [`docs/STATUS.md`](docs/STATUS.md) lebih dulu: di sana tertulis jujur apa yang sudah diuji
> dan apa yang **belum pernah dijalankan di Windows**.

## Fitur yang ada di kode

| Fitur | Berkas utama |
|---|---|
| Integrasi libobs (startup, video, modul, log) | `src/core/ObsEngine.*` |
| Preview → Program, **CUT** / **TAKE** (Cut, Fade, Swipe, Slide, Fade ke Warna) | `src/core/SceneManager.*` |
| Sumber: kamera (dshow), layar, jendela, video, gambar, teks, mikrofon, audio desktop | `src/core/SourceFactory.*` |
| Preview & Program memakai `obs_display` | `src/ui/ObsDisplayWidget.*`, `Render.*` |
| Audio mixer (meter, mute, volume) | `src/ui/AudioMixerWidget.*` |
| Lower third | `src/core/LowerThird.*`, `src/ui/LowerThirdPanel.*` |
| Bumper webinar (putar video bumper lalu kembali ke scene sebelumnya) | `src/core/Bumper.*`, `src/ui/BumperPanel.*` |
| **Deck ala vMix**: tile scene dengan thumbnail live, CUT/TAKE per tile, tombol transisi | `src/ui/DeckStrip.*`, `DeckLayout.h`, `Render.cpp` |
| Multiview & proyektor ke monitor pilihan | `src/ui/ProjectorWindow.*`, `MainWindow.*` |
| Installer `.exe` (Inno Setup) | `installer/LiveStudio.iss`, `scripts/package-windows.ps1` |

## Mulai cepat (Windows 10/11 x64)

```powershell
# 1. Prasyarat: Visual Studio 2022 (Desktop C++), CMake >= 3.28, Git, Qt 6.8 msvc2022_64, Inno Setup 6
.\scripts\build-obs.ps1                                   # ±15-40 menit, sekali saja
.\scripts\build-livestudio.ps1 -QtPrefix C:\Qt\6.8.3\msvc2022_64
.\scripts\package-windows.ps1  -QtPrefix C:\Qt\6.8.3\msvc2022_64
# hasil: dist\LiveStudio-Setup-<versi>.exe
```

Detail, prasyarat, dan pemecahan masalah: [`docs/BUILD_WINDOWS.md`](docs/BUILD_WINDOWS.md).
Memasang dan memakai hasilnya: [`docs/INSTALL_WINDOWS.md`](docs/INSTALL_WINDOWS.md).
Daftar uji manual: [`docs/TEST_PLAN.md`](docs/TEST_PLAN.md).

## Uji cepat

```
LiveStudio.exe --selftest            # laporan ke %APPDATA%\LiveStudio\selftest.txt
LiveStudio.exe --selftest --quiet    # tanpa dialog (CI)
```

## Struktur

```
CMakeLists.txt        build utama (Qt6 + libobs)
src/core/             logika: engine libobs, scene, sumber, lower third, bumper, selftest
src/ui/               jendela Qt, display OBS, mixer, panel, proyektor
resources/            ikon, QSS, .rc Windows
scripts/              build-obs, build-livestudio, package-windows (PowerShell)
installer/            skrip Inno Setup
tests/                smoke_gui.cpp (opsional, -DLIVESTUDIO_BUILD_TESTS=ON)
docs/                 panduan & status
.github/workflows/    CI Windows (eksperimental)
```

## Lisensi

GPL-2.0-or-later (karena menautkan libobs). Lihat `LICENSE.txt` dan `THIRD_PARTY_NOTICES.md`.
Live Studio tidak berafiliasi dengan proyek OBS.
