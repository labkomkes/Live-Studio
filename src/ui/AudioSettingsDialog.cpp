#include "AudioSettingsDialog.h"

#include "../core/SourceFactory.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include <obs.h>

namespace ls {

static bool monitorCb(void *param, const char *name, const char *id)
{
	auto *box = static_cast<QComboBox *>(param);
	box->addItem(QString::fromUtf8(name), QString::fromUtf8(id));
	return true;
}

static void fillDevices(QComboBox *box, const ObsEngine::ListItems &items, const char *sourceName)
{
	box->addItem("Bawaan sistem (default)", "default");
	for (const auto &it : items)
		if (it.second != "default")
			box->addItem(it.first, it.second);
	if (obs_source_t *s = obs_get_source_by_name(sourceName)) {
		obs_data_t *d = obs_source_get_settings(s);
		const int i = box->findData(QString::fromUtf8(obs_data_get_string(d, "device_id")));
		if (i >= 0)
			box->setCurrentIndex(i);
		obs_data_release(d);
		obs_source_release(s);
	}
}

AudioSettingsDialog::AudioSettingsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Pengaturan Audio");
	setMinimumWidth(480);
	auto *v = new QVBoxLayout(this);

	auto *intro = new QLabel("Agar suara terdengar di komputer operator, pilih perangkat keluaran di bawah, "
				 "lalu aktifkan tombol <b>Dengar</b> pada sumber di Audio Mixer. "
				 "Video yang ditambahkan otomatis dalam keadaan Dengar.",
				 this);
	intro->setWordWrap(true);
	v->addWidget(intro);

	auto *form = new QFormLayout();
	monitor_ = new QComboBox(this);
	monitor_->setObjectName("monitorDevice");
	mic_ = new QComboBox(this);
	desktop_ = new QComboBox(this);
	form->addRow("Keluaran monitor (speaker / headphone):", monitor_);
	form->addRow("Mikrofon:", mic_);
	form->addRow("Audio desktop (suara komputer):", desktop_);
	v->addLayout(form);

	status_ = new QLabel(this);
	status_->setWordWrap(true);
	v->addWidget(status_);

	if (obs_audio_monitoring_available()) {
		obs_enum_audio_monitoring_devices(monitorCb, monitor_);
		const char *cn = nullptr, *cid = nullptr;
		obs_get_audio_monitoring_device(&cn, &cid);
		const int i = monitor_->findData(QString::fromUtf8(cid ? cid : "default"));
		if (i >= 0)
			monitor_->setCurrentIndex(i);
		status_->setText("Monitoring audio tersedia.");
	} else {
		monitor_->setEnabled(false);
		status_->setText("Monitoring audio tidak tersedia pada build ini (hanya ada di Windows/macOS/PulseAudio).");
	}
	fillDevices(mic_, factory::micDevices(), "Audio Mic");
	fillDevices(desktop_, factory::desktopAudioDevices(), "Audio Desktop");

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	v->addWidget(bb);
	connect(bb, &QDialogButtonBox::accepted, this, [this] {
		apply();
		accept();
	});
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AudioSettingsDialog::apply()
{
	if (monitor_->isEnabled() && monitor_->count() > 0) {
		const QString id = monitor_->currentData().toString();
		const QString name = monitor_->currentText();
		if (obs_set_audio_monitoring_device(name.toUtf8().constData(), id.toUtf8().constData())) {
			QSettings qs("LiveStudio", "LiveStudio");
			qs.setValue("audio/monitorId", id);
			qs.setValue("audio/monitorName", name);
		}
	}
	auto setDev = [](const char *src, QComboBox *box) {
		obs_source_t *s = obs_get_source_by_name(src);
		if (!s)
			return;
		obs_data_t *d = obs_source_get_settings(s);
		obs_data_set_string(d, "device_id", box->currentData().toString().toUtf8().constData());
		obs_source_update(s, d);
		obs_data_release(d);
		obs_source_release(s);
	};
	setDev("Audio Mic", mic_);
	setDev("Audio Desktop", desktop_);
}

} // namespace ls
