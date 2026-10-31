// Live Studio - audio mixer: meter stereo, fader dB, Mute, Dengar (monitor), menu pengaturan per sumber.
// Dua tampilan: Daftar (baris, hemat tempat) dan Kanal (fader vertikal ala mixer sungguhan).
#pragma once

#include "../core/Raii.h"

#include <QSlider>
#include <QWidget>

#include <atomic>
#include <vector>

class QBoxLayout;
class QDoubleSpinBox;
class QLabel;
class QToolButton;
class QTimer;

namespace ls {

// Meter stereo (L/R) dengan skala dB, peak-hold, dan indikator clip.
class MeterBar : public QWidget {
	Q_OBJECT
public:
	explicit MeterBar(Qt::Orientation o, QWidget *parent = nullptr);
	void setLevels(float magL, float magR, float peakL, float peakR);
	bool clipped() const { return clip_; }
	void resetPeak();
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;

private:
	Qt::Orientation o_;
	float mag_[2] = {-100.f, -100.f};
	float held_[2] = {-100.f, -100.f};
	bool clip_ = false;
};

// Slider fader: klik dua kali = kembali ke 0 dB.
class FaderSlider : public QSlider {
	Q_OBJECT
public:
	using QSlider::QSlider;
signals:
	void resetRequested();

protected:
	void mouseDoubleClickEvent(QMouseEvent *) override { emit resetRequested(); }
};

class AudioStrip : public QWidget {
	Q_OBJECT
public:
	AudioStrip(const SourceRef &src, bool vertical, QWidget *parent = nullptr);
	~AudioStrip() override;
	QString sourceName() const;
	void tick(); // timer UI: meter + sinkron nilai dari libobs
	float magnitudeDb() const { return std::max(mag_[0].load(), mag_[1].load()); } // untuk uji
	bool monitoring() const;
	void setMonitoring(bool on);
	bool hasFilter(const QString &id) const;
	bool toggleFilter(const QString &id, bool on);
	void setMuted(bool on);
	bool muted() const;
	// Fader dalam dB (-60..+12); <= -60 dianggap -inf.
	void setVolumeDb(double db);
	double volumeDb() const;

private:
	static void volmeterCb(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
			       const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS]);
	void buildMenu();
	void syncFromSource();
	void changeDevice();

	SourceRef src_;
	bool vertical_;
	obs_volmeter_t *volmeter_ = nullptr;
	QLabel *name_;
	MeterBar *meter_;
	FaderSlider *fader_;
	QDoubleSpinBox *dbSpin_;
	QToolButton *mute_;
	QToolButton *monitor_;
	QToolButton *menuBtn_;
	QToolButton *clip_;
	std::atomic<float> mag_[2] = {{-100.f}, {-100.f}};
	std::atomic<float> peak_[2] = {{-100.f}, {-100.f}};
	std::atomic<int> nch_{2};
	int syncTick_ = 0;
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
	bool vertical() const { return vertical_; }
	void setVertical(bool v);
	void muteAll(bool on);

signals:
	void openSettingsRequested();
	void addInputRequested();
	void viewModeChanged(bool vertical);

private:
	void rebuild();

	QBoxLayout *stripsLayout_ = nullptr;
	QWidget *inner_ = nullptr;
	QLabel *hint_ = nullptr;
	QToolButton *muteAllBtn_ = nullptr;
	QToolButton *viewBtn_ = nullptr;
	std::vector<AudioStrip *> strips_;
	QTimer *timer_;
	bool vertical_ = false;
};

} // namespace ls
