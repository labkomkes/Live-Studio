// Live Studio - kontrol putar video (play/pause, ulang, stop, geser menit, loop).
#pragma once

#include <QSlider>
#include <QWidget>

#include <obs.h>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

namespace ls {

// Slider yang melompat ke posisi klik (seperti pemutar video).
class SeekSlider : public QSlider {
	Q_OBJECT
public:
	explicit SeekSlider(QWidget *parent = nullptr) : QSlider(Qt::Horizontal, parent) {}

protected:
	void mousePressEvent(QMouseEvent *e) override;
};

class MediaPanel : public QWidget {
	Q_OBJECT
public:
	explicit MediaPanel(QWidget *parent = nullptr);
	~MediaPanel() override;

	void refresh();                      // baca ulang daftar sumber video
	void selectSource(const QString &n); // pilih sumber (mis. yang baru ditambahkan)
	void clear();                        // lepas reference sebelum engine mati
	QString currentName() const;

	// Aksi (juga dipakai pengujian).
	void playPause();
	void restart();
	void stop();
	void seekTo(int64_t ms);
	static QString formatTime(int64_t ms);

private:
	void tick();
	obs_source_t *current() const; // reference baru; pemanggil harus release

	QComboBox *combo_;
	QPushButton *restartBtn_, *playBtn_, *stopBtn_;
	QCheckBox *loop_;
	SeekSlider *seek_;
	QLabel *time_;
	QLabel *hint_;
	QTimer *timer_;
	bool dragging_ = false;
};

} // namespace ls
