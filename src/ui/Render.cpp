#include "Render.h"

#include "../core/ObsEngine.h"
#include "../core/Raii.h"
#include "../core/SceneManager.h"
#include "../core/SourceFactory.h"
#include "DeckLayout.h"

#include <obs.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <string>

namespace ls::render {

static gs_vertbuffer_t *g_box = nullptr;
static gs_vertbuffer_t *g_quad = nullptr;
static std::map<std::string, SourceRef> g_labels;
static std::mutex g_labelMutex;

// Menggambar `src` (ukuran kanvas baseW x baseH) ke dalam rect (x,y,w,h) dengan letterbox.
static void drawSourceInto(obs_source_t *src, float baseW, float baseH, float x, float y, float w, float h)
{
	if (!src || baseW <= 0.f || baseH <= 0.f || w <= 1.f || h <= 1.f)
		return;
	const float scale = std::min(w / baseW, h / baseH);
	const float newW = baseW * scale;
	const float newH = baseH * scale;
	const float ox = x + (w - newW) / 2.f;
	const float oy = y + (h - newH) / 2.f;

	gs_viewport_push();
	gs_projection_push();
	gs_ortho(0.f, baseW, 0.f, baseH, -100.f, 100.f);
	gs_set_viewport(int(ox), int(oy), int(newW), int(newH));
	obs_source_video_render(src);
	gs_projection_pop();
	gs_viewport_pop();
}

static void canvasSize(float &w, float &h)
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	w = float(ovi.base_width);
	h = float(ovi.base_height);
}

static void ensureBox()
{
	if (g_box)
		return;
	gs_render_start(true);
	gs_vertex2f(0.f, 0.f);
	gs_vertex2f(0.f, 1.f);
	gs_vertex2f(1.f, 1.f);
	gs_vertex2f(1.f, 0.f);
	gs_vertex2f(0.f, 0.f);
	g_box = gs_render_save();
}

// Bingkai kotak berwarna (rgba = 0xAABBGGRR) dengan ketebalan piksel.
static void drawOutline(float x, float y, float w, float h, uint32_t abgr, int thickness)
{
	ensureBox();
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *colorParam = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 col;
	vec4_from_rgba(&col, abgr);
	gs_effect_set_vec4(colorParam, &col);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_load_vertexbuffer(g_box);
	for (int t = 0; t < thickness; ++t) {
		gs_matrix_push();
		gs_matrix_translate3f(x + t, y + t, 0.f);
		gs_matrix_scale3f(std::max(1.f, w - 2.f * t), std::max(1.f, h - 2.f * t), 1.f);
		gs_draw(GS_LINESTRIP, 0, 0);
		gs_matrix_pop();
	}
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static obs_source_t *labelSource(const std::string &text)
{
	std::lock_guard<std::mutex> lock(g_labelMutex);
	auto it = g_labels.find(text);
	if (it != g_labels.end())
		return it->second.get();

	const QString id = factory::textId();
	if (!ObsEngine::isAvailable(id))
		return nullptr;
	Data d = factory::textSettings(QString::fromStdString(text), 48, QColor(255, 255, 255), true);
	obs_data_set_int(d.get(), "bk_color", 0);
	obs_data_set_int(d.get(), "bk_opacity", 60);
	obs_source_t *s = obs_source_create_private(id.toUtf8().constData(), "LS Label", d.get());
	if (!s)
		return nullptr;
	auto res = g_labels.emplace(text, SourceRef(s));
	return res.first->second.get();
}

static void drawLabel(const std::string &text, float cellX, float cellY, float cellH)
{
	obs_source_t *lbl = labelSource(text);
	if (!lbl)
		return;
	const float lw = float(obs_source_get_width(lbl));
	const float lh = float(obs_source_get_height(lbl));
	if (lw <= 0.f || lh <= 0.f)
		return; // teks belum dirender (tick pertama)
	const float scale = (cellH * 0.13f) / lh;
	drawSourceInto(lbl, lw, lh, cellX + 6.f, cellY + 6.f, lw * scale, lh * scale);
}

void drawPreview(SceneManager *sm, uint32_t cx, uint32_t cy)
{
	SourceRef src = sm->previewSourceRef();
	float bw, bh;
	canvasSize(bw, bh);
	drawSourceInto(src.get(), bw, bh, 0.f, 0.f, float(cx), float(cy));
}

void drawProgram(SceneManager *sm, uint32_t cx, uint32_t cy)
{
	SourceRef src = sm->programSourceRef(); // transisi (channel output 0)
	float bw, bh;
	canvasSize(bw, bh);
	drawSourceInto(src.get(), bw, bh, 0.f, 0.f, float(cx), float(cy));
}

void drawMultiview(SceneManager *sm, uint32_t cx, uint32_t cy)
{
	float bw, bh;
	canvasSize(bw, bh);
	const float W = float(cx);
	const float H = float(cy);
	const float m = 6.f; // margin

	const QString progName = sm->programName();
	const QString prevName = sm->previewName();

	// Baris atas: Program (kiri) & Preview (kanan).
	const float topH = H * 0.5f;
	const float halfW = W * 0.5f;
	{
		SourceRef prog = sm->programSourceRef();
		drawSourceInto(prog.get(), bw, bh, m, m, halfW - 2 * m, topH - 2 * m);
		drawOutline(m - 2, m - 2, halfW - 2 * m + 4, topH - 2 * m + 4, 0xFF2020E0, 3); // merah
		drawLabel("PROGRAM", m, m, topH);

		SourceRef prev = sm->previewSourceRef();
		drawSourceInto(prev.get(), bw, bh, halfW + m, m, halfW - 2 * m, topH - 2 * m);
		drawOutline(halfW + m - 2, m - 2, halfW - 2 * m + 4, topH - 2 * m + 4, 0xFF20C020, 3); // hijau
		drawLabel("PREVIEW", halfW + m, m, topH);
	}

	// Baris bawah: hingga 8 scene (4 x 2).
	const auto cells = sm->scenesForDraw();
	const int cols = 4;
	const int maxCells = 8;
	const float cellW = W / cols;
	const float cellH = (H - topH) / 2.f;
	for (int i = 0; i < std::min<int>(cells.size(), maxCells); ++i) {
		const int col = i % cols;
		const int row = i / cols;
		const float x = col * cellW;
		const float y = topH + row * cellH;
		const auto &c = cells.at(i);
		drawSourceInto(c.source.get(), bw, bh, x + m, y + m, cellW - 2 * m, cellH - 2 * m);

		uint32_t border = 0xFF606060;
		if (c.name == progName)
			border = 0xFF2020E0;
		else if (c.name == prevName)
			border = 0xFF20C020;
		drawOutline(x + m - 2, y + m - 2, cellW - 2 * m + 4, cellH - 2 * m + 4, border, 2);

		const std::string label = QString("%1  %2").arg(i + 1).arg(c.name).toStdString();
		drawLabel(label, x + m, y + m, cellH);
	}
}

// ---------------------------------------------------------------- deck (gaya vMix)

static void ensureQuad()
{
	if (g_quad)
		return;
	gs_render_start(true);
	gs_vertex2f(0.f, 0.f);
	gs_vertex2f(1.f, 0.f);
	gs_vertex2f(0.f, 1.f);
	gs_vertex2f(1.f, 1.f);
	g_quad = gs_render_save();
}

// Kotak terisi warna (0xAABBGGRR).
static void fillRect(float x, float y, float w, float h, uint32_t abgr)
{
	if (w <= 0.f || h <= 0.f)
		return;
	ensureQuad();
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *colorParam = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	vec4 col;
	vec4_from_rgba(&col, abgr);
	gs_effect_set_vec4(colorParam, &col);
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_load_vertexbuffer(g_quad);
	gs_matrix_push();
	gs_matrix_translate3f(x, y, 0.f);
	gs_matrix_scale3f(w, h, 1.f);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

// Teks muat di dalam rect (x,y,w,h); center=false -> rata kiri. Tinggi huruf dinormalisasi memakai
// teks acuan ("Hg") karena tinggi sumber teks bergantung pada huruf yang dipakai (mis. ada/tidak ada ekor "g").
static void drawTextIn(const std::string &text, float x, float y, float w, float h, bool center, float frac = 0.8f)
{
	obs_source_t *lbl = labelSource(text);
	if (!lbl)
		return;
	const float lw = float(obs_source_get_width(lbl));
	const float lh = float(obs_source_get_height(lbl));
	if (lw <= 0.f || lh <= 0.f)
		return; // teks belum dirender (tick pertama)
	obs_source_t *ref = labelSource("Hg");
	const float refH = ref ? float(obs_source_get_height(ref)) : 0.f;
	const float base = refH > 0.f ? refH : lh;
	const float scale = std::min((h * frac) / base, std::max(1.f, w - 8.f) / lw);
	const float dw = lw * scale;
	const float dh = lh * scale;
	const float ox = center ? x + (w - dw) / 2.f : x + 4.f;
	drawSourceInto(lbl, lw, lh, ox, y + (h - dh) / 2.f, dw, dh);
}

static uint32_t brighten(uint32_t abgr, int add)
{
	auto ch = [&](int shift) { return std::min(255, int((abgr >> shift) & 0xFF) + add); };
	return (abgr & 0xFF000000u) | (uint32_t(ch(16)) << 16) | (uint32_t(ch(8)) << 8) | uint32_t(ch(0));
}

void drawDeck(SceneManager *sm, uint32_t cx, uint32_t cy, const DeckView &v)
{
	float bw, bh;
	canvasSize(bw, bh);
	const float d = v.dpr > 0.f ? v.dpr : 1.f;
	const float W = float(cx) / d;
	const float H = float(cy) / d;

	const auto cells = sm->scenesForDraw();
	const QString progName = sm->programName();
	const QString prevName = sm->previewName();
	const deck::Metrics m = deck::metrics(W, H, int(cells.size()), v.offset);

	// Warna 0xAABBGGRR
	const uint32_t kProgram = 0xFF3539E5; // merah
	const uint32_t kPreview = 0xFF47A043; // hijau
	const uint32_t kIdle = 0xFF4A4A4A;
	const uint32_t kBar = 0xFF2E2E2E;
	const uint32_t kCut = 0xFF006FC7;  // amber
	const uint32_t kTake = 0xFF2B2BB3; // merah tua

	for (int slot = 0; slot < m.visible; ++slot) {
		const auto &c = cells.at(m.offset + slot);
		const deck::Tile t = deck::tile(m, slot);
		const bool isProg = c.name == progName;
		const bool isPrev = !isProg && c.name == prevName;
		const uint32_t accent = isProg ? kProgram : isPrev ? kPreview : kIdle;
		const bool hot = v.hoverSlot == slot;
		auto px = [&](float f) { return f * d; };

		// Latar thumbnail + gambar live scene.
		fillRect(px(t.thumb.x), px(t.thumb.y), px(t.thumb.w), px(t.thumb.h), 0xFF000000);
		drawSourceInto(c.source.get(), bw, bh, px(t.thumb.x), px(t.thumb.y), px(t.thumb.w), px(t.thumb.h));

		// Bar nama (berwarna bila Program/Preview).
		fillRect(px(t.nameBar.x), px(t.nameBar.y), px(t.nameBar.w), px(t.nameBar.h),
			 isProg || isPrev ? accent : (hot ? brighten(kBar, 16) : kBar));
		drawTextIn(c.name.toStdString(), px(t.nameBar.x + 26.f), px(t.nameBar.y), px(t.nameBar.w - 26.f),
			   px(t.nameBar.h), false, 0.92f);

		// Lencana nomor di kiri bar nama.
		fillRect(px(t.nameBar.x), px(t.nameBar.y), px(24.f), px(t.nameBar.h), brighten(accent, isProg || isPrev ? -40 : 10));
		drawTextIn(std::to_string(m.offset + slot + 1), px(t.nameBar.x), px(t.nameBar.y), px(24.f), px(t.nameBar.h),
			   true, 0.92f);

		// Tombol CUT / TAKE per tile (inset 1 px sebagai pemisah).
		const bool hotCut = hot && v.hoverZone == int(deck::Zone::Cut);
		const bool hotTake = hot && v.hoverZone == int(deck::Zone::Take);
		fillRect(px(t.cutBtn.x + 1.f), px(t.cutBtn.y + 1.f), px(t.cutBtn.w - 2.f), px(t.cutBtn.h - 1.f),
			 hotCut ? brighten(kCut, 40) : kCut);
		drawTextIn("CUT", px(t.cutBtn.x), px(t.cutBtn.y), px(t.cutBtn.w), px(t.cutBtn.h), true, 0.78f);
		fillRect(px(t.takeBtn.x + 1.f), px(t.takeBtn.y + 1.f), px(t.takeBtn.w - 2.f), px(t.takeBtn.h - 1.f),
			 hotTake ? brighten(kTake, 40) : kTake);
		drawTextIn("TAKE", px(t.takeBtn.x), px(t.takeBtn.y), px(t.takeBtn.w), px(t.takeBtn.h), true, 0.78f);

		// Bingkai tile: tebal untuk Program/Preview.
		drawOutline(px(t.all.x) - 1.f, px(t.all.y) - 1.f, px(t.all.w) + 2.f, px(t.all.h) + 2.f,
			    hot && !isProg && !isPrev ? 0xFF9A9A9A : accent, isProg || isPrev ? 3 : 1);
	}
}

void releaseGraphicsResources()
{
	obs_enter_graphics();
	if (g_box) {
		gs_vertexbuffer_destroy(g_box);
		g_box = nullptr;
	}
	if (g_quad) {
		gs_vertexbuffer_destroy(g_quad);
		g_quad = nullptr;
	}
	obs_leave_graphics();
	clearLabelCache();
}

void clearLabelCache()
{
	std::lock_guard<std::mutex> lock(g_labelMutex);
	g_labels.clear();
}

} // namespace ls::render
