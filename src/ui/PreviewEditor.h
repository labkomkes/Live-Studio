// Live Studio - Preview yang bisa diedit langsung (seret = pindah, pegangan = ubah ukuran), ala OBS.
#pragma once

#include "ObsDisplayWidget.h"
#include "Render.h"

#include <QPointF>
#include <QString>

#include <mutex>

class QTimer;

namespace ls {

class SceneManager;

class PreviewEditor : public ObsDisplayWidget {
	Q_OBJECT
public:
	explicit PreviewEditor(SceneManager *sm, QWidget *parent = nullptr);

	// Konversi koordinat widget <-> kanvas (untuk uji).
	QPointF toCanvas(const QPointF &widgetPt) const;
	QPointF toWidget(const QPointF &canvasPt) const;
	void setSnapEnabled(bool on) { snap_ = on; }
	bool snapEnabled() const { return snap_; }

signals:
	void itemMenuRequested(const QString &name, const QPoint &globalPos);
	void itemDoubleClicked(const QString &name);
	void deleteRequested(const QString &name);

protected:
	void mousePressEvent(QMouseEvent *e) override;
	void mouseMoveEvent(QMouseEvent *e) override;
	void mouseReleaseEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override;
	void leaveEvent(QEvent *e) override;

private:
	enum class Mode { None, Move, Resize };
	int handleAt(const QPointF &canvasPt) const;
	void refreshView();
	void publish(const render::EditorView &v);
	void applyResize(const QPointF &pt, bool free);

	SceneManager *sm_;
	QTimer *timer_;
	render::EditorView view_;
	std::mutex viewMutex_;
	Mode mode_ = Mode::None;
	int handle_ = -1;
	QPointF pressPt_;
	float sx_ = 0, sy_ = 0, sw_ = 0, sh_ = 0; // kotak awal saat tekan
	QString dragName_;
	bool snap_ = true;
};

} // namespace ls
