// Live Studio - jendela Pengaturan bergaya OBS: sidebar kategori di kiri, halaman di kanan,
// tombol OK / Batal / Terapkan. Menggabungkan dialog Output, Audio, Video, dan Plugin.
#pragma once

#include <QDialog>
#include <QList>

class QComboBox;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace ls {

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	enum Page { Output = 0, Audio, Video, Plugins };
	explicit SettingsDialog(QWidget *parent = nullptr, Page start = Output);

	void showPage(Page p);

private:
	void applyAll(); // memanggil tombol OK milik tiap halaman tertanam
	void embed(QDialog *page, const QString &title);

	QListWidget *nav_ = nullptr;
	QStackedWidget *stack_ = nullptr;
	QComboBox *videoPreset_ = nullptr;
	QList<QDialog *> pages_;
	QPushButton *apply_ = nullptr;
};

} // namespace ls
