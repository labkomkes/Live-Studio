// Live Studio - widget Qt yang menampung obs_display_t (render Direct3D11 ke HWND).
#pragma once

#include <QPaintEngine>
#include <QWidget>

#include <functional>
#include <mutex>

#include <obs.h>

namespace ls {

class ObsDisplayWidget : public QWidget {
	Q_OBJECT
public:
	using DrawFn = std::function<void(uint32_t cx, uint32_t cy)>;

	explicit ObsDisplayWidget(QWidget *parent = nullptr);
	~ObsDisplayWidget() override;

	void setDrawFn(DrawFn fn);
	// Panggil sebelum widget dihancurkan / sebelum engine dimatikan.
	void destroyDisplay();

	QPaintEngine *paintEngine() const override { return nullptr; }
	QSize sizeHint() const override { return QSize(640, 360); }

protected:
	void showEvent(QShowEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void paintEvent(QPaintEvent *) override {}

private:
	void createDisplay();
	uint32_t pxW() const;
	uint32_t pxH() const;
	static void drawCallback(void *param, uint32_t cx, uint32_t cy);

	obs_display_t *display_ = nullptr;
	DrawFn fn_;
	std::mutex fnMutex_;
};

} // namespace ls
