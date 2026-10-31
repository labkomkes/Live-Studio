#include "OutputManager.h"

#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QSettings>
#include <QStandardPaths>

namespace ls {

OutputSettings OutputSettings::load()
{
	QSettings q("LiveStudio", "LiveStudio");
	OutputSettings s;
	s.recordDir = q.value("out/dir", QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) +
						 "/LiveStudio").toString();
	s.server = q.value("out/server", "rtmp://a.rtmp.youtube.com/live2").toString();
	s.key = q.value("out/key").toString();
	s.videoKbps = q.value("out/vkbps", 4500).toInt();
	s.audioKbps = q.value("out/akbps", 160).toInt();
	s.preset = q.value("out/preset", "veryfast").toString();
	return s;
}

void OutputSettings::save(bool rememberKey) const
{
	QSettings q("LiveStudio", "LiveStudio");
	q.setValue("out/dir", recordDir);
	q.setValue("out/server", server);
	if (rememberKey)
		q.setValue("out/key", key);
	else
		q.remove("out/key");
	q.setValue("out/vkbps", videoKbps);
	q.setValue("out/akbps", audioKbps);
	q.setValue("out/preset", preset);
}

OutputManager::OutputManager(QObject *parent) : QObject(parent) {}
OutputManager::~OutputManager() { shutdown(); }

bool OutputManager::makeEncoders(const OutputSettings &s, obs_encoder_t **v, obs_encoder_t **a, const char *tag)
{
	obs_data_t *vd = obs_data_create();
	obs_data_set_string(vd, "rate_control", "CBR");
	obs_data_set_int(vd, "bitrate", s.videoKbps);
	obs_data_set_int(vd, "keyint_sec", 2);
	obs_data_set_string(vd, "preset", s.preset.toUtf8().constData());
	obs_data_set_string(vd, "profile", "high");
	*v = obs_video_encoder_create("obs_x264", (QString("LS video ") + tag).toUtf8().constData(), vd, nullptr);
	obs_data_release(vd);

	obs_data_t *ad = obs_data_create();
	obs_data_set_int(ad, "bitrate", s.audioKbps);
	*a = obs_audio_encoder_create("ffmpeg_aac", (QString("LS audio ") + tag).toUtf8().constData(), ad, 0, nullptr);
	obs_data_release(ad);

	if (!*v || !*a) {
		if (*v) obs_encoder_release(*v);
		if (*a) obs_encoder_release(*a);
		*v = *a = nullptr;
		return false;
	}
	obs_encoder_set_video(*v, obs_get_video());
	obs_encoder_set_audio(*a, obs_get_audio());
	return true;
}

void OutputManager::onSignal(void *param, calldata_t *cd)
{
	// Dipanggil dari thread libobs: teruskan ke thread UI.
	auto *self = static_cast<OutputManager *>(param);
	obs_output_t *out = nullptr;
	calldata_get_ptr(cd, "output", &out);
	const bool isRec = out == self->rec_;
	const int code = int(calldata_int(cd, "code"));
	const char *le = nullptr;
	calldata_get_string(cd, "last_error", &le);
	const QString err = le ? QString::fromUtf8(le) : QString();
	QMetaObject::invokeMethod(self, [self, isRec, code, err] {
		if (isRec) {
			self->recActive_ = false;
			emit self->recordingChanged(false, code == 0 ? "Rekaman selesai" : "Rekaman berhenti: " + err);
		} else {
			self->strActive_ = false;
			emit self->streamingChanged(false, code == 0 ? "Siaran selesai"
								    : "Siaran terputus/gagal: " + err +
									      QString(" (kode %1)").arg(code));
		}
	}, Qt::QueuedConnection);
}

bool OutputManager::startRecording(QString *error)
{
	if (recording())
		return true;
	const OutputSettings s = OutputSettings::load();
	QDir().mkpath(s.recordDir);
	recPath_ = s.recordDir + "/LiveStudio-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".mkv";

	if (rec_) { obs_output_release(rec_); rec_ = nullptr; }
	if (recV_) { obs_encoder_release(recV_); recV_ = nullptr; }
	if (recA_) { obs_encoder_release(recA_); recA_ = nullptr; }
	if (!makeEncoders(s, &recV_, &recA_, "rec")) {
		if (error) *error = "Encoder x264/AAC tidak tersedia (plugin obs-x264 / obs-ffmpeg belum dimuat).";
		return false;
	}
	obs_data_t *od = obs_data_create();
	obs_data_set_string(od, "path", recPath_.toUtf8().constData());
	rec_ = obs_output_create("ffmpeg_muxer", "LS record", od, nullptr);
	obs_data_release(od);
	if (!rec_) {
		if (error) *error = "Output rekaman (ffmpeg_muxer) tidak tersedia.";
		return false;
	}
	obs_output_set_video_encoder(rec_, recV_);
	obs_output_set_audio_encoder(rec_, recA_, 0);
	signal_handler_t *sh = obs_output_get_signal_handler(rec_);
	signal_handler_connect(sh, "stop", &OutputManager::onSignal, this);
	if (!obs_output_start(rec_)) {
		if (error) *error = QString("Gagal memulai rekaman: %1").arg(QString::fromUtf8(obs_output_get_last_error(rec_)));
		return false;
	}
	recActive_ = true;
	emit recordingChanged(true, recPath_);
	return true;
}

void OutputManager::stopRecording()
{
	if (rec_ && recActive_)
		obs_output_stop(rec_);
}

bool OutputManager::startStreaming(QString *error)
{
	if (streaming())
		return true;
	const OutputSettings s = OutputSettings::load();
	if (s.server.trimmed().isEmpty() || s.key.trimmed().isEmpty()) {
		if (error) *error = "Isi alamat server dan kunci siaran di Pengaturan Output lebih dulu.";
		return false;
	}
	if (str_) { obs_output_release(str_); str_ = nullptr; }
	if (strV_) { obs_encoder_release(strV_); strV_ = nullptr; }
	if (strA_) { obs_encoder_release(strA_); strA_ = nullptr; }
	if (service_) { obs_service_release(service_); service_ = nullptr; }
	if (!makeEncoders(s, &strV_, &strA_, "stream")) {
		if (error) *error = "Encoder x264/AAC tidak tersedia.";
		return false;
	}
	obs_data_t *sd = obs_data_create();
	obs_data_set_string(sd, "server", s.server.toUtf8().constData());
	obs_data_set_string(sd, "key", s.key.toUtf8().constData());
	service_ = obs_service_create("rtmp_custom", "LS service", sd, nullptr);
	obs_data_release(sd);
	str_ = obs_output_create("rtmp_output", "LS stream", nullptr, nullptr);
	if (!service_ || !str_) {
		if (error) *error = "Output RTMP tidak tersedia (plugin obs-outputs / rtmp-services).";
		return false;
	}
	obs_output_set_video_encoder(str_, strV_);
	obs_output_set_audio_encoder(str_, strA_, 0);
	obs_output_set_service(str_, service_);
	signal_handler_t *sh = obs_output_get_signal_handler(str_);
	signal_handler_connect(sh, "stop", &OutputManager::onSignal, this);
	if (!obs_output_start(str_)) {
		if (error) *error = QString("Gagal memulai siaran: %1").arg(QString::fromUtf8(obs_output_get_last_error(str_)));
		return false;
	}
	strActive_ = true;
	emit streamingChanged(true, s.server);
	return true;
}

void OutputManager::stopStreaming()
{
	if (str_ && strActive_)
		obs_output_stop(str_);
}

void OutputManager::shutdown()
{
	// force_stop hanya pada output yang masih aktif: pada output yang sudah berhenti ia membuat
	// obs_output_release menunggu selamanya (terbukti hang pada uji).
	if (rec_) {
		if (obs_output_active(rec_))
			obs_output_force_stop(rec_);
		obs_output_release(rec_);
		rec_ = nullptr;
	}
	if (str_) {
		if (obs_output_active(str_))
			obs_output_force_stop(str_);
		obs_output_release(str_);
		str_ = nullptr;
	}
	for (obs_encoder_t **e : {&recV_, &recA_, &strV_, &strA_})
		if (*e) { obs_encoder_release(*e); *e = nullptr; }
	if (service_) { obs_service_release(service_); service_ = nullptr; }
	recActive_ = strActive_ = false;
}

uint64_t OutputManager::recordingBytes() const { return rec_ ? obs_output_get_total_bytes(rec_) : 0; }
uint64_t OutputManager::streamBytes() const { return str_ ? obs_output_get_total_bytes(str_) : 0; }
int OutputManager::streamDropped() const { return str_ ? obs_output_get_frames_dropped(str_) : 0; }
int OutputManager::streamTotal() const { return str_ ? obs_output_get_total_frames(str_) : 0; }

} // namespace ls
