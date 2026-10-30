# Membangun Live Studio di Windows

> Skrip dan langkah di bawah **belum diuji di Windows** (lihat `STATUS.md`). Ikuti urut; jika ada
> yang gagal, lihat Pemecahan masalah dan kirim teks galatnya.

## 1. Prasyarat

| Alat | Versi | Catatan |
|---|---|---|
| Windows | 10 (1809+) / 11, x64 | |
| Visual Studio 2022 | 17.x | workload **Desktop development with C++**, Windows SDK 10.0.22621+ |
| CMake | >= 3.28 | masuk PATH |
| Git | terbaru | masuk PATH |
| Qt | 6.8 LTS, `msvc2022_64` | pasang dari Qt Online Installer (komponen Qt 6.8.x > MSVC 2022 64-bit) |
| Inno Setup | 6.x | hanya untuk membuat installer |
| PowerShell | 5.1+ | jalankan dari "Developer PowerShell for VS 2022" |

Ruang disk: sekitar 15 GB (source + build OBS). Internet diperlukan (OBS mengunduh obs-deps otomatis).

## 2. Langkah build

Buka **Developer PowerShell for VS 2022** di folder proyek:

```powershell
# (a) libobs + plugin OBS tanpa UI OBS -> deps\obs-install   (sekali saja, 15-40 menit)
.\scripts\build-obs.ps1                  # opsional: -ObsTag 32.2.2

# (b) Live Studio -> build\ ; hasil: build\stage\bin\64bit\LiveStudio.exe
.\scripts\build-livestudio.ps1 -QtPrefix C:\Qt\6.8.3\msvc2022_64 -Config Release
# tambahkan -WithTests untuk membangun LiveStudioSmoke

# (c) paket + uji mandiri + installer -> dist\LiveStudio-Setup-<versi>.exe
.\scripts\package-windows.ps1 -QtPrefix C:\Qt\6.8.3\msvc2022_64 -Config Release
```

Skrip (c) menjalankan `cmake --install`, `windeployqt`, mengunduh VC++ Redistributable, menjalankan
`LiveStudio.exe --selftest --quiet`, lalu memanggil Inno Setup. Periksa keluaran selftest sebelum merilis.

## 3. Build manual (tanpa skrip)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.8.3/msvc2022_64" `
  -DOBS_INSTALL_DIR="C:/path/deps/obs-install" `
  -DLIVESTUDIO_OBS_RUNTIME_DIR="C:/path/deps/obs-install"
cmake --build build --config Release --parallel
```

Opsi CMake: `OBS_INSTALL_DIR`, `LIVESTUDIO_OBS_RUNTIME_DIR` (sumber DLL/plugin/data yang disalin saat
install), `W32_PTHREADS_INCLUDE_DIR` (folder berisi `pthread.h`), `LIVESTUDIO_BUILD_TESTS`.

Tata letak runtime yang diharapkan aplikasi (mengikuti OBS):

```
<root>\bin\64bit\LiveStudio.exe  (+ obs.dll, Qt DLL, FFmpeg DLL)
<root>\obs-plugins\64bit\*.dll
<root>\data\libobs, <root>\data\obs-plugins\...
```

## 4. Pemecahan masalah

| Gejala | Penyebab / solusi |
|---|---|
| `libobs tidak ditemukan` | Isi `-DOBS_INSTALL_DIR` dengan folder hasil `cmake --install` OBS, atau `-Dlibobs_DIR=<folder libobsConfig.cmake>`. |
| `pthread.h: No such file` | Set `-DW32_PTHREADS_INCLUDE_DIR=<deps\obs-studio\.deps\obs-deps-...\include>`. |
| `Qt6 tidak ditemukan` | Berikan `-QtPrefix` ke folder `msvc2022_64` (bukan `mingw`). |
| Konfigurasi OBS gagal: `Visual Studio 18 2026` tidak ditemukan | Preset bawaan OBS 32.x butuh VS 2026. `build-obs.ps1` kini memakai `-Generator "Visual Studio 17 2022"` (bisa diganti lewat parameter). |
| Dialog `avcodec-62.dll / avformat-62.dll / avutil-60.dll / swscale-9.dll was not found` | DLL FFmpeg dari paket obs-deps tidak ikut terpasang (`cmake --install` OBS tidak menyalinnya). `package-windows.ps1` kini menyalin `deps\obs-studio\.deps\obs-deps-*\bin\*.dll` ke `bin\64bit` dan memeriksa dependensi dengan `dumpbin`; build berhenti dengan daftar `HILANG: ...` bila masih ada yang kurang. |
| Dialog `Entry Point Not Found ... obs-vst.dll` (atau plugin lain yang memakai Qt) | Plugin OBS yang dibangun dengan Qt bawaan obs-deps tidak cocok dengan Qt aplikasi. `obs-vst`, `decklink-*-ui`, `aja-output-ui` kini dikecualikan dari install dan ditolak saat start. Bila ada plugin lain: hapus DLL-nya dari `obs-plugins\64bit` dan tambahkan namanya ke `isDeniedModule()` di `src/core/ObsEngine.cpp`. |
| Aplikasi berhenti saat start | Buka `%APPDATA%\LiveStudio\logs\*.log`; biasanya `obs-plugins`/`data` tidak ikut tersalin. Jalankan `--selftest`. |
| Kamera tidak muncul | Pastikan `win-dshow.dll` ada di `obs-plugins\64bit` dan privasi kamera Windows mengizinkan aplikasi desktop. |
| Galat kompilasi karena API libobs | Catat versi OBS dan baris galat; API berbeda antarversi mayor. |

## 5. CI

`.github/workflows/windows.yml` menjalankan langkah yang sama di GitHub Actions (eksperimental, belum
pernah dijalankan). Jalankan manual lewat *Run workflow*.
