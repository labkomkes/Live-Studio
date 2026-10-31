// Live Studio - rekaman dan siaran RTMP (x264 + AAC lewat libobs).
#pragma once

#include <QObject>
#include <QString>

#include <obs.h>

namespace ls {

struct OutputSettings {
	QString recordDir;
	QString server;      // rtmp://...
	QString key;
	int videoKbps = 4500;
	int audioKbps = 160;
	QString preset = "veryfast";
	static OutputSettings load();
	void save(bool rememberKey) const;
};

class OutputManager : public QObject {
	Q_OBJECT
public:
	explicit OutputManager(QObject *parent = nullptr);
	~OutputManager() override;

	bool startRecording(QString *error);
	void stopRecording();
	bool startStreaming(QString *error);
	void stopStreaming();
	void shutdown(); // hentikan semua & lepas objek sebelum engine mati

	bool recording() const { return rec_ != nullptr && recActive_; }
	bool streaming() const { return str_ != nullptr && strActive_; }
	QString recordingPath() const { return recPath_; }
	uint64_t recordingBytes() const;
	uint64_t streamBytes() const;
	int streamDropped() const;
	int streamTotal() const;

signals:
	void recordingChanged(bool on, const QString &info);
	void streamingChanged(bool on, const QString &info);

private:
	struct Impl;
	static void onSignal(void *param, calldata_t *cd);
	bool makeEncoders(const OutputSettings &s, obs_encoder_t **v, obs_encoder_t **a, const char *tag);

	obs_output_t *rec_ = nullptr, *str_ = nullptr;
	obs_encoder_t *recV_ = nullptr, *recA_ = nullptr, *strV_ = nullptr, *strA_ = nullptr;
	obs_service_t *service_ = nullptr;
	bool recActive_ = false, strActive_ = false;
	QString recPath_;
};

} // namespace ls
