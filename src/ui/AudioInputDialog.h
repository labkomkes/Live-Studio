// Live Studio - tambah input audio dengan memilih driver (WASAPI / ASIO / DirectShow / aplikasi) lalu perangkat.
#pragma once

#include "../core/SourceFactory.h"

#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;

namespace ls {

class AudioInputDialog : public QDialog {
	Q_OBJECT
public:
	explicit AudioInputDialog(QWidget *parent = nullptr);

	factory::AudioDriver driver() const;
	QString deviceValue() const;
	QString deviceLabel() const;
	QString sourceName() const;

private:
	void refreshDevices();

	QList<factory::AudioDriver> drivers_;
	QComboBox *driver_;
	QComboBox *device_;
	QLineEdit *name_;
	QLabel *note_;
};

} // namespace ls
