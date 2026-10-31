# Rencana uji manual (Windows)

Isi kolom Hasil dengan PASS/FAIL dan catatan. Uji otomatis (`--selftest`, `LiveStudioSmoke`) **tidak**
mencakup tampilan visual, kualitas kamera, level audio nyata, maupun multi-monitor.

| # | Uji | Langkah | Diharapkan | Hasil |
|---|---|---|---|---|
| 1 | Start | Jalankan LiveStudio.exe | Jendela terbuka, Preview & Program tampil, tanpa dialog galat | |
| 2 | Selftest | `LiveStudio.exe --selftest` | Tidak ada FAIL pada tipe sumber Windows | |
| 3 | Webcam | Tambah Sumber > Kamera, pilih perangkat | Gambar kamera muncul di Preview | |
| 4 | Resolusi kamera | Pilih resolusi tertentu | Gambar mengikuti pilihan | |
| 5 | Layar | Tambah > Layar, pilih monitor | Isi monitor tampil di Preview | |
| 6 | Jendela | Tambah > Jendela | Jendela terpilih tampil | |
| 7 | Video | Tambah file mp4, loop | Video diputar, audio muncul di mixer | |
| 8 | Gambar & teks | Tambah png dan teks | Tampil, teks terbaca | |
| 9 | CUT | Ubah Preview, tekan CUT | Program berganti seketika, label tertukar | |
| 10 | TAKE | Tekan TAKE (Fade 400 ms) | Program berganti dengan transisi halus | |
| 11 | Mixer | Bicara ke mikrofon | Meter bergerak; Mute meredam | |
| 12 | Audio desktop | Putar musik | Meter audio desktop bergerak | |
| 13 | Lower third | Isi nama/jabatan, TAMPILKAN | Muncul di Program, SEMBUNYIKAN menghilangkan | |
| 14 | Bumper | Pilih video, Putar bumper; biarkan selesai | Program kembali ke scene sebelumnya | |
| 15 | Multiview | Menu Multiview > pilih monitor ke-2 (mode Profesional) | Jendela Multiview terbuka di monitor itu, scene tampil | |
| 16 | Proyektor | Program ke monitor ke-2 layar penuh | Tampil benar, Esc menutup | |
| 17 | Simpan | Buat scene, tutup, buka lagi | Scene & sumber kembali | |
| 18 | Tutup bersih | Tutup jendela | Proses berakhir, tidak ada crash/hang | |
| 19 | Stabil | Biarkan 1 jam dengan kamera + layar | Tidak crash, memori stabil | |
| 21 | Deck: tile | Klik tile scene di deck | Preview berganti ke scene itu; border hijau | |
| 22 | Deck: CUT tile | Klik tombol CUT pada sebuah tile | Program langsung berganti; border merah | |
| 23 | Deck: TAKE tile | Klik tombol TAKE pada tile | Program berganti dengan transisi | |
| 24 | Deck: transisi | Pilih Cut / Fade / Swipe; ubah durasi | TAKE memakai transisi terpilih | |
| 25 | Deck: banyak scene | Buat 12+ scene; putar roda mouse di deck | Daftar tile bergeser, klik tetap tepat | |
| 26 | Deck: tampilan layar kecil / DPI 150% | Jalankan di laptop 1366x768 dan skala 150% | Semua kontrol terlihat; klik tile tepat | |
| 20 | Installer | Pasang di PC bersih, jalankan, copot | Terpasang, jalan, tercopot | |
| 27 | Video: kontrol | Tambah video; tekan ▶, ⏸, ⏮, ⏹, geser bilah | Putar/jeda/ulang/loncat bekerja, waktu berjalan | |
| 28 | Suara terdengar | Audio ⚙ pilih speaker; video Dengar aktif; putar | Suara keluar di speaker | |
| 29 | Dengar mikrofon | Nyalakan Dengar pada mic (pakai headphone) | Suara mic terdengar di headphone | |
| 30 | Filter | Mic > Filter > Peredam bising / Kompresor | Meter dan suara berubah | |
| 31 | Rekam | Ctrl+R, 10 dtk, Ctrl+R | File .mkv di folder rekaman, video+audio ada | |
| 32 | Siaran | Isi server+kunci, Ctrl+L | Platform menerima aliran; kbps/drop tampil; henti menutup | |
| 33 | Tata letak | Dua sumber video > Tata letak > PiP | Susunan berubah di Preview | |
