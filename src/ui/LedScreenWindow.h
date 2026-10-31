// Live Studio - jendela keluaran layar LED (ukuran piksel bebas) pada monitor terpilih.
#pragma once

#include <QPointer>
#include <QWidget>

class QScreen;

namespace ls {

class SceneManager;
class LedScreen;
class ObsDisplayWidget;

class LedScreenWindow : public QWidget {
	Q_OBJECT
public:
	LedScreenWindow(SceneManager *sm, LedScreen *led, QWidget *parent = nullptr);
	~LedScreenWindow() override;
	// screen == nullptr -> jendela biasa. Ukuran jendela = ukuran layar LED (piksel fisik).
	void presentOn(QScreen *screen);

protected:
	void keyPressEvent(QKeyEvent *e) override;
	void closeEvent(QCloseEvent *e) override;

private:
	SceneManager *sm_;
	LedScreen *led_;
	ObsDisplayWidget *display_;
};

} // namespace ls
