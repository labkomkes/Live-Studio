#include "PreviewEditor.h"

#include "../core/SceneManager.h"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>

#include <obs.h>

namespace ls {

static void canvasDims(float &w, float &h)
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	w = float(ovi.base_width);
	h = float(ovi.base_height);
}

PreviewEditor::PreviewEditor(SceneManager *sm, QWidget *parent) : ObsDisplayWidget(parent), sm_(sm)
{
	setMouseTracking(true);
	setFocusPolicy(Qt::ClickFocus);
	setToolTip("Klik sumber untuk memilih · seret = pindah · tarik pegangan = ubah ukuran (sudut menjaga rasio, "
		   "Shift = bebas) · panah = geser 1 px (Shift 10 px) · klik dua kali = Transform · klik kanan = menu");
	setDrawFn([this](uint32_t cx, uint32_t cy) {
		render::EditorView v;
		{
			std::lock_guard<std::mutex> lock(viewMutex_);
			v = view_;
		}
		render::drawPreviewEditor(sm_, cx, cy, v);
	});
	timer_ = new QTimer(this);
	timer_->setInterval(40);
	connect(timer_, &QTimer::timeout, this, [this] { refreshView(); });
	timer_->start();
}

QPointF PreviewEditor::toCanvas(const QPointF &p) const
{
	float bw, bh;
	canvasDims(bw, bh);
	const float dpr = float(devicePixelRatioF());
	const float W = width() * dpr, H = height() * dpr;
	const float scale = std::min(W / bw, H / bh);
	const float ox = (W - bw * scale) / 2.f, oy = (H - bh * scale) / 2.f;
	return QPointF((p.x() * dpr - ox) / scale, (p.y() * dpr - oy) / scale);
}

QPointF PreviewEditor::toWidget(const QPointF &c) const
{
	float bw, bh;
	canvasDims(bw, bh);
	const float dpr = float(devicePixelRatioF());
	const float W = width() * dpr, H = height() * dpr;
	const float scale = std::min(W / bw, H / bh);
	const float ox = (W - bw * scale) / 2.f, oy = (H - bh * scale) / 2.f;
	return QPointF((ox + c.x() * scale) / dpr, (oy + c.y() * scale) / dpr);
}

void PreviewEditor::publish(const render::EditorView &v)
{
	std::lock_guard<std::mutex> lock(viewMutex_);
	v.hasSel ? (void)0 : (void)0;
	view_ = v;
}

void PreviewEditor::refreshView()
{
	render::EditorView v;
	{
		std::lock_guard<std::mutex> lock(viewMutex_);
		v = view_; // pertahankan hoverHandle & garis bantu
	}
	SceneManager::ItemRect r;
	const QString sel = sm_->selectedItem();
	v.hasSel = !sel.isEmpty() && sm_->itemRect(sm_->previewName(), sel, &r);
	if (v.hasSel) {
		v.x = r.x;
		v.y = r.y;
		v.w = r.w;
		v.h = r.h;
	}
	if (mode_ != Mode::Move)
		v.snapX = v.snapY = false;
	publish(v);
}

int PreviewEditor::handleAt(const QPointF &c) const
{
	const QString sel = sm_->selectedItem();
	SceneManager::ItemRect r;
	if (sel.isEmpty() || !sm_->itemRect(sm_->previewName(), sel, &r))
		return -1;
	float bw, bh;
	canvasDims(bw, bh);
	const float scale = std::min(float(width()) * devicePixelRatioF() / bw, float(height()) * devicePixelRatioF() / bh);
	const float tol = 9.f / scale; // piksel kanvas yang setara 9 px layar
	const float hx[8] = {r.x, r.x + r.w / 2, r.x + r.w, r.x + r.w, r.x + r.w, r.x + r.w / 2, r.x, r.x};
	const float hy[8] = {r.y, r.y, r.y, r.y + r.h / 2, r.y + r.h, r.y + r.h, r.y + r.h, r.y + r.h / 2};
	for (int i = 0; i < 8; ++i)
		if (std::fabs(c.x() - hx[i]) <= tol && std::fabs(c.y() - hy[i]) <= tol)
			return i;
	return -1;
}

void PreviewEditor::mousePressEvent(QMouseEvent *e)
{
	setFocus();
	const QPointF c = toCanvas(e->position());
	const QString scene = sm_->previewName();
	if (e->button() == Qt::RightButton) {
		const QString n = sm_->itemAt(scene, float(c.x()), float(c.y()));
		if (!n.isEmpty()) {
			sm_->selectItem(n);
			emit itemMenuRequested(n, e->globalPosition().toPoint());
		}
		return;
	}
	if (e->button() != Qt::LeftButton)
		return;

	const int h = handleAt(c);
	if (h >= 0) {
		SceneManager::ItemRect r;
		sm_->itemRect(scene, sm_->selectedItem(), &r);
		mode_ = Mode::Resize;
		handle_ = h;
		dragName_ = sm_->selectedItem();
		sx_ = r.x;
		sy_ = r.y;
		sw_ = r.w;
		sh_ = r.h;
		pressPt_ = c;
		return;
	}
	const QString n = sm_->itemAt(scene, float(c.x()), float(c.y()));
	sm_->selectItem(n);
	if (n.isEmpty()) {
		mode_ = Mode::None;
		return;
	}
	SceneManager::ItemRect r;
	sm_->itemRect(scene, n, &r);
	mode_ = Mode::Move;
	dragName_ = n;
	sx_ = r.x;
	sy_ = r.y;
	sw_ = r.w;
	sh_ = r.h;
	pressPt_ = c;
}

void PreviewEditor::applyResize(const QPointF &pt, bool free)
{
	const float dx = float(pt.x() - pressPt_.x()), dy = float(pt.y() - pressPt_.y());
	float l = sx_, t = sy_, r = sx_ + sw_, b = sy_ + sh_;
	const int h = handle_;
	const bool corner = (h == 0 || h == 2 || h == 4 || h == 6);
	if (h == 0 || h == 6 || h == 7)
		l += dx;
	if (h == 2 || h == 3 || h == 4)
		r += dx;
	if (h == 0 || h == 1 || h == 2)
		t += dy;
	if (h == 4 || h == 5 || h == 6)
		b += dy;
	const float minS = 16.f;
	if (r - l < minS)
		(h == 0 || h == 6 || h == 7) ? l = r - minS : r = l + minS;
	if (b - t < minS)
		(h == 0 || h == 1 || h == 2) ? t = b - minS : b = t + minS;

	if (corner && !free && sw_ > 0 && sh_ > 0) {
		// jaga rasio awal: lebar menjadi penentu, jangkar = sudut berlawanan
		const float ratio = sw_ / sh_;
		float nw = r - l;
		float nh = nw / ratio;
		if (h == 0 || h == 6)
			l = r - nw;
		else
			r = l + nw;
		if (h == 0 || h == 2)
			t = b - nh;
		else
			b = t + nh;
	}
	SceneManager::ItemRect nr{l, t, r - l, b - t};
	sm_->setItemRect(sm_->previewName(), dragName_, nr);
}

void PreviewEditor::mouseMoveEvent(QMouseEvent *e)
{
	const QPointF c = toCanvas(e->position());
	const QString scene = sm_->previewName();

	if (mode_ == Mode::Move) {
		float dx = float(c.x() - pressPt_.x()), dy = float(c.y() - pressPt_.y());
		render::EditorView v;
		{
			std::lock_guard<std::mutex> lock(viewMutex_);
			v = view_;
		}
		v.snapX = v.snapY = false;
		if (snap_ && !(e->modifiers() & Qt::ControlModifier)) {
			float bw, bh;
			canvasDims(bw, bh);
			const float scale = std::min(float(width()) * devicePixelRatioF() / bw,
						     float(height()) * devicePixelRatioF() / bh);
			const float tol = 8.f / scale;
			const float nx = sx_ + dx, ny = sy_ + dy;
			const float xs[3] = {nx, nx + sw_ / 2, nx + sw_};
			const float tx[3] = {0.f, bw / 2, bw};
			for (int i = 0; i < 3 && !v.snapX; ++i)
				for (int j = 0; j < 3; ++j)
					if (std::fabs(xs[i] - tx[j]) < tol) {
						dx += tx[j] - xs[i];
						v.snapX = true;
						v.snapXPos = tx[j];
						break;
					}
			const float ys[3] = {ny, ny + sh_ / 2, ny + sh_};
			const float ty[3] = {0.f, bh / 2, bh};
			for (int i = 0; i < 3 && !v.snapY; ++i)
				for (int j = 0; j < 3; ++j)
					if (std::fabs(ys[i] - ty[j]) < tol) {
						dy += ty[j] - ys[i];
						v.snapY = true;
						v.snapYPos = ty[j];
						break;
					}
		}
		SceneManager::ItemRect cur;
		if (sm_->itemRect(scene, dragName_, &cur))
			sm_->moveItemBy(scene, dragName_, (sx_ + dx) - cur.x, (sy_ + dy) - cur.y);
		publish(v);
		return;
	}
	if (mode_ == Mode::Resize) {
		applyResize(c, e->modifiers() & Qt::ShiftModifier);
		return;
	}
	// hover: ubah kursor & sorot pegangan
	const int h = handleAt(c);
	{
		std::lock_guard<std::mutex> lock(viewMutex_);
		view_.hoverHandle = h;
	}
	static const Qt::CursorShape shapes[8] = {Qt::SizeFDiagCursor, Qt::SizeVerCursor, Qt::SizeBDiagCursor,
						  Qt::SizeHorCursor,   Qt::SizeFDiagCursor, Qt::SizeVerCursor,
						  Qt::SizeBDiagCursor, Qt::SizeHorCursor};
	if (h >= 0)
		setCursor(shapes[h]);
	else if (!sm_->itemAt(scene, float(c.x()), float(c.y())).isEmpty())
		setCursor(Qt::SizeAllCursor);
	else
		unsetCursor();
}

void PreviewEditor::mouseReleaseEvent(QMouseEvent *)
{
	mode_ = Mode::None;
	handle_ = -1;
	dragName_.clear();
}

void PreviewEditor::mouseDoubleClickEvent(QMouseEvent *e)
{
	const QPointF c = toCanvas(e->position());
	const QString n = sm_->itemAt(sm_->previewName(), float(c.x()), float(c.y()));
	if (!n.isEmpty())
		emit itemDoubleClicked(n);
}

void PreviewEditor::leaveEvent(QEvent *)
{
	std::lock_guard<std::mutex> lock(viewMutex_);
	view_.hoverHandle = -1;
}

void PreviewEditor::keyPressEvent(QKeyEvent *e)
{
	const QString sel = sm_->selectedItem();
	if (sel.isEmpty()) {
		ObsDisplayWidget::keyPressEvent(e);
		return;
	}
	const float step = (e->modifiers() & Qt::ShiftModifier) ? 10.f : 1.f;
	float dx = 0, dy = 0;
	switch (e->key()) {
	case Qt::Key_Left: dx = -step; break;
	case Qt::Key_Right: dx = step; break;
	case Qt::Key_Up: dy = -step; break;
	case Qt::Key_Down: dy = step; break;
	case Qt::Key_Delete:
	case Qt::Key_Backspace:
		emit deleteRequested(sel);
		return;
	case Qt::Key_Escape:
		sm_->selectItem(QString());
		return;
	default:
		ObsDisplayWidget::keyPressEvent(e);
		return;
	}
	sm_->moveItemBy(sm_->previewName(), sel, dx, dy);
}

} // namespace ls
