// Live Studio - jendela proyektor (Program / Preview / Multiview) untuk monitor terpilih.
#pragma once

#include <QWidget>

class QScreen;

namespace ls {

class SceneManager;
class ObsDisplayWidget;

class ProjectorWindow : public QWidget {
	Q_OBJECT
public:
	enum class Mode { Program, Preview, Multiview };

	ProjectorWindow(SceneManager *sm, Mode mode, QWidget *parent = nullptr);
	~ProjectorWindow() override;

	// screen == nullptr -> jendela biasa (tidak fullscreen).
	void presentOn(QScreen *screen);
	Mode mode() const { return mode_; }

protected:
	void keyPressEvent(QKeyEvent *e) override;
	void closeEvent(QCloseEvent *e) override;

private:
	SceneManager *sm_;
	Mode mode_;
	ObsDisplayWidget *display_;
};

} // namespace ls
