#include "AudioMixerWidget.h"

#include "../core/ObsEngine.h"

#include <QActionGroup>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
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
static const float kMaxDb = 12.f;

// ------------------------------------------------------------------ MeterBar

MeterBar::MeterBar(Qt::Orientation o, QWidget *parent) : QWidget(parent), o_(o)
{
	if (o_ == Qt::Horizontal)
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	else
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
	setMinimumSize(o_ == Qt::Horizontal ? QSize(60, 30) : QSize(34, 60));
}

QSize MeterBar::sizeHint() const
{
	return o_ == Qt::Horizontal ? QSize(160, 30) : QSize(34, 100);
}

void MeterBar::resetPeak()
{
	clip_ = false;
	held_[0] = held_[1] = -100.f;
	update();
}

void MeterBar::setLevels(float magL, float magR, float peakL, float peakR)
{
	mag_[0] = magL;
	mag_[1] = magR;
	held_[0] = std::max(peakL, held_[0] - 0.7f);
	held_[1] = std::max(peakR, held_[1] - 0.7f);
	if (peakL > -1.f || peakR > -1.f)
		clip_ = true;
	update();
}

void MeterBar::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.fillRect(rect(), QColor(24, 24, 24));
	const bool h = o_ == Qt::Horizontal;
	const int barT = 8;      // tebal satu batang
	const int gap = 2;
	const int scaleT = 11;   // ruang label skala
	QFont f = font();
	f.setPixelSize(8);
	p.setFont(f);

	const int len = h ? width() : height();
	auto pos = [&](float db) {
		const float c = std::clamp(db, kMinDb, 0.f);
		const float t = (c - kMinDb) / (0.f - kMinDb);
		return int(t * (len - 1));
	};

	const int ticks[] = {0, -6, -12, -20, -30, -40, -50};
	for (int ch = 0; ch < 2; ++ch) {
		const int off = ch * (barT + gap) + 1;
		const int wMag = pos(mag_[ch]);
		const int g = pos(-20.f), y = pos(-9.f);
		auto seg = [&](int a, int b, const QColor &c) {
			if (b <= a)
				return;
			if (h)
				p.fillRect(a, off, b - a, barT, c);
			else
				p.fillRect(off, len - b, barT, b - a, c);
		};
		// jalur redup (skala warna) + level terang
		seg(0, g, QColor(30, 70, 38));
		seg(g, y, QColor(80, 72, 22));
		seg(y, len, QColor(80, 28, 24));
		seg(0, std::min(wMag, g), QColor(70, 205, 95));
		if (wMag > g)
			seg(g, std::min(wMag, y), QColor(240, 210, 60));
		if (wMag > y)
			seg(y, wMag, QColor(235, 70, 55));
		const int xh = pos(held_[ch]);
		p.setPen(QPen(QColor(245, 245, 245), 2));
		if (h)
			p.drawLine(xh, off, xh, off + barT);
		else
			p.drawLine(off, len - xh, off + barT, len - xh);
	}

	// skala dB
	p.setPen(QColor(130, 130, 130));
	const int base = 2 * (barT + gap) + 2;
	for (int db : ticks) {
		const int x = pos(float(db));
		if (h) {
			p.drawLine(x, base - 2, x, base + 2);
			if (height() >= base + scaleT - 2)
				p.drawText(std::clamp(x - 8, 0, width() - 18), base + 3, 18, 9, Qt::AlignCenter, QString::number(db));
		} else {
			const int yy = len - x - 1;
			p.drawLine(base - 2, yy, base + 2, yy);
			if (width() >= base + 20)
				p.drawText(base + 3, std::clamp(yy - 4, 0, len - 9), 22, 9, Qt::AlignLeft | Qt::AlignVCenter,
					   QString::number(db));
		}
	}
}

// ------------------------------------------------------------------ AudioStrip

static int dbToSlider(double db) { return int(std::lround(std::clamp(db, double(kMinDb), double(kMaxDb)) * 10.0)); }
static float dbToMul(double db) { return db <= kMinDb ? 0.f : float(std::pow(10.0, db / 20.0)); }
static double mulToDb(float mul)
{
	if (mul <= 0.00098f)
		return kMinDb;
	return std::clamp(20.0 * std::log10(double(mul)), double(kMinDb), double(kMaxDb));
}

AudioStrip::AudioStrip(const SourceRef &src, bool vertical, QWidget *parent)
	: QWidget(parent), src_(src), vertical_(vertical)
{
	setObjectName("audioStrip");
	setAttribute(Qt::WA_StyledBackground, true);

	name_ = new QLabel(this);
	name_->setObjectName("stripName");
	name_->setToolTip(QString::fromUtf8(obs_source_get_name(src_.get())));

	meter_ = new MeterBar(vertical ? Qt::Vertical : Qt::Horizontal, this);
	clip_ = new QToolButton(this);
	clip_->setObjectName("clipLed");
	clip_->setText("CLIP");
	clip_->setToolTip("Menyala merah bila sinyal menyentuh 0 dB (distorsi). Klik untuk mereset.");
	clip_->setFixedHeight(14);

	fader_ = new FaderSlider(vertical ? Qt::Vertical : Qt::Horizontal, this);
	fader_->setObjectName("fader");
	fader_->setRange(int(kMinDb * 10), int(kMaxDb * 10));
	fader_->setToolTip("Volume. Klik dua kali = 0 dB.");

	dbSpin_ = new QDoubleSpinBox(this);
	dbSpin_->setObjectName("dbSpin");
	dbSpin_->setRange(kMinDb, kMaxDb);
	dbSpin_->setDecimals(1);
	dbSpin_->setSingleStep(0.5);
	dbSpin_->setSuffix(" dB");
	dbSpin_->setSpecialValueText("-∞ dB");
	dbSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
	dbSpin_->setAlignment(Qt::AlignCenter);
	dbSpin_->setKeyboardTracking(false);

	mute_ = new QToolButton(this);
	mute_->setObjectName("muteBtn");
	mute_->setCheckable(true);
	mute_->setText("Mute");
	mute_->setToolTip("Bisukan sumber ini (juga dari siaran/rekaman)");

	monitor_ = new QToolButton(this);
	monitor_->setObjectName("monitorBtn");
	monitor_->setCheckable(true);
	monitor_->setText("Dengar");
	monitor_->setToolTip("Dengarkan di speaker/headphone operator.\nMatikan untuk mikrofon (gema) dan audio desktop.");

	menuBtn_ = new QToolButton(this);
	menuBtn_->setObjectName("stripMenu");
	menuBtn_->setText("⚙");
	menuBtn_->setPopupMode(QToolButton::InstantPopup);
	menuBtn_->setToolTip("Pengaturan sumber: filter, monitor, mono, balance, sinkron, perangkat");

	if (!vertical) {
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(8, 5, 8, 5);
		v->setSpacing(3);
		auto *top = new QHBoxLayout();
		top->setSpacing(4);
		top->addWidget(name_, 1);
		top->addWidget(clip_);
		top->addWidget(monitor_);
		top->addWidget(mute_);
		top->addWidget(menuBtn_);
		v->addLayout(top);
		v->addWidget(meter_);
		auto *row = new QHBoxLayout();
		row->setSpacing(6);
		row->addWidget(fader_, 1);
		dbSpin_->setFixedWidth(74);
		row->addWidget(dbSpin_);
		v->addLayout(row);
	} else {
		setFixedWidth(104);
		for (auto *b : {mute_, monitor_, menuBtn_})
			b->setStyleSheet("padding:3px 3px; min-width:0px;");
		auto *v = new QVBoxLayout(this);
		v->setContentsMargins(5, 5, 5, 5);
		v->setSpacing(3);
		name_->setAlignment(Qt::AlignCenter);
		v->addWidget(name_);
		auto *mid = new QHBoxLayout();
		mid->setSpacing(4);
		mid->addWidget(meter_, 0);
		fader_->setMinimumHeight(64);
		mid->addWidget(fader_, 1, Qt::AlignHCenter);
		v->addLayout(mid, 1);
		v->addWidget(clip_);
		v->addWidget(dbSpin_);
		v->addWidget(mute_);
		auto *b = new QHBoxLayout();
		b->setSpacing(3);
		b->addWidget(monitor_, 1);
		b->addWidget(menuBtn_);
		v->addLayout(b);
	}

	// Nama dipotong dengan "…" sesuai lebar.
	name_->setText(QFontMetrics(name_->font()).elidedText(QString::fromUtf8(obs_source_get_name(src_.get())),
							       Qt::ElideRight, vertical ? 90 : 220));

	buildMenu();

	connect(fader_, &QSlider::valueChanged, this, [this](int v) {
		const double db = v / 10.0;
		obs_source_set_volume(src_.get(), dbToMul(db));
		QSignalBlocker b(dbSpin_);
		dbSpin_->setValue(db);
	});
	connect(fader_, &FaderSlider::resetRequested, this, [this] { setVolumeDb(0.0); });
	connect(dbSpin_, &QDoubleSpinBox::valueChanged, this, [this](double db) { setVolumeDb(db); });
	connect(mute_, &QToolButton::toggled, this, [this](bool on) {
		obs_source_set_muted(src_.get(), on);
		setProperty("muted", on);
		style()->unpolish(this);
		style()->polish(this);
		mute_->setText(on ? "MUTED" : "Mute");
	});
	connect(monitor_, &QToolButton::toggled, this, [this](bool on) { setMonitoring(on); });
	connect(clip_, &QToolButton::clicked, this, [this] {
		meter_->resetPeak();
		clip_->setProperty("lit", false);
		clip_->style()->unpolish(clip_);
		clip_->style()->polish(clip_);
	});

	syncFromSource();

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

void AudioStrip::syncFromSource()
{
	// Cerminkan keadaan libobs ke UI tanpa memicu sinyal balik.
	const double db = mulToDb(obs_source_get_volume(src_.get()));
	{
		QSignalBlocker a(fader_), b(dbSpin_);
		fader_->setValue(dbToSlider(db));
		dbSpin_->setValue(db);
	}
	const bool m = obs_source_muted(src_.get());
	if (mute_->isChecked() != m) {
		QSignalBlocker b(mute_);
		mute_->setChecked(m);
		setProperty("muted", m);
		style()->unpolish(this);
		style()->polish(this);
		mute_->setText(m ? "MUTED" : "Mute");
	}
	const bool mon = monitoring();
	if (monitor_->isChecked() != mon) {
		QSignalBlocker b(monitor_);
		monitor_->setChecked(mon);
	}
}

void AudioStrip::buildMenu()
{
	auto *menu = new QMenu(menuBtn_);

	// --- mode monitor ---
	QMenu *mon = menu->addMenu("Mode monitor");
	auto *grp = new QActionGroup(mon);
	struct M {
		const char *label;
		obs_monitoring_type t;
	};
	static const M modes[] = {{"Mati (tidak didengar operator)", OBS_MONITORING_TYPE_NONE},
				  {"Dengar + ikut siaran/rekaman", OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT},
				  {"Dengar saja (TIDAK ikut siaran)", OBS_MONITORING_TYPE_MONITOR_ONLY}};
	for (const M &m : modes) {
		QAction *a = mon->addAction(m.label);
		a->setCheckable(true);
		a->setActionGroup(grp);
		const auto t = m.t;
		connect(a, &QAction::triggered, this, [this, t] {
			obs_source_set_monitoring_type(src_.get(), t);
			syncFromSource();
		});
	}
	connect(mon, &QMenu::aboutToShow, this, [this, grp] {
		const auto cur = obs_source_get_monitoring_type(src_.get());
		for (QAction *a : grp->actions())
			a->setChecked(false);
		const int idx = cur == OBS_MONITORING_TYPE_NONE ? 0 : cur == OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT ? 1 : 2;
		grp->actions().value(idx)->setChecked(true);
	});

	// --- filter ---
	QMenu *fm = menu->addMenu("Filter audio");
	struct F {
		const char *id;
		const char *label;
	};
	static const F kFilters[] = {{"noise_suppress_filter_v2", "Peredam bising"},
				     {"noise_gate_filter", "Noise gate"},
				     {"compressor_filter", "Kompresor"},
				     {"limiter_filter", "Limiter"},
				     {"gain_filter", "Penguat (gain +)"},
				     {"expander_filter", "Expander"}};
	for (const F &f : kFilters) {
		const QString id = f.id;
		QAction *a = fm->addAction(f.label);
		a->setCheckable(true);
		a->setEnabled(obs_source_get_display_name(f.id) != nullptr);
		if (!a->isEnabled())
			a->setToolTip("Tidak tersedia pada build ini");
		connect(a, &QAction::toggled, this, [this, id, a](bool on) {
			if (!toggleFilter(id, on)) {
				QSignalBlocker b(a);
				a->setChecked(!on);
			}
		});
	}
	connect(fm, &QMenu::aboutToShow, this, [this, fm] {
		for (QAction *a : fm->actions()) {
			// cocokkan lewat urutan kFilters
			const int i = int(fm->actions().indexOf(a));
			QSignalBlocker b(a);
			a->setChecked(hasFilter(kFilters[i].id));
		}
	});

	menu->addSeparator();

	// --- mono ---
	QAction *mono = menu->addAction("Paksa mono (gabung kiri+kanan)");
	mono->setCheckable(true);
	connect(mono, &QAction::toggled, this, [this](bool on) {
		uint32_t fl = obs_source_get_flags(src_.get());
		fl = on ? (fl | OBS_SOURCE_FLAG_FORCE_MONO) : (fl & ~uint32_t(OBS_SOURCE_FLAG_FORCE_MONO));
		obs_source_set_flags(src_.get(), fl);
	});

	// --- balance ---
	menu->addAction("Keseimbangan kiri/kanan…", this, [this] {
		bool ok = false;
		const int cur = int(std::lround((obs_source_get_balance_value(src_.get()) - 0.5f) * 200.f));
		const int v = QInputDialog::getInt(this, "Keseimbangan",
						   "-100 = penuh kiri, 0 = tengah, +100 = penuh kanan:", cur, -100, 100, 5, &ok);
		if (ok)
			obs_source_set_balance_value(src_.get(), 0.5f + float(v) / 200.f);
	});

	// --- sinkron ---
	menu->addAction("Offset sinkron audio…", this, [this] {
		bool ok = false;
		const int cur = int(obs_source_get_sync_offset(src_.get()) / 1000000);
		const int v = QInputDialog::getInt(this, "Offset sinkron",
						   "Tunda audio (ms). Naikkan bila suara datang lebih dulu dari gambar:", cur,
						   -950, 20000, 10, &ok);
		if (ok)
			obs_source_set_sync_offset(src_.get(), int64_t(v) * 1000000);
	});

	menu->addSeparator();
	menu->addAction("Ganti perangkat…", this, [this] { changeDevice(); });
	menu->addAction("Setel volume ke 0 dB", this, [this] { setVolumeDb(0.0); });
	menuBtn_->setMenu(menu);

	connect(menu, &QMenu::aboutToShow, this, [this, mono] {
		QSignalBlocker b(mono);
		mono->setChecked(obs_source_get_flags(src_.get()) & OBS_SOURCE_FLAG_FORCE_MONO);
	});
}

void AudioStrip::changeDevice()
{
	obs_properties_t *props = obs_source_properties(src_.get());
	if (!props)
		return;
	obs_property_t *list = nullptr;
	for (const char *key : {"device_id", "audio_device_id", "device"}) {
		obs_property_t *p = obs_properties_get(props, key);
		if (p && obs_property_get_type(p) == OBS_PROPERTY_LIST) {
			list = p;
			break;
		}
	}
	if (!list) {
		obs_properties_destroy(props);
		QMessageBox::information(this, "Perangkat", "Sumber ini tidak punya pilihan perangkat.");
		return;
	}
	const QString key = QString::fromUtf8(obs_property_name(list));
	QStringList labels, values;
	for (size_t i = 0; i < obs_property_list_item_count(list); ++i) {
		if (obs_property_list_item_disabled(list, i))
			continue;
		labels << QString::fromUtf8(obs_property_list_item_name(list, i));
		const char *sv = obs_property_list_item_string(list, i);
		values << QString::fromUtf8(sv ? sv : "");
	}
	obs_properties_destroy(props);
	if (labels.isEmpty()) {
		QMessageBox::information(this, "Perangkat", "Tidak ada perangkat terdeteksi.");
		return;
	}
	bool ok = false;
	const QString chosen = QInputDialog::getItem(this, "Ganti perangkat", "Perangkat:", labels, 0, false, &ok);
	if (!ok)
		return;
	obs_data_t *d = obs_source_get_settings(src_.get());
	obs_data_set_string(d, key.toUtf8().constData(), values.value(int(labels.indexOf(chosen))).toUtf8().constData());
	obs_source_update(src_.get(), d);
	obs_data_release(d);
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

bool AudioStrip::muted() const { return obs_source_muted(src_.get()); }

void AudioStrip::setMuted(bool on)
{
	mute_->setChecked(on); // memicu toggled -> libobs
}

void AudioStrip::setVolumeDb(double db)
{
	db = std::clamp(db, double(kMinDb), double(kMaxDb));
	fader_->setValue(dbToSlider(db)); // memicu valueChanged -> libobs + spin
}

double AudioStrip::volumeDb() const { return mulToDb(obs_source_get_volume(src_.get())); }

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
	// Thread audio OBS: hanya simpan nilai; UI membaca lewat timer.
	auto *self = static_cast<AudioStrip *>(param);
	const int n = obs_volmeter_get_nr_channels(self->volmeter_);
	self->nch_.store(n);
	for (int i = 0; i < 2; ++i) {
		const int src = (n <= 1) ? 0 : i; // mono: cerminkan ke dua batang
		self->mag_[i].store(magnitude[src]);
		self->peak_[i].store(peak[src]);
	}
}

void AudioStrip::tick()
{
	meter_->setLevels(mag_[0].load(), mag_[1].load(), peak_[0].load(), peak_[1].load());
	if (meter_->clipped() && !clip_->property("lit").toBool()) {
		clip_->setProperty("lit", true);
		clip_->style()->unpolish(clip_);
		clip_->style()->polish(clip_);
	}
	if (++syncTick_ >= 6) { // ~5x/detik: ikuti perubahan dari luar (mis. Mute semua)
		syncTick_ = 0;
		syncFromSource();
	}
}

// ------------------------------------------------------------------ AudioMixerWidget

AudioMixerWidget::AudioMixerWidget(QWidget *parent) : QWidget(parent)
{
	QSettings qs("LiveStudio", "LiveStudio");
	vertical_ = qs.value("ui/mixerVertical", false).toBool();

	auto *outer = new QVBoxLayout(this);
	outer->setContentsMargins(0, 0, 0, 0);
	outer->setSpacing(0);

	// Bilah alat mixer
	auto *bar = new QWidget(this);
	bar->setObjectName("mixerBar");
	auto *bl = new QHBoxLayout(bar);
	bl->setContentsMargins(6, 4, 6, 4);
	bl->setSpacing(4);
	auto mk = [&](const QString &text, const QString &tip) {
		auto *b = new QToolButton(bar);
		b->setObjectName("miniBtn");
		b->setText(text);
		b->setToolTip(tip);
		return b;
	};
	auto *addBtn = mk("＋ Input", "Tambah input audio (pilih driver dan perangkat)");
	muteAllBtn_ = mk("Mute semua", "Bisukan / aktifkan semua sumber audio");
	muteAllBtn_->setCheckable(true);
	muteAllBtn_->setObjectName("muteAllBtn");
	viewBtn_ = mk("Kanal", "Ganti tampilan: Daftar (baris) atau Kanal (fader vertikal)");
	auto *setBtn = mk("⚙ Audio", "Pengaturan audio: perangkat monitor, mikrofon, audio desktop");
	bl->addWidget(addBtn);
	bl->addWidget(muteAllBtn_);
	bl->addStretch(1);
	bl->addWidget(viewBtn_);
	bl->addWidget(setBtn);
	outer->addWidget(bar);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	inner_ = new QWidget(scroll);
	inner_->setObjectName("mixerInner");
	scroll->setWidget(inner_);
	outer->addWidget(scroll, 1);

	hint_ = new QLabel("Belum ada sumber audio.\nTekan ＋ Input untuk menambah mikrofon, line-in, atau audio aplikasi.", inner_);
	hint_->setAlignment(Qt::AlignCenter);
	hint_->setWordWrap(true);
	hint_->setStyleSheet("color:#8a8a8a; padding:12px;");

	connect(addBtn, &QToolButton::clicked, this, &AudioMixerWidget::addInputRequested);
	connect(setBtn, &QToolButton::clicked, this, &AudioMixerWidget::openSettingsRequested);
	connect(muteAllBtn_, &QToolButton::clicked, this, [this](bool on) { muteAll(on); });
	connect(viewBtn_, &QToolButton::clicked, this, [this] { setVertical(!vertical_); });

	timer_ = new QTimer(this);
	timer_->setInterval(33);
	connect(timer_, &QTimer::timeout, this, [this] {
		for (AudioStrip *s : strips_)
			s->tick();
		if (!strips_.empty()) {
			bool all = true;
			for (AudioStrip *s : strips_)
				all = all && s->muted();
			if (muteAllBtn_->isChecked() != all) {
				QSignalBlocker b(muteAllBtn_);
				muteAllBtn_->setChecked(all);
			}
		}
	});
	timer_->start();
	rebuild();
}

AudioMixerWidget::~AudioMixerWidget()
{
	clear();
}

void AudioMixerWidget::muteAll(bool on)
{
	for (AudioStrip *s : strips_)
		s->setMuted(on);
}

void AudioMixerWidget::setVertical(bool v)
{
	if (v == vertical_)
		return;
	vertical_ = v;
	QSettings("LiveStudio", "LiveStudio").setValue("ui/mixerVertical", v);
	clear();
	rebuild();
	refresh();
	emit viewModeChanged(v);
}

void AudioMixerWidget::rebuild()
{
	// Susun ulang wadah strip sesuai tampilan.
	delete inner_->layout();
	if (vertical_) {
		auto *l = new QHBoxLayout(inner_);
		l->setContentsMargins(4, 4, 4, 4);
		l->setSpacing(4);
		stripsLayout_ = l;
	} else {
		auto *l = new QVBoxLayout(inner_);
		l->setContentsMargins(0, 0, 0, 0);
		l->setSpacing(1);
		stripsLayout_ = l;
	}
	stripsLayout_->addWidget(hint_);
	stripsLayout_->addStretch(1);
	viewBtn_->setText(vertical_ ? "Daftar" : "Kanal");
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
		auto *strip = new AudioStrip(r, vertical_, inner_);
		stripsLayout_->insertWidget(int(strips_.size()) + 1, strip); // setelah hint_
		strips_.push_back(strip);
	}
	hint_->setVisible(strips_.empty());
}

} // namespace ls
