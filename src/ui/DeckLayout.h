// Live Studio - geometri deck (strip tile scene ala vMix).
// Dipakai bersama oleh penggambar (graphics thread) dan penangan mouse (UI thread) agar klik
// selalu cocok dengan apa yang tampak. Murni hitungan, tanpa Qt/libobs. Satuan: piksel logis.
#pragma once

#include <algorithm>

namespace ls::deck {

struct Rect {
	float x = 0, y = 0, w = 0, h = 0;
	bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Tile {
	Rect all, thumb, nameBar, cutBtn, takeBtn;
};

constexpr float kMargin = 8.f;
constexpr float kGap = 8.f;
constexpr float kNameH = 22.f;
constexpr float kBtnH = 28.f;
constexpr float kMaxTileW = 200.f;
constexpr float kMinTileW = 96.f;

struct Metrics {
	float tileW = kMinTileW;
	int fit = 1;     // jumlah tile yang muat pada lebar ini
	int offset = 0;  // indeks scene pertama yang terlihat (sudah dibatasi)
	int visible = 0; // jumlah tile yang digambar
};

// n = jumlah scene, offsetWanted = geser yang diminta (roda mouse).
inline Metrics metrics(float W, float H, int n, int offsetWanted)
{
	Metrics m;
	const float availThumbH = H - 2.f * kMargin - kNameH - kBtnH;
	m.tileW = std::min(kMaxTileW, std::max(kMinTileW, availThumbH * 16.f / 9.f));
	m.fit = std::max(1, int((W - 2.f * kMargin + kGap) / (m.tileW + kGap)));
	const int maxOffset = std::max(0, n - m.fit);
	m.offset = std::min(std::max(0, offsetWanted), maxOffset);
	m.visible = std::max(0, std::min(n - m.offset, m.fit));
	return m;
}

inline Tile tile(const Metrics &m, int slot)
{
	Tile t;
	const float x = kMargin + slot * (m.tileW + kGap);
	const float thumbH = m.tileW * 9.f / 16.f;
	const float y = kMargin;
	t.thumb = {x, y, m.tileW, thumbH};
	t.nameBar = {x, y + thumbH, m.tileW, kNameH};
	const float by = y + thumbH + kNameH;
	t.cutBtn = {x, by, m.tileW / 2.f, kBtnH};
	t.takeBtn = {x + m.tileW / 2.f, by, m.tileW / 2.f, kBtnH};
	t.all = {x, y, m.tileW, thumbH + kNameH + kBtnH};
	return t;
}

enum class Zone { None = 0, Thumb = 1, Name = 2, Cut = 3, Take = 4 };

struct Hit {
	int slot = -1;
	Zone zone = Zone::None;
	int sceneIndex = -1; // offset + slot
};

inline Hit hitTest(float W, float H, int n, int offsetWanted, float px, float py)
{
	Hit h;
	const Metrics m = metrics(W, H, n, offsetWanted);
	for (int s = 0; s < m.visible; ++s) {
		const Tile t = tile(m, s);
		if (!t.all.contains(px, py))
			continue;
		h.slot = s;
		h.sceneIndex = m.offset + s;
		if (t.thumb.contains(px, py))
			h.zone = Zone::Thumb;
		else if (t.nameBar.contains(px, py))
			h.zone = Zone::Name;
		else if (t.cutBtn.contains(px, py))
			h.zone = Zone::Cut;
		else
			h.zone = Zone::Take;
		return h;
	}
	return h;
}

} // namespace ls::deck
