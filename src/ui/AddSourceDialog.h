// Live Studio - dialog "Tambah Sumber" bergaya OBS: kategori di kiri, kotak (tile) di kanan,
// "Tambah yang sudah ada" untuk memakai ulang sumber yang sama di scene lain.
#pragma once

#include <QDialog>

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

namespace ls {

class SceneManager;

class AddSourceDialog : public QDialog {
	Q_OBJECT
public:
	// Urutan 0..7 sama dengan MainWindow::SourceKind; Zoom = 8.
	enum Kind { Webcam = 0, Screen, Window, Video, Image, Text, Mic, DesktopAudio, Zoom, Count };

	explicit AddSourceDialog(SceneManager *sm, QWidget *parent = nullptr);

	int newKind() const { return newKind_; }                 // -1 bila memilih sumber yang sudah ada
	QString existingName() const { return existing_; }
	static int categoryOf(const QString &typeId, const QString &name);

private:
	void fillTiles();
	void chooseCurrent();

	SceneManager *sm_;
	QListWidget *cats_ = nullptr;
	QListWidget *tiles_ = nullptr;
	QLabel *title_ = nullptr;
	QLabel *hint_ = nullptr;
	QPushButton *addExistingBtn_ = nullptr;
	int newKind_ = -1;
	QString existing_;
};

} // namespace ls
