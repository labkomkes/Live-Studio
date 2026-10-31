# Memasang dan memakai Live Studio (Windows)

> Fitur 0.4.0 baru diuji di Linux (lihat `STATUS.md`); verifikasi di Windows menunggu laporan Anda.

## Memasang

1. Jalankan `LiveStudio-Setup-<versi>.exe` (butuh hak administrator).
2. Setujui lisensi, pilih folder (bawaan `C:\Program Files\Live Studio`), centang pintasan Desktop bila perlu.
3. Jika Microsoft Visual C++ Runtime belum ada, installer memasangnya otomatis.
4. Selesai. Pintasan: Start Menu > **Live Studio** dan **Live Studio - Uji Mandiri**.

Mencopot: Settings > Apps > Live Studio. Data pengguna di `%APPDATA%\LiveStudio` (scene, log) **tidak dihapus**.

## Uji pertama (disarankan)

Jalankan **Live Studio - Uji Mandiri**. Laporan tersimpan di `%APPDATA%\LiveStudio\selftest.txt`.
`FAIL` pada kamera/monitor/WASAPI berarti plugin tidak termuat; kirim laporan itu.

## Tampilan

- **Atas (gaya OBS Studio Mode):** monitor **Preview** (kiri), tombol **CUT** dan **TAKE** (tengah), monitor **Program** (kanan). Di bawahnya panel Adegan, Sumber, Audio Mixer, dan tab Lower Third / Bumper.
- **Bawah (gaya vMix), deck:** satu tile per scene dengan gambar live. Border **merah = Program**, **hijau = Preview**. Klik tile = Preview; tombol **CUT** pada tile = langsung ke Program; tombol **TAKE** pada tile = ke Program dengan transisi; klik dua kali = CUT; roda mouse menggeser daftar bila tile tidak muat. Di kanan deck: tombol transisi (Cut, Fade, Swipe, Slide, Warna), durasi, **Lower Third**, **＋ Sumber**, dan **Output** (kirim ke monitor lain).
- **Mode Sederhana** (bawaan) menyembunyikan panel Adegan, Sumber, dan tab Bumper, serta menu Multiview. **Mode Profesional**: menu *Tampilan* > Mode Profesional.
- Pintasan: **1–9** pilih scene ke Preview, **Spasi** = TAKE, **Enter** = CUT.

## Alur pakai singkat

1. **Tambah sumber** (tombol **＋ Sumber** di deck, atau panel Sumber di mode Profesional): Kamera, Layar, Jendela, Video, Gambar, Teks, Mikrofon, Audio Desktop.
   Sumber ditambahkan ke scene yang sedang berada di **Preview**.
2. Atur scene di Preview; tekan **CUT** (instan) atau **TAKE** (transisi) untuk menayangkan ke **Program**.
3. **Audio mixer**: atur volume dan Mute per sumber.
4. **Lower third**: isi nama dan jabatan, tekan **TAMPILKAN** / **SEMBUNYIKAN** (ada sembunyi otomatis dan preset).
5. **Bumper**: pilih berkas video, tekan **Putar bumper**; setelah selesai (atau **Hentikan**) Program kembali ke scene sebelumnya. Scene "Segera Dimulai" tersedia sebagai scene awal.
6. **Proyektor / Multiview**: menu *Multiview* (mode Profesional) dan menu keluaran Preview/Program membuka jendela di monitor yang dipilih.
7. Mode **Sederhana** vs **Profesional** lewat menu *Tampilan* > Mode Profesional.

## Pemutar video, audio, rekam, siaran (0.4.0)

- **Pemutar video**: setelah menambah video, tab **Pemutar Video** terbuka. Video berhenti di frame awal;
  tekan ▶ untuk memutar, ⏸ untuk jeda, ⏮ ulang dari awal, ⏹ berhenti, geser bilah untuk loncat, centang
  *Ulangi* untuk loop. Waktu tampil sebagai `posisi / durasi`. Pintasan: Ctrl+P putar/jeda, Ctrl+Shift+P ulang.
- **Suara tidak keluar di komputer?** libobs hanya memutar ke speaker lewat *monitoring*. Buka **Audio ⚙**
  (bar bawah), pilih perangkat keluaran monitor, lalu pastikan tombol **Dengar** di Audio Mixer menyala untuk
  sumber itu. Video otomatis Dengar. Jangan menyalakan Dengar pada mikrofon tanpa headphone (gema).
- **Audio Mixer (0.4.2)**: tiap sumber punya meter stereo L/R berskala dB (indikator CLIP merah bila menyentuh 0 dB),
  fader + kolom dB yang bisa diketik (klik dua kali fader = 0 dB), tombol **Mute** (menjadi merah *MUTED*),
  **Dengar**, dan menu **⚙**: mode monitor, filter (peredam bising, noise gate, kompresor, limiter, penguat, expander),
  paksa mono, keseimbangan kiri/kanan, offset sinkron, ganti perangkat. Bilah atas mixer: **＋ Input**, **Mute semua**,
  tombol tampilan **Kanal/Daftar**, dan **⚙ Audio**. Batas antara monitor dan panel bawah bisa digeser untuk memperbesar mixer.
- **Input audio & driver**: *＋ Sumber > Input Audio (pilih driver)…*: Driver Windows (WASAPI) untuk mikrofon/line-in/mixer USB,
  audio satu aplikasi, ASIO (butuh plugin ASIO pihak ketiga, lihat Plugin), atau DirectShow (kartu capture). Beberapa driver belum teruji (lihat STATUS).
- **Plugin**: tombol **Plugin** di bar bawah (atau File > Plugin…) menampilkan plugin termuat dan membolehkan folder plugin tambahan (berlaku setelah buka ulang).
- **Output ke monitor**: di deck, bagian *KIRIM KE MONITOR*: **Program**, **Preview**, **Multiview** — klik, pilih monitor; tanda ● berarti aktif; pilih monitor lain untuk memindah, atau *Tutup* / Esc.
- **Rekam / Siaran**: tombol **● Rekam** (Ctrl+R) dan **▶ Siaran** (Ctrl+L) di bar bawah. Atur folder, server,
  kunci, dan bitrate lewat **Output ⚙**. Rekaman berformat .mkv. Bar status menampilkan waktu, kbps, frame drop, FPS, CPU.
- **Tata letak**: panel Sumber (mode Profesional) > *Tata letak ▾*: Layar penuh, Dua kolom, Pembicara + slide, PiP.

## Lokasi berkas

| Isi | Lokasi |
|---|---|
| Scene & pengaturan | `%APPDATA%\LiveStudio\` |
| Log | `%APPDATA%\LiveStudio\logs\` |
| Laporan uji mandiri | `%APPDATA%\LiveStudio\selftest.txt` |
