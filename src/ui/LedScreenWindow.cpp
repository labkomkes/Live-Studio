#include "LedScreenWindow.h"

#include "../core/LedScreen.h"
#include "../core/SceneManager.h"
#include "ObsDisplayWidget.h"
#include "Render.h"

#include <QCloseEvent>
#include <QKeyEvent>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

namespace ls {

LedScreenWindow::LedScreenWindow(SceneManager *sm, LedScreen *led, QWidget *parent)
	: QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint), sm_(sm), led_(led)
{
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle("Layar LED - Live Studio");
	auto *lay = new QVBoxLayout(this);
	lay->setContentsMargins(0, 0, 0, 0);
	display_ = new ObsDisplayWidget(this);
	display_->setMinimumSize(16, 16);
	lay->addWidget(display_);
	display_->setDrawFn([this](uint32_t cx, uint32_t cy) { render::drawLed(sm_, led_->layout(), cx, cy); });
}

LedScreenWindow::~LedScreenWindow()
{
	if (display_)
		display_->destroyDisplay();
}

void LedScreenWindow::presentOn(QScreen *screen)
{
	const LedLayout L = led_->layout();
	if (!screen) {
		resize(std::min(L.width, 1280), std::min(L.height, 720));
		show();
		return;
	}
	const double dpr = screen->devicePixelRatio();
	const QRect g = screen->geometry();
	const int lw = int(std::lround(L.width / dpr)), lh = int(std::lround(L.height / dpr));
	show(); // buat window handle dulu
	if (QWindow *wh = windowHandle())
		wh->setScreen(screen);
	if (lw == g.width() && lh == g.height()) {
		setGeometry(g);
		showFullScreen();
	} else {
		// Ukuran LED berbeda dari monitor: jendela tanpa bingkai persis sebesar layar LED di sudut kiri-atas monitor.
		setGeometry(QRect(g.topLeft(), QSize(lw, lh)));
		show();
	}
}

void LedScreenWindow::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Escape) {
		close();
		return;
	}
	QWidget::keyPressEvent(e);
}

void LedScreenWindow::closeEvent(QCloseEvent *e)
{
	if (display_)
		display_->destroyDisplay();
	e->accept();
}

} // namespace ls
