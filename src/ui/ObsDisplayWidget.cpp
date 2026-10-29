#include "ObsDisplayWidget.h"

#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>

#include <algorithm>

namespace ls {

ObsDisplayWidget::ObsDisplayWidget(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setMinimumSize(160, 90);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

ObsDisplayWidget::~ObsDisplayWidget()
{
	destroyDisplay();
}

uint32_t ObsDisplayWidget::pxW() const
{
	return uint32_t(std::max(1, int(width() * devicePixelRatioF())));
}

uint32_t ObsDisplayWidget::pxH() const
{
	return uint32_t(std::max(1, int(height() * devicePixelRatioF())));
}

void ObsDisplayWidget::setDrawFn(DrawFn fn)
{
	std::lock_guard<std::mutex> lock(fnMutex_);
	fn_ = std::move(fn);
}

void ObsDisplayWidget::showEvent(QShowEvent *e)
{
	QWidget::showEvent(e);
	// Tunda satu iterasi event loop agar HWND native dan ukuran final sudah ada.
	QTimer::singleShot(0, this, [this] { createDisplay(); });
}

void ObsDisplayWidget::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	if (display_)
		obs_display_resize(display_, pxW(), pxH());
}

void ObsDisplayWidget::createDisplay()
{
	if (display_ || !isVisible())
		return;

	gs_init_data info = {};
	info.cx = pxW();
	info.cy = pxH();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;
#ifdef _WIN32
	info.window.hwnd = reinterpret_cast<void *>(winId());
#else
	// Hanya agar bisa dikompilasi di Linux untuk pemeriksaan; produk yang didukung = Windows.
	info.window.id = uint32_t(winId());
	info.window.display = nullptr;
#endif

	display_ = obs_display_create(&info, 0xFF141414);
	if (!display_)
		return;
	obs_display_add_draw_callback(display_, &ObsDisplayWidget::drawCallback, this);
}

void ObsDisplayWidget::destroyDisplay()
{
	if (!display_)
		return;
	obs_display_remove_draw_callback(display_, &ObsDisplayWidget::drawCallback, this);
	obs_display_destroy(display_);
	display_ = nullptr;
}

void ObsDisplayWidget::drawCallback(void *param, uint32_t cx, uint32_t cy)
{
	auto *self = static_cast<ObsDisplayWidget *>(param);
	DrawFn fn;
	{
		std::lock_guard<std::mutex> lock(self->fnMutex_);
		fn = self->fn_;
	}
	if (fn)
		fn(cx, cy);
}

} // namespace ls
