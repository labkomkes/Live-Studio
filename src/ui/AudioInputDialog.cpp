#include "AudioInputDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ls {

AudioInputDialog::AudioInputDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Tambah Input Audio");
	setMinimumWidth(520);
	drivers_ = factory::audioInputDrivers();

	auto *v = new QVBoxLayout(this);
	auto *form = new QFormLayout();
	driver_ = new QComboBox(this);
	driver_->setObjectName("audioDriver");
	for (int i = 0; i < drivers_.size(); ++i) {
		const auto &d = drivers_[i];
		driver_->addItem(d.available ? d.label : d.label + "  (tidak tersedia)", i);
	}
	// Pilih driver pertama yang tersedia.
	for (int i = 0; i < drivers_.size(); ++i)
		if (drivers_[i].available) {
			driver_->setCurrentIndex(i);
			break;
		}
	device_ = new QComboBox(this);
	device_->setObjectName("audioDevice");
	name_ = new QLineEdit(this);
	form->addRow("Driver / jenis input:", driver_);
	form->addRow("Perangkat:", device_);
	form->addRow("Nama di mixer:", name_);
	v->addLayout(form);

	note_ = new QLabel(this);
	note_->setWordWrap(true);
	note_->setStyleSheet("color:#8a8a8a;");
	v->addWidget(note_);

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	v->addWidget(bb);
	connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(driver_, &QComboBox::currentIndexChanged, this, [this] { refreshDevices(); });
	refreshDevices();
}

factory::AudioDriver AudioInputDialog::driver() const
{
	const int i = driver_->currentData().toInt();
	return drivers_.value(i);
}

QString AudioInputDialog::deviceValue() const { return device_->currentData().toString(); }
QString AudioInputDialog::deviceLabel() const { return device_->currentText(); }
QString AudioInputDialog::sourceName() const
{
	const QString n = name_->text().trimmed();
	return n.isEmpty() ? "Audio: " + deviceLabel() : n;
}

void AudioInputDialog::refreshDevices()
{
	const auto drv = driver();
	device_->clear();
	QString msg = drv.note;
	if (!drv.available) {
		msg = "Driver ini tidak tersedia pada build/komputer ini. " + drv.note;
	} else {
		const auto items = factory::audioDriverDevices(drv);
		for (const auto &p : items)
			device_->addItem(p.first, p.second);
		if (items.isEmpty())
			msg = "Tidak ada perangkat terdeteksi untuk driver ini. " + drv.note;
	}
	note_->setText(msg);
	if (auto *ok = findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok))
		ok->setEnabled(drv.available && device_->count() > 0);
}

} // namespace ls
