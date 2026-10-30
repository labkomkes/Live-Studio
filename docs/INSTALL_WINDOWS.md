# Memasang dan memakai Live Studio (Windows)

> Installer belum pernah dibuat/dijalankan di Windows (lihat `STATUS.md`). Panduan ini menjelaskan
> perilaku yang dirancang.

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

Catatan: versi ini belum mengirim siaran (RTMP) atau merekam.

## Lokasi berkas

| Isi | Lokasi |
|---|---|
| Scene & pengaturan | `%APPDATA%\LiveStudio\` |
| Log | `%APPDATA%\LiveStudio\logs\` |
| Laporan uji mandiri | `%APPDATA%\LiveStudio\selftest.txt` |
