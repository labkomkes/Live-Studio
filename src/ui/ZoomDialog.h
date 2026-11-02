// Live Studio - dialog "Scene Zoom": membuat satu scene siap siaran yang menangkap jendela rapat Zoom
// + audio Zoom (per aplikasi) + mikrofon opsional.
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;

namespace ls {

class ZoomDialog : public QDialog {
	Q_OBJECT
public:
	explicit ZoomDialog(QWidget *parent = nullptr);

	QString sceneName() const;
	QString windowValue() const; // "" = otomatis
	bool captureAudio() const;
	bool addMic() const;

private:
	void refreshWindows();
	void openZoom();

	QLineEdit *link_ = nullptr;
	QLineEdit *name_ = nullptr;
	QComboBox *win_ = nullptr;
	QCheckBox *audio_ = nullptr;
	QCheckBox *mic_ = nullptr;
};

} // namespace ls
