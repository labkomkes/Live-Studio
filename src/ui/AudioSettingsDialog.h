// Live Studio - pengaturan audio: perangkat monitoring (speaker/headphone), mikrofon, audio desktop.
#pragma once

#include <QDialog>

class QComboBox;
class QLabel;

namespace ls {

class AudioSettingsDialog : public QDialog {
	Q_OBJECT
public:
	explicit AudioSettingsDialog(QWidget *parent = nullptr);

private:
	void apply();

	QComboBox *monitor_;
	QComboBox *mic_;
	QComboBox *desktop_;
	QLabel *status_;
};

} // namespace ls
