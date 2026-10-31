# Status pengujian (jujur)

Terakhir diperbarui: 2026-10-31, versi 0.4.0.

## Sudah diuji, dan hasilnya

Lingkungan: Linux x86_64, Qt 6.4.2, libobs 30.0.2 (paket Ubuntu), Xvfb (tanpa GPU, tanpa kamera/audio).

| Pengujian | Hasil |
|---|---|
| Kompilasi seluruh kode (`livestudio_core`, `LiveStudio`, `LiveStudioSmoke`) | Berhasil, tanpa galat |
| `LiveStudio --selftest` | 21 PASS, 0 FAIL; 10 WARN (sumber khusus Windows dan perangkat tidak ada, wajar di Linux) |
| `LiveStudioSmoke` (GUI nyata, 60 pemeriksaan) | 60 PASS, 0 FAIL (lihat rincian fitur 0.4.0 di bawah). Mencakup: awal Program/Preview, **CUT**, **TAKE**, **deck** (klik tile, CUT/TAKE per tile, pilih transisi), buka Multiview, lower third tampil/sembunyi, strip mixer muncul/hilang, meter bergerak, **Mute**, bumper tayang lalu kembali |

Artinya logika inti (engine libobs, scene, transisi, teks/video/gambar/warna, mixer, lower third,
bumper, alur shutdown) berjalan pada libobs sungguhan.

## Fitur baru 0.4.0 dan hasil uji di Linux (libobs 30.0.2, Xvfb)

| Fitur | Diuji | Hasil |
|---|---|---|
| Pemutar video: Putar/Jeda, Ulang, Stop, geser (seek), waktu "posisi / durasi", Ulangi | tombol/aksi panel pada video 12 dtk | PASS: durasi terbaca, PLAYING, PAUSED (posisi diam saat jeda), lanjut, seek ke 0:08, restart |
| Video baru berada di frame awal (cue) sampai tombol Putar ditekan | otomatis | PASS |
| Audio: video otomatis mode Dengar (monitor + output), tombol Dengar per sumber, filter (Penguat, Kompresor), offset sinkron | tombol/aksi strip | PASS untuk keadaan dan pemasangan filter; **bunyi di speaker tidak dapat diuji di sini** (tanpa perangkat audio) |
| Tata letak otomatis: Dua kolom, PiP (juga Layar penuh, Pembicara + slide) | posisi item dibaca ulang | PASS untuk Dua kolom dan PiP; dua lainnya belum diuji |
| Rekam (x264 + AAC, .mkv) | rekam 3 dtk, `ffprobe` | PASS: durasi ~3 dtk, trek video dan audio ada |
| Siaran RTMP | (a) tolak bila kunci kosong, (b) server mati dilaporkan gagal, (c) siaran ke penerima RTMP lokal (`ffmpeg -listen`) | PASS ketiganya, termasuk data diterima dan terbaca `ffprobe`. Belum diuji ke YouTube/Facebook/Twitch |
| Bar status: REC/LIVE, waktu, kbps, frame drop, FPS, CPU | tampil di tangkapan layar | Tampil; angka kbps/drop tidak diverifikasi akurat |

Temuan saat menguji: `obs_output_release` hang bila `obs_output_force_stop` dipanggil pada output yang sudah
berhenti; sudah diperbaiki (hanya force-stop output yang aktif).

**Belum diverifikasi di Windows** untuk fitur 0.4.0: apakah suara terdengar di speaker (butuh
perangkat monitoring WASAPI), x264 dan ffmpeg_muxer dari build OBS 32.2.2 termuat, dan tampilan baru.

## Hasil nyata di Windows (dilaporkan pengguna)

- Build OBS 32.2.2, build Live Studio (MSVC), windeployqt, dan installer berhasil dibuat lewat GitHub Actions.
- Menjalankan installer hasil build pertama gagal start: `avcodec-62.dll`, `avformat-62.dll`, `avutil-60.dll`, `swscale-9.dll` tidak ditemukan. Diperbaiki di `package-windows.ps1` (0.3.1-r5); perbaikan ini belum diverifikasi di Windows.

- Galat kedua di Windows: `obs-vst.dll` -> *Entry Point Not Found* (`QObjectData::dynamicMetaObject`): plugin itu dibangun terhadap Qt bawaan obs-deps, berbeda dari Qt 6.8.3 aplikasi. Perbaikan (0.3.1-r6): plugin Qt/frontend OBS (`obs-vst`, `decklink-captions`, `decklink-output-ui`, `aja-output-ui`) tidak dipasang dan ditolak saat memuat modul; Windows tidak lagi menampilkan dialog modal bila sebuah plugin gagal dimuat. Belum diverifikasi di Windows.
- Tampilan baru (OBS di atas, deck vMix di bawah) diuji hanya di Linux/Xvfb (render OpenGL, tangkapan layar diperiksa manual). Tampilan di Windows (Direct3D 11, font GDI+, DPI) belum pernah dilihat.

## BELUM diuji (jangan dianggap berfungsi)

- **Verifikasi di Windows**: installer sudah berhasil dibuat lewat GitHub Actions, tetapi baru satu kali
  dijalankan dan langsung gagal start (DLL FFmpeg hilang, lihat di atas). Setelah perbaikan, belum ada
  laporan bahwa aplikasi benar-benar terbuka, bahwa `--selftest` lulus di Windows, atau bahwa
  uninstall bersih.
- Sumber Windows: kamera/webcam (`dshow_input`), capture monitor dan jendela, audio WASAPI
  (mikrofon dan audio desktop). Kodenya ada, tetapi tidak pernah dijalankan dengan perangkat nyata.
- Tampilan visual sebenarnya (render GPU Direct3D 11), kualitas gambar, sinkron audio-video.
- Pemilihan monitor / proyektor pada konfigurasi multi-monitor sungguhan.
- Kinerja (CPU/GPU) pada sesi panjang, stabilitas berjam-jam.
- Encoder hardware (NVENC/AMD/QSV) belum ada; hanya x264 (CPU).
- Kunci siaran yang diingat disimpan tanpa enkripsi di pengaturan pengguna.
- Seek pada video mendarat di keyframe terdekat sebelum posisi yang diminta (perilaku libobs); pada
  video dengan keyframe jarang, posisi bisa meleset beberapa detik.
- Hotkey dapat diubah pengguna belum ada (hanya pintasan tetap).

## Risiko yang diketahui pada build Windows

- Tata letak instalasi OBS berbeda antarversi; `CMakeLists.txt` mencoba paket `libobs` resmi lalu
  pencarian manual. Jika gagal, lihat bagian Pemecahan masalah di `BUILD_WINDOWS.md`.
- Header libobs di Windows membutuhkan `pthread.h` (w32-pthreads dari obs-deps); skrip mengatur
  jalur pencariannya, tetapi belum terverifikasi.
- Tag OBS default `32.2.2`; Linux diuji dengan libobs 30.0.2. API yang dipakai stabil, namun
  perbedaan versi mungkin memunculkan galat kompilasi kecil di Windows.

Mohon laporkan hasil build/uji Windows Anda (teks galat lengkap atau isi `selftest.txt`) agar
status ini bisa diperbarui berdasarkan fakta.
