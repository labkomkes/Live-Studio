// Live Studio - fungsi gambar untuk display libobs (dipanggil di graphics thread).
#pragma once

#include <cstdint>

class QString;

namespace ls {
class SceneManager;
}

namespace ls::render {

// Menggambar kanvas program/preview dengan letterbox ke seluruh display.
void drawPreview(SceneManager *sm, uint32_t cx, uint32_t cy);
void drawProgram(SceneManager *sm, uint32_t cx, uint32_t cy);
// Grid multiview: Program + Preview (baris atas) dan hingga 8 scene (baris bawah).
void drawMultiview(SceneManager *sm, uint32_t cx, uint32_t cy);

// Panggil dari UI thread sebelum ObsEngine::shutdown().
void releaseGraphicsResources();
// Panggil saat daftar/nama scene berubah agar label multiview dibuat ulang.
void clearLabelCache();

} // namespace ls::render
