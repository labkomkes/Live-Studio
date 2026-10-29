// Live Studio - panel kontrol lower third untuk operator.
#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;

namespace ls {

class LowerThird;

class LowerThirdPanel : public QWidget {
	Q_OBJECT
public:
	explicit LowerThirdPanel(LowerThird *lt, QWidget *parent = nullptr);

private:
	void loadPresets();
	void savePresets() const;
	void applyFields();
	void addPreset();
	void toggle();

	LowerThird *lt_;
	QLineEdit *name_;
	QLineEdit *title_;
	QSpinBox *autoHide_;
	QPushButton *toggleBtn_;
	QListWidget *presets_;
};

} // namespace ls
