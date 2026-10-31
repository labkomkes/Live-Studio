// Live Studio - fungsi gambar untuk display libobs (dipanggil di graphics thread).
#pragma once

#include <cstdint>

class QString;

namespace ls {
class SceneManager;
struct LedLayout;
}

namespace ls::render {

// Menggambar kanvas program/preview dengan letterbox ke seluruh display.
void drawPreview(SceneManager *sm, uint32_t cx, uint32_t cy);
void drawProgram(SceneManager *sm, uint32_t cx, uint32_t cy);

// Keadaan editor Preview (UI thread -> graphics thread). Koordinat = piksel kanvas.
struct EditorView {
	bool hasSel = false;
	float x = 0, y = 0, w = 0, h = 0; // kotak item terpilih
	int hoverHandle = -1;              // 0..7 = TL,T,TR,R,BR,B,BL,L
	bool snapX = false, snapY = false; // garis bantu saat menempel ke tengah/tepi
	float snapXPos = 0, snapYPos = 0;
};
// Preview + bingkai seleksi + 8 pegangan ubah ukuran.
void drawPreviewEditor(SceneManager *sm, uint32_t cx, uint32_t cy, const EditorView &v);
// Grid multiview: Program + Preview (baris atas) dan hingga 8 scene (baris bawah).
void drawMultiview(SceneManager *sm, uint32_t cx, uint32_t cy);

// Keadaan tampilan deck (diisi widget DeckStrip dari UI thread, dibaca graphics thread).
struct DeckView {
	int offset = 0;     // scene pertama yang terlihat (geser roda mouse)
	int hoverSlot = -1; // tile di bawah kursor
	int hoverZone = 0;  // ls::deck::Zone
	float dpr = 1.f;    // device pixel ratio widget
};
// Strip tile scene ala vMix: thumbnail live, nomor, nama, tombol CUT/TAKE per tile.
void drawDeck(SceneManager *sm, uint32_t cx, uint32_t cy, const DeckView &view);

// Layar LED: menggambar semua slice (potongan scene -> posisi di layar LED); letterbox ke window.
void drawLed(SceneManager *sm, const LedLayout &layout, uint32_t cx, uint32_t cy);

// Panggil dari UI thread sebelum ObsEngine::shutdown().
void releaseGraphicsResources();
// Panggil saat daftar/nama scene berubah agar label multiview dibuat ulang.
void clearLabelCache();

} // namespace ls::render
