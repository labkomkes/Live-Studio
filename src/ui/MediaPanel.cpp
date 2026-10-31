#include "MediaPanel.h"

#include "../core/SourceFactory.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <vector>

namespace ls {

void SeekSlider::mousePressEvent(QMouseEvent *e)
{
	if (e->button() == Qt::LeftButton && maximum() > minimum()) {
		const int span = width();
		const int v = minimum() + int(double(e->position().x()) / double(span) * (maximum() - minimum()));
		setValue(std::clamp(v, minimum(), maximum()));
		emit sliderMoved(value());
	}
	QSlider::mousePressEvent(e);
}

QString MediaPanel::formatTime(int64_t ms)
{
	if (ms < 0)
		ms = 0;
	const int64_t s = ms / 1000;
	const int h = int(s / 3600), m = int((s / 60) % 60), sec = int(s % 60);
	return h > 0 ? QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'))
		     : QString("%1:%2").arg(m, 2, 10, QChar('0')).arg(sec, 2, 10, QChar('0'));
}

MediaPanel::MediaPanel(QWidget *parent) : QWidget(parent)
{
	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(8, 6, 8, 6);
	v->setSpacing(6);

	combo_ = new QComboBox(this);
	combo_->setObjectName("mediaCombo");
	v->addWidget(combo_);

	seek_ = new SeekSlider(this);
	seek_->setObjectName("mediaSeek");
	seek_->setRange(0, 1000);
	v->addWidget(seek_);

	auto *row = new QHBoxLayout();
	row->setSpacing(4);
	restartBtn_ = new QPushButton("⏮", this);
	restartBtn_->setToolTip("Ulang dari awal");
	playBtn_ = new QPushButton("▶", this);
	playBtn_->setToolTip("Putar / Jeda");
	stopBtn_ = new QPushButton("⏹", this);
	stopBtn_->setToolTip("Berhenti (kembali ke awal)");
	for (auto *b : {restartBtn_, playBtn_, stopBtn_}) {
		b->setObjectName("mediaBtn");
		b->setFixedWidth(40);
	}
	loop_ = new QCheckBox("Ulangi", this);
	time_ = new QLabel("0:00 / 0:00", this);
	time_->setObjectName("mediaTime");
	time_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	row->addWidget(restartBtn_);
	row->addWidget(playBtn_);
	row->addWidget(stopBtn_);
	row->addWidget(loop_);
	row->addStretch(1);
	row->addWidget(time_);
	v->addLayout(row);

	hint_ = new QLabel("Belum ada video.\nTambahkan lewat ＋ Sumber > Video.", this);
	hint_->setAlignment(Qt::AlignCenter);
	hint_->setStyleSheet("color:#8a8a8a;");
	v->addWidget(hint_);
	v->addStretch(1);

	connect(restartBtn_, &QPushButton::clicked, this, [this] { restart(); });
	connect(playBtn_, &QPushButton::clicked, this, [this] { playPause(); });
	connect(stopBtn_, &QPushButton::clicked, this, [this] { stop(); });
	connect(seek_, &QSlider::sliderPressed, this, [this] { dragging_ = true; });
	connect(seek_, &QSlider::sliderReleased, this, [this] {
		dragging_ = false;
		if (obs_source_t *s = current()) {
			const int64_t dur = obs_source_media_get_duration(s);
			obs_source_media_set_time(s, dur * seek_->value() / seek_->maximum());
			obs_source_release(s);
		}
	});
	connect(seek_, &QSlider::sliderMoved, this, [this](int v) {
		if (obs_source_t *s = current()) {
			const int64_t dur = obs_source_media_get_duration(s);
			time_->setText(formatTime(dur * v / seek_->maximum()) + " / " + formatTime(dur));
			obs_source_release(s);
		}
	});
	connect(loop_, &QCheckBox::toggled, this, [this](bool on) {
		if (obs_source_t *s = current()) {
			obs_data_t *d = obs_source_get_settings(s);
			obs_data_set_bool(d, "looping", on);
			obs_source_update(s, d);
			obs_data_release(d);
			obs_source_release(s);
		}
	});
	connect(combo_, &QComboBox::currentIndexChanged, this, [this] {
		if (obs_source_t *s = current()) {
			obs_data_t *d = obs_source_get_settings(s);
			QSignalBlocker b(loop_);
			loop_->setChecked(obs_data_get_bool(d, "looping"));
			obs_data_release(d);
			obs_source_release(s);
		}
	});

	timer_ = new QTimer(this);
	timer_->setInterval(100);
	connect(timer_, &QTimer::timeout, this, [this] { tick(); });
	timer_->start();
	refresh();
}

MediaPanel::~MediaPanel() = default;

static bool enumMedia(void *param, obs_source_t *s)
{
	auto *out = static_cast<std::vector<std::string> *>(param);
	if (obs_source_removed(s))
		return true;
	const char *id = obs_source_get_id(s);
	if (id && std::string(id) == "ffmpeg_source") {
		// Bumper memakai sumber internal berawalan "LS "; jangan tampilkan.
		const std::string n = obs_source_get_name(s);
		if (n.rfind("LS ", 0) != 0)
			out->push_back(n);
	}
	return true;
}

void MediaPanel::refresh()
{
	std::vector<std::string> names;
	obs_enum_sources(enumMedia, &names);
	QStringList want;
	for (auto &n : names)
		want << QString::fromStdString(n);
	want.sort();

	QStringList have;
	for (int i = 0; i < combo_->count(); ++i)
		have << combo_->itemText(i);
	if (have != want) {
		const QString keep = combo_->currentText();
		QSignalBlocker b(combo_);
		combo_->clear();
		combo_->addItems(want);
		const int idx = combo_->findText(keep);
		combo_->setCurrentIndex(idx >= 0 ? idx : (want.isEmpty() ? -1 : 0));
	}
	const bool any = combo_->count() > 0;
	hint_->setVisible(!any);
	combo_->setVisible(any);
	seek_->setVisible(any);
	restartBtn_->setEnabled(any);
	playBtn_->setEnabled(any);
	stopBtn_->setEnabled(any);
	loop_->setEnabled(any);
}

void MediaPanel::selectSource(const QString &n)
{
	refresh();
	const int i = combo_->findText(n);
	if (i >= 0)
		combo_->setCurrentIndex(i);
}

void MediaPanel::clear()
{
	QSignalBlocker b(combo_);
	combo_->clear();
}

QString MediaPanel::currentName() const
{
	return combo_->currentText();
}

obs_source_t *MediaPanel::current() const
{
	const QString n = combo_->currentText();
	if (n.isEmpty())
		return nullptr;
	return obs_get_source_by_name(n.toUtf8().constData());
}

void MediaPanel::playPause()
{
	obs_source_t *s = current();
	if (!s)
		return;
	switch (obs_source_media_get_state(s)) {
	case OBS_MEDIA_STATE_PLAYING:
	case OBS_MEDIA_STATE_BUFFERING:
	case OBS_MEDIA_STATE_OPENING:
		obs_source_media_play_pause(s, true);
		break;
	case OBS_MEDIA_STATE_PAUSED:
		obs_source_media_play_pause(s, false);
		break;
	default: // dihentikan / selesai: mulai dari awal
		obs_source_media_restart(s);
		break;
	}
	obs_source_release(s);
}

void MediaPanel::restart()
{
	if (obs_source_t *s = current()) {
		obs_source_media_restart(s);
		obs_source_release(s);
	}
}

void MediaPanel::stop()
{
	if (obs_source_t *s = current()) {
		obs_source_media_stop(s);
		obs_source_release(s);
	}
}

void MediaPanel::seekTo(int64_t ms)
{
	if (obs_source_t *s = current()) {
		obs_source_media_set_time(s, ms);
		obs_source_release(s);
	}
}

void MediaPanel::tick()
{
	obs_source_t *s = current();
	if (!s) {
		time_->setText("0:00 / 0:00");
		return;
	}
	const int64_t dur = obs_source_media_get_duration(s);
	const int64_t pos = obs_source_media_get_time(s);
	const auto st = obs_source_media_get_state(s);
	obs_source_release(s);

	const bool playing = st == OBS_MEDIA_STATE_PLAYING || st == OBS_MEDIA_STATE_BUFFERING ||
			     st == OBS_MEDIA_STATE_OPENING;
	playBtn_->setText(playing ? "⏸" : "▶");
	if (!dragging_) {
		QSignalBlocker b(seek_);
		seek_->setValue(dur > 0 ? int(pos * seek_->maximum() / dur) : 0);
		time_->setText(formatTime(pos) + " / " + formatTime(dur));
	}
}

} // namespace ls
