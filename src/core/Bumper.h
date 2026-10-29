// Live Studio - bumper video (intro/outro/stinger sederhana) untuk webinar.
#pragma once

#include "SceneManager.h"

#include <QObject>
#include <QString>
#include <QTimer>

namespace ls {

class Bumper : public QObject {
	Q_OBJECT
public:
	explicit Bumper(SceneManager *sm, QObject *parent = nullptr);
	~Bumper() override;

	// Memutar file video sebagai bumper di Program, lalu pindah ke `thenScene`
	// (kosong = kembali ke scene Program sebelumnya).
	bool play(const QString &file, const QString &thenScene);
	void stop();
	bool isPlaying() const { return playing_; }
	void releaseAll();

signals:
	void started();
	void finished();

private slots:
	void onMediaEnded();
	void armFallback();

private:
	bool ensureScene();
	void finish();
	static void mediaEndedCb(void *param, calldata_t *cd);

	SceneManager *sm_;
	SourceRef scene_;
	SourceRef media_;
	QTimer *fallback_;
	bool playing_ = false;
	bool signalConnected_ = false;
	QString thenScene_;
};

} // namespace ls
