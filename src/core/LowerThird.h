// Live Studio - lower third (nama + jabatan) sebagai overlay di semua scene.
#pragma once

#include "SceneManager.h"

#include <QColor>
#include <QObject>
#include <QString>
#include <QTimer>

namespace ls {

class LowerThird : public QObject {
	Q_OBJECT
public:
	explicit LowerThird(SceneManager *sm, QObject *parent = nullptr);

	// Membuat/menemukan item bar + 2 teks di scene overlay. Aman dipanggil berulang.
	bool ensureItems();

	void setContent(const QString &name, const QString &title);
	void setBarColor(const QColor &c);
	void setTextColor(const QColor &c);

	void show();
	void showFor(int seconds); // tampil lalu sembunyi otomatis
	void hide();
	bool isShown() const { return shown_; }

signals:
	void visibilityChanged(bool shown);

private:
	void setVisible(bool v);
	void applyFade(obs_sceneitem_t *item);
	obs_sceneitem_t *find(const char *name) const;

	SceneManager *sm_;
	QTimer *autoHide_;
	bool shown_ = false;
	QColor barColor_{20, 90, 180};
	QColor textColor_{255, 255, 255};
	QString name_{"Nama Pembicara"};
	QString title_{"Jabatan / Institusi"};
};

} // namespace ls
