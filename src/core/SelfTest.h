// Live Studio - uji mandiri non-interaktif: `LiveStudio.exe --selftest`.
// Menulis laporan ke %APPDATA%\LiveStudio\selftest.txt dan menampilkannya.
#pragma once

namespace ls {

// Dipanggil setelah ObsEngine::start() berhasil. Mengembalikan 0 bila tidak ada FAIL.
// Tidak menyimpan apa pun ke koleksi scene pengguna.
// showDialog=false: cetak ke stdout saja (CI / headless).
int runSelfTest(bool showDialog = true);

} // namespace ls
