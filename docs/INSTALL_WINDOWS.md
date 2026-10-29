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

## Alur pakai singkat

1. **Tambah sumber** (menu *Tambah Sumber*): Kamera, Layar, Jendela, Video, Gambar, Teks, Mikrofon, Audio Desktop.
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
