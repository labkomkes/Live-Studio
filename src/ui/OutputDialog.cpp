#include "OutputDialog.h"

#include "../core/OutputManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ls {

OutputDialog::OutputDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Pengaturan Output (Rekam & Siaran)");
	setMinimumWidth(520);
	const OutputSettings s = OutputSettings::load();
	auto *v = new QVBoxLayout(this);
	auto *form = new QFormLayout();

	auto *dirRow = new QHBoxLayout();
	dir_ = new QLineEdit(s.recordDir, this);
	auto *browse = new QPushButton("Pilih…", this);
	dirRow->addWidget(dir_, 1);
	dirRow->addWidget(browse);
	form->addRow("Folder rekaman (.mkv):", dirRow);
	connect(browse, &QPushButton::clicked, this, [this] {
		const QString d = QFileDialog::getExistingDirectory(this, "Folder rekaman", dir_->text());
		if (!d.isEmpty())
			dir_->setText(d);
	});

	platform_ = new QComboBox(this);
	platform_->addItem("Kustom", "");
	platform_->addItem("YouTube", "rtmp://a.rtmp.youtube.com/live2");
	platform_->addItem("Facebook Live", "rtmps://live-api-s.facebook.com:443/rtmp/");
	platform_->addItem("Twitch", "rtmp://live.twitch.tv/app");
	form->addRow("Platform (isi server otomatis):", platform_);
	server_ = new QLineEdit(s.server, this);
	key_ = new QLineEdit(s.key, this);
	key_->setEchoMode(QLineEdit::Password);
	remember_ = new QCheckBox("Ingat kunci siaran di PC ini (disimpan tanpa enkripsi)", this);
	remember_->setChecked(!s.key.isEmpty());
	form->addRow("Server:", server_);
	form->addRow("Kunci siaran:", key_);
	form->addRow("", remember_);
	connect(platform_, &QComboBox::activated, this, [this](int i) {
		const QString u = platform_->itemData(i).toString();
		if (!u.isEmpty())
			server_->setText(u);
	});

	vkbps_ = new QSpinBox(this);
	vkbps_->setRange(500, 20000);
	vkbps_->setSingleStep(500);
	vkbps_->setSuffix(" kbps");
	vkbps_->setValue(s.videoKbps);
	akbps_ = new QSpinBox(this);
	akbps_->setRange(64, 320);
	akbps_->setSingleStep(32);
	akbps_->setSuffix(" kbps");
	akbps_->setValue(s.audioKbps);
	preset_ = new QComboBox(this);
	preset_->addItems({"ultrafast", "superfast", "veryfast", "faster", "fast", "medium"});
	preset_->setCurrentText(s.preset);
	form->addRow("Bitrate video:", vkbps_);
	form->addRow("Bitrate audio:", akbps_);
	form->addRow("Preset x264 (kiri = ringan CPU):", preset_);
	v->addLayout(form);

	auto *note = new QLabel("Encoder: x264 (CPU) + AAC. Rekaman .mkv tetap bisa dibuka bila aplikasi tertutup mendadak.", this);
	note->setWordWrap(true);
	note->setStyleSheet("color:#8a8a8a;");
	v->addWidget(note);

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	v->addWidget(bb);
	connect(bb, &QDialogButtonBox::accepted, this, [this] {
		save();
		accept();
	});
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void OutputDialog::save()
{
	OutputSettings s;
	s.recordDir = dir_->text().trimmed();
	s.server = server_->text().trimmed();
	s.key = key_->text().trimmed();
	s.videoKbps = vkbps_->value();
	s.audioKbps = akbps_->value();
	s.preset = preset_->currentText();
	s.save(remember_->isChecked());
}

} // namespace ls
