// Live Studio - dialog Transform sumber: posisi, ukuran, rotasi, crop (angka tepat), ala OBS.
#pragma once

#include "../core/SceneManager.h"

#include <QDialog>

class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;

namespace ls {

class TransformDialog : public QDialog {
	Q_OBJECT
public:
	TransformDialog(SceneManager *sm, const QString &scene, const QString &item, QWidget *parent = nullptr);

private:
	void apply();
	SceneManager *sm_;
	QString scene_, item_;
	SceneManager::ItemTransform orig_;
	QDoubleSpinBox *x_, *y_, *w_, *h_, *rot_;
	QSpinBox *cl_, *ct_, *cr_, *cb_;
	QCheckBox *aspect_;
	bool loading_ = true;
	float ratio_ = 1.f;
};

} // namespace ls
