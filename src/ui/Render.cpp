#include "Render.h"

#include "../core/ObsEngine.h"
#include "../core/Raii.h"
#include "../core/SceneManager.h"
#include "../core/SourceFactory.h"

#include <obs.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <string>

namespace ls::render {

static gs_vertbuffer_t *g_box = nullptr;
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

void releaseGraphicsResources()
{
	obs_enter_graphics();
	if (g_box) {
		gs_vertexbuffer_destroy(g_box);
		g_box = nullptr;
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
