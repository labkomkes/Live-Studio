// Live Studio - pengaturan rekam & siaran (RTMP).
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

namespace ls {

class OutputDialog : public QDialog {
	Q_OBJECT
public:
	explicit OutputDialog(QWidget *parent = nullptr);

private:
	void save();
	QLineEdit *dir_, *server_, *key_;
	QSpinBox *vkbps_, *akbps_;
	QComboBox *preset_, *platform_;
	QCheckBox *remember_;
};

} // namespace ls
