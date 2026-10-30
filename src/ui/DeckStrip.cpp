#include "DeckStrip.h"

#include "../core/SceneManager.h"
#include "DeckLayout.h"
#include "Render.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>

namespace ls {

DeckStrip::DeckStrip(SceneManager *sm, QWidget *parent) : ObsDisplayWidget(parent), sm_(sm)
{
	setMouseTracking(true);
	setMinimumSize(320, 150);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	setToolTip("Klik tile = Preview · CUT = ke Program seketika · TAKE = ke Program dengan transisi\n"
		   "Klik dua kali = CUT · Roda mouse = geser daftar");
	dpr_.store(float(devicePixelRatioF()));

	setDrawFn([this](uint32_t cx, uint32_t cy) {
		render::DeckView v;
		v.offset = offset_.load();
		v.hoverSlot = hoverSlot_.load();
		v.hoverZone = hoverZone_.load();
		v.dpr = dpr_.load();
		render::drawDeck(sm_, cx, cy, v);
	});
}

void DeckStrip::showEvent(QShowEvent *e)
{
	dpr_.store(float(devicePixelRatioF()));
	ObsDisplayWidget::showEvent(e);
}

void DeckStrip::resizeEvent(QResizeEvent *e)
{
	dpr_.store(float(devicePixelRatioF()));
	ObsDisplayWidget::resizeEvent(e);
}

DeckStrip::Target DeckStrip::targetAt(const QPoint &p) const
{
	const QStringList names = sm_->sceneNames();
	const deck::Hit h = deck::hitTest(float(width()), float(height()), int(names.size()), offset_.load(),
					  float(p.x()), float(p.y()));
	Target t;
	if (h.sceneIndex >= 0 && h.sceneIndex < names.size()) {
		t.scene = names.at(h.sceneIndex);
		t.zone = int(h.zone);
	}
	return t;
}

void DeckStrip::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton)
		return;
	const Target t = targetAt(e->position().toPoint());
	if (t.scene.isEmpty())
		return;
	switch (deck::Zone(t.zone)) {
	case deck::Zone::Cut:
		emit cutRequested(t.scene);
		break;
	case deck::Zone::Take:
		emit takeRequested(t.scene);
		break;
	default:
		emit previewRequested(t.scene);
		break;
	}
}

void DeckStrip::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton)
		return;
	const Target t = targetAt(e->position().toPoint());
	// Dua klik cepat pada tombol CUT/TAKE dihitung sebagai satu aksi (sudah ditangani oleh klik pertama).
	if (!t.scene.isEmpty() && (deck::Zone(t.zone) == deck::Zone::Thumb || deck::Zone(t.zone) == deck::Zone::Name))
		emit cutRequested(t.scene);
}

void DeckStrip::mouseMoveEvent(QMouseEvent *e)
{
	const QPoint p = e->position().toPoint();
	const QStringList names = sm_->sceneNames();
	const deck::Hit h = deck::hitTest(float(width()), float(height()), int(names.size()), offset_.load(),
					  float(p.x()), float(p.y()));
	hoverSlot_.store(h.slot);
	hoverZone_.store(int(h.zone));
	setCursor(h.slot >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void DeckStrip::leaveEvent(QEvent *e)
{
	hoverSlot_.store(-1);
	hoverZone_.store(0);
	ObsDisplayWidget::leaveEvent(e);
}

void DeckStrip::wheelEvent(QWheelEvent *e)
{
	const int n = int(sm_->sceneNames().size());
	const deck::Metrics m = deck::metrics(float(width()), float(height()), n, offset_.load());
	const int dy = e->angleDelta().y();
	int off = m.offset;
	if (dy < 0)
		++off;
	else if (dy > 0)
		--off;
	offset_.store(deck::metrics(float(width()), float(height()), n, off).offset);
	e->accept();
}

} // namespace ls
