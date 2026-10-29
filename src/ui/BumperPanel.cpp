#include "BumperPanel.h"

#include "../core/Bumper.h"
#include "../core/SceneManager.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace ls {

BumperPanel::BumperPanel(Bumper *bumper, SceneManager *sm, QWidget *parent)
	: QWidget(parent), bumper_(bumper), sm_(sm)
{
	auto *lay = new QVBoxLayout(this);
	auto *form = new QFormLayout();

	auto *pathRow = new QHBoxLayout();
	path_ = new QLineEdit(this);
	path_->setPlaceholderText("File video bumper (mp4, mov, webm…)");
	auto *browse = new QPushButton("Cari…", this);
	pathRow->addWidget(path_, 1);
	pathRow->addWidget(browse);
	form->addRow("Video", pathRow);

	then_ = new QComboBox(this);
	form->addRow("Setelah selesai", then_);
	lay->addLayout(form);

	auto *btns = new QHBoxLayout();
	play_ = new QPushButton("▶  Putar bumper", this);
	play_->setMinimumHeight(34);
	stop_ = new QPushButton("■  Hentikan", this);
	stop_->setEnabled(false);
	btns->addWidget(play_, 1);
	btns->addWidget(stop_);
	lay->addLayout(btns);
	lay->addStretch(1);

	QSettings s("LiveStudio", "LiveStudio");
	path_->setText(s.value("bumper/path").toString());
	refreshScenes();

	connect(browse, &QPushButton::clicked, this, [this] {
		const QString f = QFileDialog::getOpenFileName(this, "Pilih video bumper", QString(),
							       "Video (*.mp4 *.mov *.mkv *.webm *.avi);;Semua file (*)");
		if (!f.isEmpty()) {
			path_->setText(f);
			QSettings("LiveStudio", "LiveStudio").setValue("bumper/path", f);
		}
	});
	connect(play_, &QPushButton::clicked, this, [this] {
		const QString then = then_->currentData().toString();
		if (!bumper_->play(path_->text().trimmed(), then))
			QMessageBox::warning(this, "Bumper", "Tidak bisa memutar bumper. Periksa path file video.");
	});
	connect(stop_, &QPushButton::clicked, bumper_, &Bumper::stop);
	connect(bumper_, &Bumper::started, this, [this] {
		play_->setEnabled(false);
		stop_->setEnabled(true);
	});
	connect(bumper_, &Bumper::finished, this, [this] {
		play_->setEnabled(true);
		stop_->setEnabled(false);
	});
}

void BumperPanel::refreshScenes()
{
	const QString keep = then_->currentData().toString();
	then_->clear();
	then_->addItem("Kembali ke adegan sebelumnya", QString());
	for (const QString &n : sm_->sceneNames())
		then_->addItem(n, n);
	const int idx = then_->findData(keep);
	then_->setCurrentIndex(idx >= 0 ? idx : 0);
}

} // namespace ls
