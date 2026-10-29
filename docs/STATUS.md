# Status pengujian (jujur)

Terakhir diperbarui: 2026-10-29, versi 0.3.1.

## Sudah diuji, dan hasilnya

Lingkungan: Linux x86_64, Qt 6.4.2, libobs 30.0.2 (paket Ubuntu), Xvfb (tanpa GPU, tanpa kamera/audio).

| Pengujian | Hasil |
|---|---|
| Kompilasi seluruh kode (`livestudio_core`, `LiveStudio`, `LiveStudioSmoke`) | Berhasil, tanpa galat |
| `LiveStudio --selftest` | 21 PASS, 0 FAIL; 10 WARN (sumber khusus Windows dan perangkat tidak ada, wajar di Linux) |
| `LiveStudioSmoke` (GUI nyata, 17 pemeriksaan) | 17 PASS: awal Program/Preview, **CUT**, **TAKE**, buka Multiview, lower third tampil/sembunyi, strip mixer muncul/hilang, meter bergerak, **Mute**, bumper tayang lalu kembali |

Artinya logika inti (engine libobs, scene, transisi, teks/video/gambar/warna, mixer, lower third,
bumper, alur shutdown) berjalan pada libobs sungguhan.

## BELUM diuji (jangan dianggap berfungsi)

- **Semua yang khusus Windows**: kompilasi dengan MSVC, `build-obs.ps1`, `build-livestudio.ps1`,
  `package-windows.ps1`, workflow GitHub Actions, dan skrip Inno Setup `.iss`. Tidak ada Windows
  di lingkungan pengembangan, jadi **installer `.exe` belum pernah dibuat maupun dijalankan**.
- Sumber Windows: kamera/webcam (`dshow_input`), capture monitor dan jendela, audio WASAPI
  (mikrofon dan audio desktop). Kodenya ada, tetapi tidak pernah dijalankan dengan perangkat nyata.
- Tampilan visual sebenarnya (render GPU Direct3D 11), kualitas gambar, sinkron audio-video.
- Pemilihan monitor / proyektor pada konfigurasi multi-monitor sungguhan.
- Kinerja (CPU/GPU) pada sesi panjang, stabilitas berjam-jam.
- Siaran/rekaman: Live Studio **belum** memiliki output streaming (RTMP) maupun rekaman.
  Itu fitur berikutnya, bukan bagian dari daftar saat ini.

## Risiko yang diketahui pada build Windows

- Tata letak instalasi OBS berbeda antarversi; `CMakeLists.txt` mencoba paket `libobs` resmi lalu
  pencarian manual. Jika gagal, lihat bagian Pemecahan masalah di `BUILD_WINDOWS.md`.
- Header libobs di Windows membutuhkan `pthread.h` (w32-pthreads dari obs-deps); skrip mengatur
  jalur pencariannya, tetapi belum terverifikasi.
- Tag OBS default `32.2.2`; Linux diuji dengan libobs 30.0.2. API yang dipakai stabil, namun
  perbedaan versi mungkin memunculkan galat kompilasi kecil di Windows.

Mohon laporkan hasil build/uji Windows Anda (teks galat lengkap atau isi `selftest.txt`) agar
status ini bisa diperbarui berdasarkan fakta.
