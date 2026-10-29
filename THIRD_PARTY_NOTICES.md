# Pemberitahuan komponen pihak ketiga

Live Studio dilisensikan **GPL-2.0-or-later** (lihat `LICENSE.txt`) karena ditautkan dengan libobs.

| Komponen | Lisensi | Sumber |
|---|---|---|
| libobs, plugin OBS (win-capture, win-dshow, win-wasapi, obs-ffmpeg, obs-transitions, text/image source) | GPL-2.0-or-later | https://github.com/obsproject/obs-studio |
| FFmpeg (DLL dari paket dependensi OBS) | LGPL-2.1+ / GPL (tergantung build) | https://ffmpeg.org |
| Qt 6 (QtCore, QtGui, QtWidgets) | LGPL-3.0 / GPL (dilink dinamis lewat DLL) | https://www.qt.io |
| Dependensi OBS lain (x264, zlib, dll.) | lihat paket obs-deps | https://github.com/obsproject/obs-deps |

Saat mendistribusikan installer, Anda wajib menyertakan atau menawarkan kode sumber lengkap
Live Studio dan menyertakan teks lisensi di atas. Teks lisensi Qt (LGPL-3.0) dan FFmpeg
disalin dari paket masing-masing ke folder `licenses/` oleh `scripts/package-windows.ps1`
bila tersedia di folder sumber; periksa hasilnya sebelum rilis.

"OBS" dan "OBS Studio" adalah merek proyek OBS. Live Studio adalah aplikasi independen dan
tidak berafiliasi dengan atau didukung oleh proyek OBS.
