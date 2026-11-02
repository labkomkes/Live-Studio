#include "SettingsDialog.h"

#include "AudioSettingsDialog.h"
#include "OutputDialog.h"
#include "PluginsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace ls {

SettingsDialog::SettingsDialog(QWidget *parent, Page start) : QDialog(parent)
{
	setObjectName("settingsDialog");
	setWindowTitle("Pengaturan");
	resize(980, 640);

	auto *root = new QVBoxLayout(this);
	auto *row = new QHBoxLayout;
	root->addLayout(row, 1);

	nav_ = new QListWidget(this);
	nav_->setObjectName("settingsNav");
	nav_->setFixedWidth(190);
	stack_ = new QStackedWidget(this);
	row->addWidget(nav_);
	row->addWidget(stack_, 1);

	// urutan harus sama dengan enum Page
	embed(new OutputDialog(this), "⏺  Output");
	embed(new AudioSettingsDialog(this), "♪  Audio");

	// Halaman Video (sederhana; berlaku setelah aplikasi dibuka ulang)
	auto *vp = new QWidget(this);
	auto *vl = new QVBoxLayout(vp);
	vl->addWidget(new QLabel("<b>Video</b>", vp));
	videoPreset_ = new QComboBox(vp);
	videoPreset_->setObjectName("videoPreset");
	videoPreset_->addItems({"1920x1080 @ 30", "1920x1080 @ 60", "1280x720 @ 30", "1280x720 @ 60"});
	{
		QSettings s("LiveStudio", "LiveStudio");
		const int i = videoPreset_->findText(s.value("video/preset").toString());
		if (i >= 0)
			videoPreset_->setCurrentIndex(i);
	}
	vl->addWidget(new QLabel("Kanvas dan frame rate:", vp));
	vl->addWidget(videoPreset_);
	auto *note = new QLabel("Berlaku setelah Live Studio ditutup dan dibuka ulang.", vp);
	note->setObjectName("settingsNote");
	vl->addWidget(note);
	vl->addStretch(1);
	stack_->addWidget(vp);
	nav_->addItem("▭  Video");

	embed(new PluginsDialog(this), "✚  Plugin");

	auto *bb = new QDialogButtonBox(this);
	auto *ok = bb->addButton("OK", QDialogButtonBox::AcceptRole);
	ok->setObjectName("settingsOk");
	bb->addButton("Batal", QDialogButtonBox::RejectRole);
	apply_ = bb->addButton("Terapkan", QDialogButtonBox::ApplyRole);
	apply_->setObjectName("settingsApply");
	root->addWidget(bb);
	connect(bb, &QDialogButtonBox::accepted, this, [this] {
		applyAll();
		accept();
	});
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(apply_, &QPushButton::clicked, this, [this] { applyAll(); });

	connect(nav_, &QListWidget::currentRowChanged, stack_, &QStackedWidget::setCurrentIndex);
	showPage(start);
}

void SettingsDialog::embed(QDialog *page, const QString &title)
{
	page->setWindowFlags(Qt::Widget);
	// tombol OK/Batal milik halaman disembunyikan; diganti tombol jendela Pengaturan
	for (auto *b : page->findChildren<QDialogButtonBox *>())
		b->hide();
	pages_ << page;
	stack_->addWidget(page);
	nav_->addItem(title);
}

void SettingsDialog::applyAll()
{
	for (QDialog *p : pages_) {
		for (auto *b : p->findChildren<QDialogButtonBox *>())
			if (auto *okb = b->button(QDialogButtonBox::Ok))
				okb->click(); // menyimpan halaman (dan menyembunyikannya)
		p->show();
	}
	QSettings s("LiveStudio", "LiveStudio");
	s.setValue("video/preset", videoPreset_->currentText());
}

void SettingsDialog::showPage(Page p)
{
	nav_->setCurrentRow(int(p));
}

} // namespace ls
