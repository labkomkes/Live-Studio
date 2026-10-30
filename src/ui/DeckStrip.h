// Live Studio - deck ala vMix: strip tile scene dengan thumbnail live dan tombol CUT/TAKE per tile.
#pragma once

#include "ObsDisplayWidget.h"

#include <atomic>

namespace ls {

class SceneManager;

class DeckStrip : public ObsDisplayWidget {
	Q_OBJECT
public:
	explicit DeckStrip(SceneManager *sm, QWidget *parent = nullptr);
	QSize sizeHint() const override { return QSize(640, 188); }

signals:
	void previewRequested(const QString &scene); // klik tile
	void cutRequested(const QString &scene);     // tombol CUT tile / klik dua kali
	void takeRequested(const QString &scene);    // tombol TAKE tile

protected:
	void showEvent(QShowEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void mousePressEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void leaveEvent(QEvent *e) override;
	void wheelEvent(QWheelEvent *e) override;

private:
	struct Target {
		QString scene;
		int zone = 0;
	};
	Target targetAt(const QPoint &p) const;

	SceneManager *sm_;
	std::atomic<int> offset_{0};
	std::atomic<int> hoverSlot_{-1};
	std::atomic<int> hoverZone_{0};
	std::atomic<float> dpr_{1.f};
};

} // namespace ls
