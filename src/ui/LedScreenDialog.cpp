#include "LedScreenDialog.h"

#include "../core/SceneManager.h"
#include "LedScreenWindow.h"
#include "ObsDisplayWidget.h"
#include "Render.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace ls {

enum Col { cOn, cName, cScene, cFit, cIX, cIY, cIW, cIH, cOX, cOY, cOW, cOH, cCount };

static QString progLabel() { return "● Program (live)"; }
static QString prevLabel() { return "Preview"; }

LedScreenDialog::LedScreenDialog(SceneManager *sm, LedScreen *led, QWidget *parent)
	: QDialog(parent), sm_(sm), led_(led)
{
	setWindowTitle("Layar LED / Mapping");
	setObjectName("ledDialog");
	resize(1180, 720);
	setModal(false);

	auto *root = new QVBoxLayout(this);
	auto *top = new QHBoxLayout();

	// ---- kiri: pratinjau ----
	auto *left = new QVBoxLayout();
	preview_ = new ObsDisplayWidget(this);
	preview_->setMinimumSize(420, 200);
	preview_->setDrawFn([this](uint32_t cx, uint32_t cy) { render::drawLed(sm_, led_->layout(), cx, cy); });
	left->addWidget(new QLabel("Pratinjau layar LED (apa yang dikirim ke keluaran):", this));
	left->addWidget(preview_, 1);
	top->addLayout(left, 3);

	// ---- kanan: pengaturan layar ----
	auto *right = new QVBoxLayout();
	right->addWidget(new QLabel("<b>Ukuran layar LED</b> (piksel):", this));
	auto *sizeRow = new QHBoxLayout();
	sizeCombo_ = new QComboBox(this);
	sizeCombo_->setObjectName("ledSizePreset");
	struct P {
		const char *label;
		int w, h;
	};
	static const P sizes[] = {{"1920 × 1080 (Full HD)", 1920, 1080}, {"3840 × 2160 (4K)", 3840, 2160},
				  {"3840 × 1080 (2 × FHD lebar)", 3840, 1080}, {"2560 × 720", 2560, 720},
				  {"1920 × 540", 1920, 540},                    {"1280 × 360", 1280, 360},
				  {"5760 × 1080 (3 × FHD lebar)", 5760, 1080}, {"7680 × 1080 (4 × FHD lebar)", 7680, 1080},
				  {"Kustom", 0, 0}};
	for (const P &p : sizes)
		sizeCombo_->addItem(p.label, QVariant::fromValue(QPoint(p.w, p.h)));
	w_ = new QSpinBox(this);
	w_->setObjectName("ledW");
	w_->setRange(64, 16384);
	w_->setSuffix(" px");
	h_ = new QSpinBox(this);
	h_->setObjectName("ledH");
	h_->setRange(64, 16384);
	h_->setSuffix(" px");
	sizeRow->addWidget(sizeCombo_, 1);
	sizeRow->addWidget(w_);
	sizeRow->addWidget(new QLabel("×", this));
	sizeRow->addWidget(h_);
	right->addLayout(sizeRow);

	right->addWidget(new QLabel("<b>Bagi layar</b> (preset, isi scene berurutan):", this));
	auto *presetBtn = new QToolButton(this);
	presetBtn->setObjectName("ledPresetBtn");
	presetBtn->setText("Pilih preset pembagian ▾");
	presetBtn->setPopupMode(QToolButton::InstantPopup);
	auto *pm = new QMenu(presetBtn);
	pm->addAction("Satu layar penuh", this, [this] { applyPreset(0, 1); });
	for (int n : {2, 3, 4})
		pm->addAction(QString("Belah %1 kolom (scene berbeda per kolom)").arg(n), this, [this, n] { applyPreset(1, n); });
	pm->addAction("Grid 2 × 2", this, [this] { applyPreset(2, 22); });
	pm->addAction("Grid 3 × 2", this, [this] { applyPreset(2, 32); });
	pm->addSeparator();
	for (int n : {2, 3, 4})
		pm->addAction(QString("Satu scene dibelah %1 (satu gambar lebar)").arg(n), this, [this, n] { applyPreset(3, n); });
	presetBtn->setMenu(pm);
	right->addWidget(presetBtn);

	right->addWidget(new QLabel("<b>Keluaran</b>:", this));
	auto *outRow = new QHBoxLayout();
	monitor_ = new QComboBox(this);
	monitor_->setObjectName("ledMonitor");
	outRow->addWidget(monitor_, 1);
	auto *openBtn = new QPushButton("Buka", this);
	openBtn->setObjectName("ledOpen");
	auto *closeBtn = new QPushButton("Tutup", this);
	outRow->addWidget(openBtn);
	outRow->addWidget(closeBtn);
	right->addLayout(outRow);
	test_ = new QCheckBox("Pola uji (garis slice + nama + ukuran) untuk kalibrasi LED", this);
	test_->setObjectName("ledTest");
	right->addWidget(test_);

	auto *fileRow = new QHBoxLayout();
	auto *saveBtn = new QPushButton("Simpan preset…", this);
	auto *loadBtn = new QPushButton("Muat preset…", this);
	fileRow->addWidget(saveBtn);
	fileRow->addWidget(loadBtn);
	right->addLayout(fileRow);
	auto *info = new QLabel("Slice = potongan scene (kolom <i>Ambil dari scene</i>, % dari kanvas) yang ditempatkan "
				"di posisi piksel pada layar LED. Satu scene bisa dibelah ke beberapa slice, atau tiap slice "
				"diisi scene berbeda. Esc menutup jendela keluaran. Perubahan langsung tersimpan.", this);
	info->setWordWrap(true);
	info->setStyleSheet("color:#8a8a8a;");
	right->addWidget(info);
	right->addStretch(1);
	top->addLayout(right, 2);
	root->addLayout(top, 3);

	// ---- tabel slice ----
	auto *bar = new QHBoxLayout();
	bar->addWidget(new QLabel("<b>Slice</b>", this));
	auto *addB = new QPushButton("＋ Slice", this);
	addB->setObjectName("ledAddSlice");
	auto *delB = new QPushButton("− Hapus", this);
	bar->addWidget(addB);
	bar->addWidget(delB);
	bar->addStretch(1);
	root->addLayout(bar);
	table_ = new QTableWidget(0, cCount, this);
	table_->setObjectName("ledTable");
	table_->setHorizontalHeaderLabels({"Aktif", "Nama", "Ambil dari scene", "Mode", "Ambil X %", "Y %", "Lebar %",
					   "Tinggi %", "Tampil X px", "Y px", "Lebar px", "Tinggi px"});
	table_->horizontalHeader()->setSectionResizeMode(cName, QHeaderView::Stretch);
	table_->horizontalHeader()->setSectionResizeMode(cScene, QHeaderView::ResizeToContents);
	table_->verticalHeader()->setVisible(false);
	table_->setSelectionBehavior(QAbstractItemView::SelectRows);
	root->addWidget(table_, 2);

	// monitor
	auto fillMonitors = [this] {
		monitor_->clear();
		int i = 1;
		for (QScreen *sc : QGuiApplication::screens())
			monitor_->addItem(QString("Monitor %1 — %2 (%3×%4)").arg(i++).arg(sc->name()).arg(sc->geometry().width()).arg(sc->geometry().height()));
		monitor_->addItem("Jendela biasa");
	};
	fillMonitors();

	// sinyal
	connect(sizeCombo_, &QComboBox::activated, this, [this](int i) {
		const QPoint p = sizeCombo_->itemData(i).toPoint();
		if (p.x() > 0) {
			QSignalBlocker a(w_), b(h_);
			w_->setValue(p.x());
			h_->setValue(p.y());
			applySize();
		}
	});
	connect(w_, &QSpinBox::editingFinished, this, [this] { applySize(); });
	connect(h_, &QSpinBox::editingFinished, this, [this] { applySize(); });
	connect(test_, &QCheckBox::toggled, this, [this](bool on) {
		LedLayout L = led_->layout();
		L.testPattern = on;
		led_->setLayout(L);
	});
	connect(openBtn, &QPushButton::clicked, this, [this] {
		const int i = monitor_->currentIndex();
		const auto screens = QGuiApplication::screens();
		openOn(i >= 0 && i < screens.size() ? screens.at(i) : nullptr);
	});
	connect(closeBtn, &QPushButton::clicked, this, [this] { closeOutput(); });
	connect(addB, &QPushButton::clicked, this, [this] {
		LedLayout L = led_->layout();
		LedSlice s;
		s.name = QString("Slice %1").arg(L.slices.size() + 1);
		s.outW = L.width / 2.0;
		s.outH = L.height;
		L.slices.append(s);
		led_->setLayout(L);
		rebuildTable();
	});
	connect(delB, &QPushButton::clicked, this, [this] {
		LedLayout L = led_->layout();
		const int r = table_->currentRow();
		if (r >= 0 && r < L.slices.size() && L.slices.size() > 1) {
			L.slices.removeAt(r);
			led_->setLayout(L);
			rebuildTable();
		}
	});
	connect(saveBtn, &QPushButton::clicked, this, [this] {
		const QString f = QFileDialog::getSaveFileName(this, "Simpan preset layar LED", "layar-led.json", "JSON (*.json)");
		if (!f.isEmpty() && !led_->saveTo(f))
			QMessageBox::warning(this, "Layar LED", "Gagal menyimpan berkas.");
	});
	connect(loadBtn, &QPushButton::clicked, this, [this] {
		const QString f = QFileDialog::getOpenFileName(this, "Muat preset layar LED", QString(), "JSON (*.json)");
		if (!f.isEmpty()) {
			if (!led_->loadFrom(f))
				QMessageBox::warning(this, "Layar LED", "Berkas tidak valid.");
			else {
				led_->setLayout(led_->layout()); // simpan sebagai layout aktif
				rebuildTable();
			}
		}
	});
	connect(sm_, &SceneManager::scenesChanged, this, [this] { rebuildTable(); });

	rebuildTable();
}

LedScreenDialog::~LedScreenDialog()
{
	closeOutput();
	if (preview_)
		preview_->destroyDisplay();
}

QStringList LedScreenDialog::sceneChoices() const
{
	QStringList c;
	c << progLabel() << prevLabel();
	c << sm_->sceneNames();
	return c;
}

static QString sceneToLabel(const QString &s)
{
	if (s.isEmpty())
		return progLabel();
	if (s == "@preview")
		return prevLabel();
	return s;
}
static QString labelToScene(const QString &l)
{
	if (l == progLabel())
		return QString();
	if (l == prevLabel())
		return "@preview";
	return l;
}

void LedScreenDialog::rebuildTable()
{
	building_ = true;
	const LedLayout L = led_->layout();
	{
		QSignalBlocker a(w_), b(h_), c(test_);
		w_->setValue(L.width);
		h_->setValue(L.height);
		test_->setChecked(L.testPattern);
		int match = sizeCombo_->count() - 1;
		for (int i = 0; i < sizeCombo_->count(); ++i)
			if (sizeCombo_->itemData(i).toPoint() == QPoint(L.width, L.height))
				match = i;
		sizeCombo_->setCurrentIndex(match);
	}
	table_->setRowCount(0);
	const QStringList choices = sceneChoices();
	auto onChange = [this] {
		if (!building_)
			collectFromTable();
	};
	auto dspin = [&](double v, double lo, double hi, int dec) {
		auto *s = new QDoubleSpinBox(table_);
		s->setRange(lo, hi);
		s->setDecimals(dec);
		s->setValue(v);
		s->setButtonSymbols(QAbstractSpinBox::NoButtons);
		s->setKeyboardTracking(false);
		connect(s, &QDoubleSpinBox::valueChanged, this, onChange);
		return s;
	};
	for (const LedSlice &sl : L.slices) {
		const int r = table_->rowCount();
		table_->insertRow(r);
		auto *on = new QCheckBox(table_);
		on->setChecked(sl.enabled);
		connect(on, &QCheckBox::toggled, this, onChange);
		table_->setCellWidget(r, cOn, on);
		auto *name = new QLineEdit(sl.name, table_);
		connect(name, &QLineEdit::editingFinished, this, onChange);
		table_->setCellWidget(r, cName, name);
		auto *sc = new QComboBox(table_);
		sc->addItems(choices);
		const QString lab = sceneToLabel(sl.scene);
		if (sc->findText(lab) < 0)
			sc->addItem(lab);
		sc->setCurrentText(lab);
		connect(sc, &QComboBox::currentIndexChanged, this, onChange);
		table_->setCellWidget(r, cScene, sc);
		auto *fit = new QComboBox(table_);
		fit->addItems({"Regang", "Muat", "Isi"});
		fit->setCurrentIndex(sl.fit);
		connect(fit, &QComboBox::currentIndexChanged, this, onChange);
		table_->setCellWidget(r, cFit, fit);
		table_->setCellWidget(r, cIX, dspin(sl.inX * 100, 0, 100, 1));
		table_->setCellWidget(r, cIY, dspin(sl.inY * 100, 0, 100, 1));
		table_->setCellWidget(r, cIW, dspin(sl.inW * 100, 0.1, 100, 1));
		table_->setCellWidget(r, cIH, dspin(sl.inH * 100, 0.1, 100, 1));
		table_->setCellWidget(r, cOX, dspin(sl.outX, -16384, 16384, 0));
		table_->setCellWidget(r, cOY, dspin(sl.outY, -16384, 16384, 0));
		table_->setCellWidget(r, cOW, dspin(sl.outW, 1, 16384, 0));
		table_->setCellWidget(r, cOH, dspin(sl.outH, 1, 16384, 0));
		table_->setRowHeight(r, 28);
	}
	building_ = false;
}

void LedScreenDialog::collectFromTable()
{
	LedLayout L = led_->layout();
	L.slices.clear();
	for (int r = 0; r < table_->rowCount(); ++r) {
		auto get = [&](int c) { return static_cast<QDoubleSpinBox *>(table_->cellWidget(r, c))->value(); };
		LedSlice s;
		s.enabled = static_cast<QCheckBox *>(table_->cellWidget(r, cOn))->isChecked();
		s.name = static_cast<QLineEdit *>(table_->cellWidget(r, cName))->text();
		s.scene = labelToScene(static_cast<QComboBox *>(table_->cellWidget(r, cScene))->currentText());
		s.fit = static_cast<QComboBox *>(table_->cellWidget(r, cFit))->currentIndex();
		s.inX = get(cIX) / 100.0;
		s.inY = get(cIY) / 100.0;
		s.inW = get(cIW) / 100.0;
		s.inH = get(cIH) / 100.0;
		s.outX = get(cOX);
		s.outY = get(cOY);
		s.outW = get(cOW);
		s.outH = get(cOH);
		L.slices.append(s);
	}
	led_->setLayout(L);
}

void LedScreenDialog::applySize()
{
	// Ukuran baru: skala ulang posisi slice supaya tetap proporsional.
	LedLayout L = led_->layout();
	const double fx = double(w_->value()) / L.width, fy = double(h_->value()) / L.height;
	L.width = w_->value();
	L.height = h_->value();
	for (LedSlice &s : L.slices) {
		s.outX = std::round(s.outX * fx);
		s.outW = std::max(1.0, std::round(s.outW * fx));
		s.outY = std::round(s.outY * fy);
		s.outH = std::max(1.0, std::round(s.outH * fy));
	}
	led_->setLayout(L);
	rebuildTable();
	if (window_) // ukuran jendela keluaran mengikuti
		openOn(window_->screen());
}

void LedScreenDialog::applyPreset(int kind, int n)
{
	const LedLayout cur = led_->layout();
	const QStringList scenes = sm_->sceneNames();
	LedLayout L;
	switch (kind) {
	case 0: L = LedScreen::presetFull(cur.width, cur.height); break;
	case 1: L = LedScreen::presetColumns(cur.width, cur.height, n, scenes); break;
	case 2: L = LedScreen::presetGrid(cur.width, cur.height, n / 10, n % 10, scenes); break;
	default: L = LedScreen::presetSplitOne(cur.width, cur.height, n, QString()); break;
	}
	L.testPattern = cur.testPattern;
	L.name = cur.name;
	led_->setLayout(L);
	rebuildTable();
}

void LedScreenDialog::openOn(QScreen *screen)
{
	closeOutput();
	window_ = new LedScreenWindow(sm_, led_);
	window_->presentOn(screen);
	emit outputChanged();
}

void LedScreenDialog::closeOutput()
{
	if (window_)
		window_->close();
	window_.clear();
	emit outputChanged();
}

} // namespace ls
