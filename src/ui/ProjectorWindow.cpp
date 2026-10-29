#include "ProjectorWindow.h"

#include "../core/SceneManager.h"
#include "ObsDisplayWidget.h"
#include "Render.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QScreen>
#include <QVBoxLayout>
#include <QWindow>

namespace ls {

ProjectorWindow::ProjectorWindow(SceneManager *sm, Mode mode, QWidget *parent)
	: QWidget(parent, Qt::Window), sm_(sm), mode_(mode)
{
	setAttribute(Qt::WA_DeleteOnClose);
	const char *title = mode == Mode::Multiview ? "Live Studio - Multiview"
			    : mode == Mode::Program ? "Live Studio - Program Output"
						    : "Live Studio - Preview Output";
	setWindowTitle(title);
	setStyleSheet("background:#000;");

	auto *lay = new QVBoxLayout(this);
	lay->setContentsMargins(0, 0, 0, 0);
	display_ = new ObsDisplayWidget(this);
	lay->addWidget(display_);

	switch (mode_) {
	case Mode::Program:
		display_->setDrawFn([sm](uint32_t cx, uint32_t cy) { render::drawProgram(sm, cx, cy); });
		break;
	case Mode::Preview:
		display_->setDrawFn([sm](uint32_t cx, uint32_t cy) { render::drawPreview(sm, cx, cy); });
		break;
	case Mode::Multiview:
		display_->setDrawFn([sm](uint32_t cx, uint32_t cy) { render::drawMultiview(sm, cx, cy); });
		break;
	}
	resize(1280, 720);
}

ProjectorWindow::~ProjectorWindow()
{
	display_->destroyDisplay();
}

void ProjectorWindow::presentOn(QScreen *screen)
{
	if (!screen) {
		show();
		return;
	}
	show(); // buat window handle dulu
	if (QWindow *wh = windowHandle())
		wh->setScreen(screen);
	setGeometry(screen->geometry());
	showFullScreen();
}

void ProjectorWindow::keyPressEvent(QKeyEvent *e)
{
	if (e->key() == Qt::Key_Escape) {
		close();
		return;
	}
	QWidget::keyPressEvent(e);
}

void ProjectorWindow::closeEvent(QCloseEvent *e)
{
	display_->destroyDisplay();
	QWidget::closeEvent(e);
}

} // namespace ls
