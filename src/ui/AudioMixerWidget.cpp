#include "AudioMixerWidget.h"

#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>

#include <obs.h>

namespace ls {

static const float kMinDb = -60.f;

// ------------------------------------------------------------------ MeterBar

MeterBar::MeterBar(QWidget *parent) : QWidget(parent)
{
	setMinimumHeight(12);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MeterBar::setLevels(float magnitudeDb, float peakDb)
{
	mag_ = magnitudeDb;
	peak_ = peakDb;
	held_ = std::max(peakDb, held_ - 0.8f); // peak-hold turun perlahan
	update();
}

void MeterBar::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(30, 30, 30));

	auto toX = [&](float db) {
		const float c = std::clamp(db, kMinDb, 0.f);
		return int((c - kMinDb) / (0.f - kMinDb) * width());
	};

	const int wMag = toX(mag_);
	const int greenEnd = toX(-20.f);
	const int yellowEnd = toX(-9.f);

	p.fillRect(0, 0, std::min(wMag, greenEnd), height(), QColor(60, 190, 80));
	if (wMag > greenEnd)
		p.fillRect(greenEnd, 0, std::min(wMag, yellowEnd) - greenEnd, height(), QColor(230, 200, 50));
	if (wMag > yellowEnd)
		p.fillRect(yellowEnd, 0, wMag - yellowEnd, height(), QColor(220, 60, 50));

	const int xh = toX(held_);
	p.setPen(QPen(QColor(240, 240, 240), 2));
	p.drawLine(xh, 0, xh, height());
}

// ------------------------------------------------------------------ AudioStrip

static float sliderToMul(int v)
{
	if (v <= 0)
		return 0.f;
	const float db = kMinDb + (float(v) / 100.f) * (0.f - kMinDb);
	return std::pow(10.f, db / 20.f);
}

static int mulToSlider(float mul)
{
	if (mul <= 0.0001f)
		return 0;
	const float db = 20.f * std::log10(mul);
	const float v = (db - kMinDb) / (0.f - kMinDb) * 100.f;
	return int(std::clamp(v, 0.f, 100.f));
}

AudioStrip::AudioStrip(const SourceRef &src, QWidget *parent) : QWidget(parent), src_(src)
{
	auto *lay = new QVBoxLayout(this);
	lay->setContentsMargins(6, 4, 6, 4);
	lay->setSpacing(2);

	auto *top = new QHBoxLayout();
	top->setSpacing(4);
	name_ = new QLabel(QString::fromUtf8(obs_source_get_name(src_.get())), this);
	name_->setStyleSheet("font-weight:600;");
	top->addWidget(name_, 1);

	monitor_ = new QToolButton(this);
	monitor_->setObjectName("monitorBtn");
	monitor_->setCheckable(true);
	monitor_->setText("Dengar");
	monitor_->setToolTip("Dengarkan sumber ini di speaker/headphone operator.\n"
			     "Matikan untuk mikrofon (menghindari gema) dan audio desktop.");
	monitor_->setChecked(monitoring());
	top->addWidget(monitor_);

	filters_ = new QToolButton(this);
	filters_->setText("Filter ▾");
	filters_->setPopupMode(QToolButton::InstantPopup);
	filters_->setToolTip("Peredam bising, noise gate, kompresor, limiter, penguat, offset sinkron");
	top->addWidget(filters_);

	mute_ = new QToolButton(this);
	mute_->setCheckable(true);
	mute_->setText("Mute");
	mute_->setChecked(obs_source_muted(src_.get()));
	top->addWidget(mute_);
	lay->addLayout(top);

	meter_ = new MeterBar(this);
	lay->addWidget(meter_);

	auto *row = new QHBoxLayout();
	fader_ = new QSlider(Qt::Horizontal, this);
	fader_->setRange(0, 100);
	fader_->setValue(mulToSlider(obs_source_get_volume(src_.get())));
	dbLabel_ = new QLabel(this);
	dbLabel_->setMinimumWidth(52);
	dbLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
	row->addWidget(fader_, 1);
	row->addWidget(dbLabel_);
	lay->addLayout(row);

	auto updateDbText = [this](int v) {
		if (v <= 0)
			dbLabel_->setText("-inf dB");
		else
			dbLabel_->setText(QString("%1 dB").arg(kMinDb + (float(v) / 100.f) * (0.f - kMinDb), 0, 'f', 1));
	};
	updateDbText(fader_->value());

	connect(fader_, &QSlider::valueChanged, this, [this, updateDbText](int v) {
		obs_source_set_volume(src_.get(), sliderToMul(v));
		updateDbText(v);
	});
	connect(mute_, &QToolButton::toggled, this, [this](bool on) { obs_source_set_muted(src_.get(), on); });
	connect(monitor_, &QToolButton::toggled, this, [this](bool on) { setMonitoring(on); });

	struct F {
		const char *id;
		const char *label;
	};
	static const F kFilters[] = {{"noise_suppress_filter_v2", "Peredam bising"},
				     {"noise_gate_filter", "Noise gate"},
				     {"compressor_filter", "Kompresor"},
				     {"limiter_filter", "Limiter"},
				     {"gain_filter", "Penguat (gain)"}};
	auto *menu = new QMenu(filters_);
	for (const F &f : kFilters) {
		const QString id = f.id;
		QAction *a = menu->addAction(f.label);
		a->setCheckable(true);
		a->setEnabled(obs_source_get_display_name(f.id) != nullptr);
		if (!a->isEnabled())
			a->setToolTip("Tidak tersedia pada build ini");
		a->setChecked(hasFilter(id));
		connect(a, &QAction::toggled, this, [this, id, a](bool on) {
			if (!toggleFilter(id, on)) {
				QSignalBlocker b(a);
				a->setChecked(!on);
			}
		});
	}
	menu->addSeparator();
	menu->addAction("Offset sinkron audio…", this, [this] {
		bool ok = false;
		const int cur = int(obs_source_get_sync_offset(src_.get()) / 1000000);
		const int v = QInputDialog::getInt(this, "Offset sinkron",
						   "Tunda audio (ms). Naikkan bila suara datang lebih dulu dari gambar:",
						   cur, -950, 20000, 10, &ok);
		if (ok)
			obs_source_set_sync_offset(src_.get(), int64_t(v) * 1000000);
	});
	filters_->setMenu(menu);

	volmeter_ = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_attach_source(volmeter_, src_.get());
	obs_volmeter_add_callback(volmeter_, &AudioStrip::volmeterCb, this);
}

AudioStrip::~AudioStrip()
{
	if (volmeter_) {
		obs_volmeter_remove_callback(volmeter_, &AudioStrip::volmeterCb, this);
		obs_volmeter_detach_source(volmeter_);
		obs_volmeter_destroy(volmeter_);
		volmeter_ = nullptr;
	}
}

bool AudioStrip::monitoring() const
{
	return obs_source_get_monitoring_type(src_.get()) != OBS_MONITORING_TYPE_NONE;
}

void AudioStrip::setMonitoring(bool on)
{
	obs_source_set_monitoring_type(src_.get(), on ? OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT
						      : OBS_MONITORING_TYPE_NONE);
}

bool AudioStrip::hasFilter(const QString &id) const
{
	obs_source_t *f = obs_source_get_filter_by_name(src_.get(), id.toUtf8().constData());
	if (f)
		obs_source_release(f);
	return f != nullptr;
}

bool AudioStrip::toggleFilter(const QString &id, bool on)
{
	// Nama filter = id supaya mudah dicari; satu filter per jenis per sumber.
	const QByteArray idb = id.toUtf8();
	if (!on) {
		obs_source_t *f = obs_source_get_filter_by_name(src_.get(), idb.constData());
		if (!f)
			return true;
		obs_source_filter_remove(src_.get(), f);
		obs_source_release(f);
		return true;
	}
	if (hasFilter(id))
		return true;
	obs_data_t *d = obs_data_create();
	if (id == "noise_suppress_filter_v2")
		obs_data_set_string(d, "method", "rnnoise");
	else if (id == "limiter_filter")
		obs_data_set_double(d, "threshold", -6.0);
	obs_source_t *f = obs_source_create(idb.constData(), idb.constData(), d, nullptr);
	obs_data_release(d);
	if (!f)
		return false;
	obs_source_filter_add(src_.get(), f);
	obs_source_release(f);
	return true;
}

QString AudioStrip::sourceName() const
{
	return QString::fromUtf8(obs_source_get_name(src_.get()));
}

void AudioStrip::volmeterCb(void *param, const float magnitude[MAX_AUDIO_CHANNELS],
			    const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS])
{
	(void)inputPeak;
	// Thread audio OBS: hanya simpan nilai maksimum antar kanal; UI membaca lewat timer.
	auto *self = static_cast<AudioStrip *>(param);
	float m = -100.f, pk = -100.f;
	for (int i = 0; i < 2; ++i) { // stereo
		m = std::max(m, magnitude[i]);
		pk = std::max(pk, peak[i]);
	}
	self->mag_.store(m);
	self->peak_.store(pk);
}

void AudioStrip::tick()
{
	meter_->setLevels(mag_.load(), peak_.load());
}

// ------------------------------------------------------------------ AudioMixerWidget

AudioMixerWidget::AudioMixerWidget(QWidget *parent) : QWidget(parent)
{
	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *inner = new QWidget(scroll);
	stripsLayout_ = new QVBoxLayout(inner);
	stripsLayout_->setContentsMargins(0, 0, 0, 0);
	stripsLayout_->addStretch(1);
	hint_ = new QLabel("Belum ada sumber audio.\nTambahkan Mikrofon atau Audio Desktop lewat tombol ＋ Sumber.", inner);
	hint_->setAlignment(Qt::AlignCenter);
	hint_->setWordWrap(true);
	hint_->setStyleSheet("color:#8a8a8a; padding:12px;");
	stripsLayout_->insertWidget(0, hint_);
	scroll->setWidget(inner);
	outer->addWidget(scroll);

	timer_ = new QTimer(this);
	timer_->setInterval(33);
	connect(timer_, &QTimer::timeout, this, [this] {
		for (AudioStrip *s : strips_)
			s->tick();
	});
	timer_->start();
}

AudioMixerWidget::~AudioMixerWidget()
{
	clear();
}

void AudioMixerWidget::clear()
{
	for (AudioStrip *s : strips_) {
		stripsLayout_->removeWidget(s);
		delete s;
	}
	strips_.clear();
}

struct EnumAudioCtx {
	std::vector<SourceRef> *out;
};

static bool enumAudioCb(void *param, obs_source_t *src)
{
	auto *ctx = static_cast<EnumAudioCtx *>(param);
	if (obs_source_get_type(src) != OBS_SOURCE_TYPE_INPUT)
		return true;
	if (obs_source_removed(src))
		return true; // sudah dihapus dari scene; jangan tampilkan lagi di mixer
	if (!(obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO))
		return true;
	if (std::string(obs_source_get_name(src)).rfind("LS ", 0) == 0)
		return true; // sumber internal (bumper) tidak ditampilkan
	ctx->out->push_back(SourceRef::borrow(src));
	return true;
}

void AudioMixerWidget::refresh()
{
	std::vector<SourceRef> found;
	EnumAudioCtx ctx{&found};
	obs_enum_sources(enumAudioCb, &ctx);

	// Pertahankan strip yang masih ada; buat ulang bila daftar berubah.
	std::set<std::string> have;
	for (AudioStrip *s : strips_)
		have.insert(s->sourceName().toStdString());
	std::set<std::string> want;
	for (const SourceRef &r : found)
		want.insert(obs_source_get_name(r.get()));
	if (have == want)
		return;

	clear();
	std::sort(found.begin(), found.end(), [](const SourceRef &a, const SourceRef &b) {
		return std::string(obs_source_get_name(a.get())) < std::string(obs_source_get_name(b.get()));
	});
	for (const SourceRef &r : found) {
		auto *strip = new AudioStrip(r, this);
		stripsLayout_->insertWidget(int(strips_.size()), strip);
		strips_.push_back(strip);
	}
	hint_->setVisible(strips_.empty());
}

} // namespace ls
