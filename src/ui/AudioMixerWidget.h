// Live Studio - audio mixer: fader, mute, VU meter per sumber audio.
#pragma once

#include "../core/Raii.h"

#include <QWidget>

#include <atomic>
#include <vector>

class QSlider;
class QToolButton;
class QLabel;
class QTimer;
class QVBoxLayout;

namespace ls {

class MeterBar : public QWidget {
	Q_OBJECT
public:
	explicit MeterBar(QWidget *parent = nullptr);
	void setLevels(float magnitudeDb, float peakDb);
	QSize sizeHint() const override { return QSize(120, 12); }

protected:
	void paintEvent(QPaintEvent *) override;

private:
	float mag_ = -100.f;
	float peak_ = -100.f;
	float held_ = -100.f;
};

class AudioStrip : public QWidget {
	Q_OBJECT
public:
	explicit AudioStrip(const SourceRef &src, QWidget *parent = nullptr);
	~AudioStrip() override;
	QString sourceName() const;
	void tick(); // dipanggil timer UI untuk memperbarui meter
	float magnitudeDb() const { return mag_.load(); } // untuk uji
	bool monitoring() const;
	void setMonitoring(bool on);
	bool hasFilter(const QString &id) const;
	bool toggleFilter(const QString &id, bool on);

private:
	static void volmeterCb(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
			       const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS]);

	SourceRef src_;
	obs_volmeter_t *volmeter_ = nullptr;
	QLabel *name_;
	MeterBar *meter_;
	QSlider *fader_;
	QLabel *dbLabel_;
	QToolButton *mute_;
	QToolButton *monitor_;
	QToolButton *filters_;
	std::atomic<float> mag_{-100.f};
	std::atomic<float> peak_{-100.f};
};

class AudioMixerWidget : public QWidget {
	Q_OBJECT
public:
	explicit AudioMixerWidget(QWidget *parent = nullptr);
	~AudioMixerWidget() override;

	// Membaca ulang semua sumber bertanda audio dari libobs.
	void refresh();
	// Melepas semua reference (sebelum engine dimatikan).
	void clear();
	int stripCount() const { return int(strips_.size()); }

private:
	QVBoxLayout *stripsLayout_;
	QLabel *hint_ = nullptr; // petunjuk saat belum ada sumber audio
	std::vector<AudioStrip *> strips_;
	QTimer *timer_;
};

} // namespace ls
