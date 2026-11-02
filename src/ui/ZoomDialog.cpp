#include "ZoomDialog.h"

#include "../core/SourceFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace ls {

ZoomDialog::ZoomDialog(QWidget *parent) : QDialog(parent)
{
	setObjectName("zoomDialog");
	setWindowTitle("Scene Zoom");
	resize(560, 360);

	auto *root = new QVBoxLayout(this);
	auto *info = new QLabel("Membuat scene yang menangkap jendela rapat Zoom dan audionya, siap dikirim ke Program, "
				"rekaman, atau siaran. Zoom harus berjalan di komputer ini; bila rapat belum dimulai, "
				"gambarnya tampil setelah jendela rapat terbuka.",
				this);
	info->setWordWrap(true);
	root->addWidget(info);

	auto *form = new QFormLayout;
	name_ = new QLineEdit("Zoom", this);
	name_->setObjectName("zoomName");
	form->addRow("Nama scene:", name_);

	win_ = new QComboBox(this);
	win_->setObjectName("zoomWinCombo");
	auto *refresh = new QPushButton("Segarkan", this);
	auto *wrow = new QHBoxLayout;
	wrow->addWidget(win_, 1);
	wrow->addWidget(refresh);
	form->addRow("Jendela Zoom:", wrow);

	audio_ = new QCheckBox("Tangkap audio Zoom (suara peserta)", this);
	audio_->setObjectName("zoomAudioChk");
	audio_->setChecked(true);
	mic_ = new QCheckBox("Tambah mikrofon (suara Anda)", this);
	mic_->setObjectName("zoomMicChk");
	form->addRow("", audio_);
	form->addRow("", mic_);

	link_ = new QLineEdit(this);
	link_->setObjectName("zoomLink");
	link_->setPlaceholderText("Tempel tautan rapat Zoom (opsional)");
	auto *open = new QPushButton("Buka Zoom", this);
	open->setObjectName("zoomOpenBtn");
	auto *lrow = new QHBoxLayout;
	lrow->addWidget(link_, 1);
	lrow->addWidget(open);
	form->addRow("Tautan rapat:", lrow);
	root->addLayout(form);

	auto *note = new QLabel("Catatan: gunakan headset agar suara peserta tidak masuk kembali ke mikrofon (gema). "
				"Audio Zoom ditangkap per aplikasi (WASAPI, Windows 10 2004+).",
				this);
	note->setWordWrap(true);
	note->setObjectName("zoomNote");
	root->addWidget(note);
	root->addStretch(1);

	auto *bb = new QDialogButtonBox(this);
	auto *ok = bb->addButton("Buat scene Zoom", QDialogButtonBox::AcceptRole);
	ok->setObjectName("zoomCreateBtn");
	bb->addButton("Batal", QDialogButtonBox::RejectRole);
	root->addWidget(bb);

	connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(refresh, &QPushButton::clicked, this, [this] { refreshWindows(); });
	connect(open, &QPushButton::clicked, this, [this] { openZoom(); });
	refreshWindows();
}

void ZoomDialog::refreshWindows()
{
	win_->clear();
	win_->addItem("Otomatis (jendela rapat Zoom)", QString());
	for (const auto &w : factory::windows())
		if (w.first.contains("zoom", Qt::CaseInsensitive) || w.second.contains("zoom", Qt::CaseInsensitive))
			win_->addItem(w.first, w.second);
}

void ZoomDialog::openZoom()
{
	const QString t = link_->text().trimmed();
	QDesktopServices::openUrl(QUrl(t.isEmpty() ? QString("zoommtg://zoom.us/") : t));
}

QString ZoomDialog::sceneName() const
{
	const QString n = name_->text().trimmed();
	return n.isEmpty() ? QString("Zoom") : n;
}
QString ZoomDialog::windowValue() const { return win_->currentData().toString(); }
bool ZoomDialog::captureAudio() const { return audio_->isChecked(); }
bool ZoomDialog::addMic() const { return mic_->isChecked(); }

} // namespace ls
