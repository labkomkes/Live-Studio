#include "MainWindow.h"

#include "../core/Bumper.h"
#include "../core/LowerThird.h"
#include "../core/ObsEngine.h"
#include "../core/SceneManager.h"
#include "../core/SourceFactory.h"
#include "AudioMixerWidget.h"
#include "BumperPanel.h"
#include "DeckStrip.h"
#include "LowerThirdPanel.h"
#include "ObsDisplayWidget.h"
#include "ProjectorWindow.h"
#include "Render.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace ls {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle("Live Studio");
	resize(1500, 900);
	setMinimumSize(1100, 700);

	sm_ = new SceneManager(this);
	QString err;
	if (!sm_->init(&err))
		QMessageBox::critical(this, "Live Studio", "Gagal menyiapkan scene:\n" + err);
	lt_ = new LowerThird(sm_, this);
	lt_->ensureItems();
	bumper_ = new Bumper(sm_, this);

	buildSourceMenu();
	buildUi();
	buildMenus();
	connectSignals();

	QSettings s("LiveStudio", "LiveStudio");
	restoreGeometry(s.value("window/geometry").toByteArray());
	applyMode(s.value("ui/pro", false).toBool());

	refreshScenes();
	refreshItems();
	refreshOutputLabels();
	refreshTransitionControls();
	audio_->refresh();
	statusBar()->showMessage("Siap. Pilih scene lalu tambahkan sumber (kamera, layar, video, gambar, teks).", 8000);
}

MainWindow::~MainWindow()
{
	// Urutan penting: display -> mixer -> scene -> (engine dimatikan oleh main()).
	for (auto &p : projectors_)
		if (p)
			delete p.data();
	projectors_.clear();
	if (preview_)
		preview_->destroyDisplay();
	if (program_)
		program_->destroyDisplay();
	if (deckStrip_)
		deckStrip_->destroyDisplay();
	render::releaseGraphicsResources();
	if (audio_)
		audio_->clear();
	if (bumper_)
		bumper_->releaseAll();
	if (sm_)
		sm_->shutdown();
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	QSettings s("LiveStudio", "LiveStudio");
	s.setValue("window/geometry", saveGeometry());
	s.setValue("ui/pro", pro_);
	sm_->save();
	for (auto &p : projectors_)
		if (p)
			p->close();
	e->accept();
}

// ------------------------------------------------------------------ UI

QString MainWindow::editedScene() const
{
	return sm_->previewName();
}

QString MainWindow::friendlyType(const QString &id)
{
	if (id.startsWith("dshow_input"))
		return "Kamera";
	if (id.startsWith("monitor_capture") || id.startsWith("display_capture"))
		return "Layar";
	if (id.startsWith("window_capture"))
		return "Jendela";
	if (id.startsWith("ffmpeg_source"))
		return "Video";
	if (id.startsWith("image_source"))
		return "Gambar";
	if (id.startsWith("text_gdiplus"))
		return "Teks";
	if (id.startsWith("color_source"))
		return "Warna";
	if (id.startsWith("wasapi_input"))
		return "Mikrofon";
	if (id.startsWith("wasapi_output"))
		return "Audio Desktop";
	return id;
}

void MainWindow::buildSourceMenu()
{
	addSourceMenu_ = new QMenu("Tambah Sumber", this);
	struct Entry {
		const char *label;
		SourceKind kind;
	};
	const Entry entries[] = {
		{"Kamera / Webcam…", SourceKind::Webcam},   {"Layar (Monitor)…", SourceKind::Screen},
		{"Jendela Aplikasi…", SourceKind::Window},  {"File Video…", SourceKind::Video},
		{"Gambar…", SourceKind::Image},             {"Teks…", SourceKind::Text},
		{"Mikrofon…", SourceKind::Mic},             {"Audio Desktop (suara komputer)…", SourceKind::DesktopAudio},
	};
	for (const Entry &e : entries) {
		const SourceKind k = e.kind;
		addSourceMenu_->addAction(e.label, this, [this, k] { addSourceOfKind(k); });
	}
}

void MainWindow::buildUi()
{
	auto *central = new QWidget(this);
	auto *root = new QVBoxLayout(central);
	root->setContentsMargins(6, 6, 6, 6);
	root->setSpacing(6);

	auto makeBox = [&](const QString &title, ObsDisplayWidget *&display, QLabel *&label, const QString &obj) {
		auto *box = new QGroupBox(title, central);
		box->setObjectName(obj);
		auto *lay = new QVBoxLayout(box);
		lay->setContentsMargins(4, 4, 4, 4);
		lay->setSpacing(2);
		label = new QLabel("-", box);
		label->setObjectName("monitorName");
		label->setAlignment(Qt::AlignCenter);
		display = new ObsDisplayWidget(box);
		lay->addWidget(label);
		lay->addWidget(display, 1);
		return box;
	};

	// --- Baris 1 (gaya Studio Mode OBS): Preview | CUT/TAKE | Program ---
	QGroupBox *previewBox = makeBox("PREVIEW", preview_, previewLabel_, "previewBox");
	QGroupBox *programBox = makeBox("PROGRAM", program_, programLabel_, "programBox");

	auto *center = new QVBoxLayout();
	center->setSpacing(8);
	center->addStretch(1);
	cutBtn_ = new QPushButton("CUT", central);
	cutBtn_->setObjectName("cutButton");
	cutBtn_->setFixedSize(104, 54);
	cutBtn_->setToolTip("Preview -> Program seketika (Enter)");
	takeBtn_ = new QPushButton("TAKE", central);
	takeBtn_->setObjectName("takeButton");
	takeBtn_->setFixedSize(104, 70);
	takeBtn_->setToolTip("Preview -> Program dengan transisi (Spasi)");
	center->addWidget(cutBtn_);
	center->addWidget(takeBtn_);
	center->addStretch(1);

	auto *monitors = new QHBoxLayout();
	monitors->setSpacing(6);
	monitors->addWidget(previewBox, 1);
	monitors->addLayout(center, 0);
	monitors->addWidget(programBox, 1);
	root->addLayout(monitors, 1);

	// --- Baris 2: panel Adegan | Sumber | Audio | Lower Third/Bumper ---
	root->addWidget(buildPanels(central), 0);

	// --- Baris 3 (gaya vMix): deck ---
	root->addWidget(buildDeck(central), 0);

	setCentralWidget(central);
	preview_->setDrawFn([this](uint32_t cx, uint32_t cy) { render::drawPreview(sm_, cx, cy); });
	program_->setDrawFn([this](uint32_t cx, uint32_t cy) { render::drawProgram(sm_, cx, cy); });
}

QWidget *MainWindow::buildPanels(QWidget *parent)
{
	auto *row = new QWidget(parent);
	row->setFixedHeight(206);
	auto *lay = new QHBoxLayout(row);
	lay->setContentsMargins(0, 0, 0, 0);
	lay->setSpacing(6);

	auto makePanel = [row](const QString &title, QWidget *content) {
		auto *f = new QFrame(row);
		f->setObjectName("panel");
		auto *v = new QVBoxLayout(f);
		v->setContentsMargins(0, 0, 0, 0);
		v->setSpacing(0);
		auto *t = new QLabel(title, f);
		t->setObjectName("panelTitle");
		v->addWidget(t);
		v->addWidget(content, 1);
		return f;
	};
	auto miniBtn = [](const QString &text, const QString &tip, QWidget *p) {
		auto *b = new QPushButton(text, p);
		b->setObjectName("miniBtn");
		b->setToolTip(tip);
		return b;
	};

	// --- Adegan ---
	{
		auto *w = new QWidget(row);
		auto *v = new QVBoxLayout(w);
		v->setContentsMargins(4, 4, 4, 4);
		v->setSpacing(4);
		scenesList_ = new QListWidget(w);
		scenesList_->setToolTip("Klik = Preview, klik dua kali = CUT ke Program. Tombol 1–9 memilih adegan.");
		v->addWidget(scenesList_, 1);

		auto *btns = new QHBoxLayout();
		btns->setSpacing(4);
		auto *add = miniBtn("+", "Adegan baru", w);
		auto *del = miniBtn("−", "Hapus adegan", w);
		auto *ren = miniBtn("Nama", "Ganti nama adegan", w);
		auto *tpl = new QToolButton(w);
		tpl->setObjectName("miniBtn");
		tpl->setText("Templat ▾");
		tpl->setPopupMode(QToolButton::InstantPopup);
		auto *tplMenu = new QMenu(tpl);
		tplMenu->addAction("Segera Dimulai", this, [this] {
			sm_->createSlateScene("Segera Dimulai", "Webinar akan segera dimulai", QColor(20, 40, 90));
		});
		tplMenu->addAction("Segera Kembali (BRB)", this, [this] {
			sm_->createSlateScene("Segera Kembali", "Kami segera kembali", QColor(70, 40, 20));
		});
		tplMenu->addAction("Sesi Tanya Jawab", this, [this] {
			sm_->createSlateScene("Tanya Jawab", "Sesi tanya jawab", QColor(60, 30, 90));
		});
		tplMenu->addAction("Terima Kasih", this, [this] {
			sm_->createSlateScene("Terima Kasih", "Terima kasih telah bergabung", QColor(20, 70, 50));
		});
		tpl->setMenu(tplMenu);
		btns->addWidget(add);
		btns->addWidget(del);
		btns->addWidget(ren);
		btns->addWidget(tpl, 1);
		v->addLayout(btns);

		connect(add, &QPushButton::clicked, this, [this] {
			bool ok = false;
			const QString n = QInputDialog::getText(this, "Adegan baru", "Nama adegan:", QLineEdit::Normal,
								"Adegan", &ok);
			if (ok)
				sm_->createScene(n);
		});
		connect(del, &QPushButton::clicked, this, [this] {
			auto *it = scenesList_->currentItem();
			if (!it)
				return;
			const QString n = it->data(Qt::UserRole).toString();
			if (!sm_->removeScene(n))
				QMessageBox::information(this, "Adegan",
							 "Adegan Program yang sedang tayang atau satu-satunya adegan tidak bisa dihapus.");
		});
		connect(ren, &QPushButton::clicked, this, [this] {
			auto *it = scenesList_->currentItem();
			if (!it)
				return;
			const QString old = it->data(Qt::UserRole).toString();
			bool ok = false;
			const QString n =
				QInputDialog::getText(this, "Ganti nama", "Nama baru:", QLineEdit::Normal, old, &ok);
			if (ok && !sm_->renameScene(old, n))
				QMessageBox::information(this, "Adegan", "Nama tidak valid atau sudah dipakai.");
		});
		scenesPanel_ = makePanel("Adegan", w);
	}

	// --- Sumber ---
	{
		auto *w = new QWidget(row);
		auto *v = new QVBoxLayout(w);
		v->setContentsMargins(4, 4, 4, 4);
		v->setSpacing(4);
		sourcesList_ = new QListWidget(w);
		v->addWidget(sourcesList_, 1);

		auto *btns = new QHBoxLayout();
		btns->setSpacing(4);
		auto *addBtn = new QToolButton(w);
		addBtn->setObjectName("miniBtn");
		addBtn->setText("＋ Tambah ▾");
		addBtn->setPopupMode(QToolButton::InstantPopup);
		addBtn->setMenu(addSourceMenu_);
		auto *up = miniBtn("↑", "Naikkan lapisan", w);
		auto *down = miniBtn("↓", "Turunkan lapisan", w);
		auto *rm = miniBtn("Hapus", "Hapus sumber terpilih", w);
		btns->addWidget(addBtn, 1);
		btns->addWidget(up);
		btns->addWidget(down);
		btns->addWidget(rm);
		v->addLayout(btns);

		auto currentName = [this]() -> QString {
			auto *it = sourcesList_->currentItem();
			return it ? it->data(Qt::UserRole).toString() : QString();
		};
		connect(up, &QPushButton::clicked, this, [this, currentName] {
			const QString n = currentName();
			if (!n.isEmpty())
				sm_->moveItem(editedScene(), n, true);
		});
		connect(down, &QPushButton::clicked, this, [this, currentName] {
			const QString n = currentName();
			if (!n.isEmpty())
				sm_->moveItem(editedScene(), n, false);
		});
		connect(rm, &QPushButton::clicked, this, [this, currentName] {
			const QString n = currentName();
			if (!n.isEmpty()) {
				sm_->removeItem(editedScene(), n);
				audio_->refresh();
			}
		});
		connect(sourcesList_, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
			sm_->setItemVisible(editedScene(), it->data(Qt::UserRole).toString(),
					    it->checkState() == Qt::Checked);
		});
		sourcesPanel_ = makePanel("Sumber", w);
	}

	// --- Audio Mixer ---
	audio_ = new AudioMixerWidget(row);
	QFrame *audioPanel = makePanel("Audio Mixer", audio_);

	// --- Lower Third / Bumper (tab) ---
	toolsTabs_ = new QTabWidget(row);
	toolsTabs_->setObjectName("toolsTabs");
	auto scrolled = [](QWidget *inner) {
		auto *sa = new QScrollArea();
		sa->setWidgetResizable(true);
		sa->setFrameShape(QFrame::NoFrame);
		sa->setWidget(inner);
		return sa;
	};
	ltPanel_ = new LowerThirdPanel(lt_);
	bumperPanel_ = new BumperPanel(bumper_, sm_);
	toolsTabs_->addTab(scrolled(ltPanel_), "Lower Third");
	toolsTabs_->addTab(scrolled(bumperPanel_), "Bumper");

	lay->addWidget(scenesPanel_, 3);
	lay->addWidget(sourcesPanel_, 4);
	lay->addWidget(audioPanel, 5);
	lay->addWidget(toolsTabs_, 5);
	return row;
}

QWidget *MainWindow::buildDeck(QWidget *parent)
{
	auto *deck = new QFrame(parent);
	deck->setObjectName("deck");
	deck->setMinimumHeight(188);
	auto *h = new QHBoxLayout(deck);
	h->setContentsMargins(0, 0, 0, 0);
	h->setSpacing(0);

	deckStrip_ = new DeckStrip(sm_, deck);
	h->addWidget(deckStrip_, 1);

	auto *side = new QFrame(deck);
	side->setObjectName("deckSide");
	side->setFixedWidth(310);
	auto *g = new QVBoxLayout(side);
	g->setContentsMargins(10, 8, 10, 8);
	g->setSpacing(6);

	auto *heading = new QLabel("TRANSISI", side);
	heading->setObjectName("deckHeading");
	g->addWidget(heading);

	auto *grid = new QGridLayout();
	grid->setSpacing(4);
	transitionGroup_ = new QButtonGroup(this);
	transitionGroup_->setExclusive(true);
	int i = 0;
	for (const auto &p : SceneManager::availableTransitions()) {
		auto *b = new QToolButton(side);
		b->setObjectName("transBtn");
		b->setText(p.second == "fade_to_color_transition" ? QString("Warna") : p.first);
		b->setCheckable(true);
		b->setProperty("kind", p.second);
		b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		transitionGroup_->addButton(b);
		grid->addWidget(b, i / 3, i % 3);
		++i;
	}
	g->addLayout(grid);

	auto *dur = new QHBoxLayout();
	dur->addWidget(new QLabel("Durasi", side));
	durationSpin_ = new QSpinBox(side);
	durationSpin_->setRange(0, 5000);
	durationSpin_->setSingleStep(100);
	durationSpin_->setSuffix(" ms");
	durationSpin_->setValue(sm_->transitionMs());
	dur->addWidget(durationSpin_, 1);
	g->addLayout(dur);

	swapCheck_ = new QCheckBox("Tukar Preview setelah TAKE", side);
	swapCheck_->setChecked(sm_->swapAfterTake());
	g->addWidget(swapCheck_);
	g->addStretch(1);

	auto *btns = new QHBoxLayout();
	btns->setSpacing(4);
	ltQuickBtn_ = new QPushButton("Lower Third", side);
	ltQuickBtn_->setObjectName("ltQuick");
	ltQuickBtn_->setCheckable(true);
	ltQuickBtn_->setToolTip("Tampilkan / sembunyikan lower third (isi di tab Lower Third)");
	auto *addBtn = new QToolButton(side);
	addBtn->setObjectName("deckTool");
	addBtn->setText("＋ Sumber ▾");
	addBtn->setPopupMode(QToolButton::InstantPopup);
	addBtn->setMenu(addSourceMenu_);
	addBtn->setToolTip("Tambah kamera, layar, jendela, video, gambar, teks, atau audio");
	outputBtn_ = new QToolButton(side);
	outputBtn_->setObjectName("deckTool");
	outputBtn_->setText("Output ▾");
	outputBtn_->setPopupMode(QToolButton::InstantPopup);
	outputBtn_->setToolTip("Kirim Program / Preview / Multiview ke monitor lain");
	btns->addWidget(ltQuickBtn_, 1);
	btns->addWidget(addBtn, 1);
	btns->addWidget(outputBtn_, 1);
	g->addLayout(btns);

	h->addWidget(side, 0);
	return deck;
}

void MainWindow::buildMenus()
{
	QMenu *file = menuBar()->addMenu("&File");
	file->addAction("Simpan koleksi scene", this, [this] {
		statusBar()->showMessage(sm_->save() ? "Koleksi scene disimpan." : "Gagal menyimpan koleksi.", 4000);
	});
	file->addAction("Pengaturan video…", this, &MainWindow::chooseVideoSettings);
	file->addSeparator();
	file->addAction("Keluar", this, &QWidget::close);

	QMenu *view = menuBar()->addMenu("&Tampilan");
	modeAction_ = view->addAction("Mode Profesional");
	modeAction_->setCheckable(true);
	connect(modeAction_, &QAction::toggled, this, [this](bool on) { applyMode(on); });
	view->addSeparator();
	programOutMenu_ = view->addMenu("Program Output ke monitor");
	previewOutMenu_ = view->addMenu("Preview Output ke monitor");

	multiviewMenu_ = menuBar()->addMenu("&Multiview");
	fillScreenMenu(multiviewMenu_, int(ProjectorWindow::Mode::Multiview));
	fillScreenMenu(programOutMenu_, int(ProjectorWindow::Mode::Program));
	fillScreenMenu(previewOutMenu_, int(ProjectorWindow::Mode::Preview));
	// Daftar monitor dibaca ulang setiap menu dibuka (monitor bisa dicolok/dicabut).
	connect(multiviewMenu_, &QMenu::aboutToShow, this,
		[this] { fillScreenMenu(multiviewMenu_, int(ProjectorWindow::Mode::Multiview)); });
	connect(programOutMenu_, &QMenu::aboutToShow, this,
		[this] { fillScreenMenu(programOutMenu_, int(ProjectorWindow::Mode::Program)); });
	connect(previewOutMenu_, &QMenu::aboutToShow, this,
		[this] { fillScreenMenu(previewOutMenu_, int(ProjectorWindow::Mode::Preview)); });

	// Tombol "Output ▾" di deck memakai menu yang sama.
	auto *outMenu = new QMenu(outputBtn_);
	outMenu->addMenu(programOutMenu_);
	outMenu->addMenu(previewOutMenu_);
	outMenu->addMenu(multiviewMenu_);
	outputBtn_->setMenu(outMenu);

	QMenu *help = menuBar()->addMenu("&Bantuan");
	help->addAction("Tentang Live Studio", this, &MainWindow::showAbout);
}

void MainWindow::fillScreenMenu(QMenu *menu, int mode)
{
	menu->clear();
	const auto screens = QGuiApplication::screens();
	int i = 1;
	for (QScreen *sc : screens) {
		const QString label = QString("Monitor %1 — %2 (%3×%4)%5")
					      .arg(i++)
					      .arg(sc->name())
					      .arg(sc->geometry().width())
					      .arg(sc->geometry().height())
					      .arg(sc == QGuiApplication::primaryScreen() ? " [utama]" : "");
		QPointer<QScreen> guard(sc);
		menu->addAction(label, this, [this, mode, guard] {
			if (guard)
				openProjector(mode, guard);
		});
	}
	menu->addSeparator();
	menu->addAction("Jendela biasa (tidak fullscreen)", this, [this, mode] { openProjector(mode, nullptr); });
}

void MainWindow::openProjector(int mode, QScreen *screen)
{
	auto *w = new ProjectorWindow(sm_, ProjectorWindow::Mode(mode));
	projectors_.append(QPointer<ProjectorWindow>(w));
	w->presentOn(screen);
	statusBar()->showMessage("Tekan Esc pada jendela proyektor untuk menutup.", 5000);
}

void MainWindow::connectSignals()
{
	connect(cutBtn_, &QPushButton::clicked, sm_, &SceneManager::cut);
	connect(takeBtn_, &QPushButton::clicked, sm_, &SceneManager::take);
	connect(swapCheck_, &QCheckBox::toggled, this, [this](bool on) { sm_->setSwapAfterTake(on); });

	connect(transitionGroup_, &QButtonGroup::buttonToggled, this, [this](QAbstractButton *b, bool on) {
		if (on)
			sm_->setTransition(b->property("kind").toString(), durationSpin_->value());
	});
	connect(durationSpin_, &QSpinBox::valueChanged, this,
		[this](int ms) { sm_->setTransition(QString(), ms); });

	// Deck ala vMix.
	connect(deckStrip_, &DeckStrip::previewRequested, this, [this](const QString &n) { sm_->setPreview(n); });
	connect(deckStrip_, &DeckStrip::cutRequested, this, [this](const QString &n) { sm_->sendToProgram(n, false); });
	connect(deckStrip_, &DeckStrip::takeRequested, this, [this](const QString &n) { sm_->sendToProgram(n, true); });
	connect(ltQuickBtn_, &QPushButton::toggled, this, [this](bool on) {
		if (on)
			lt_->show();
		else
			lt_->hide();
	});
	connect(lt_, &LowerThird::visibilityChanged, this, [this](bool shown) {
		QSignalBlocker b(ltQuickBtn_);
		ltQuickBtn_->setChecked(shown);
	});

	connect(sm_, &SceneManager::scenesChanged, this, [this] {
		render::clearLabelCache();
		refreshScenes();
		bumperPanel_->refreshScenes();
		audio_->refresh();
	});
	connect(sm_, &SceneManager::previewChanged, this, [this] {
		refreshScenes();
		refreshItems();
		refreshOutputLabels();
	});
	connect(sm_, &SceneManager::programChanged, this, [this] {
		refreshScenes();
		refreshOutputLabels();
	});
	connect(sm_, &SceneManager::itemsChanged, this, [this](const QString &scene) {
		if (scene == sm_->previewName())
			refreshItems();
		audio_->refresh(); // sumber audio baru/dihapus (dari mana pun asalnya)
	});

	connect(scenesList_, &QListWidget::itemClicked, this,
		[this](QListWidgetItem *it) { sm_->setPreview(it->data(Qt::UserRole).toString()); });
	connect(scenesList_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
		sm_->setPreview(it->data(Qt::UserRole).toString());
		sm_->cut();
	});

	// Pintasan keyboard. Diabaikan saat operator sedang mengetik di kolom isian
	// (mis. nama lower third) agar tidak terjadi CUT/TAKE tak disengaja saat live.
	auto typing = [] {
		QWidget *w = QApplication::focusWidget();
		return qobject_cast<QLineEdit *>(w) || qobject_cast<QAbstractSpinBox *>(w) ||
		       qobject_cast<QPlainTextEdit *>(w) || qobject_cast<QTextEdit *>(w) ||
		       (qobject_cast<QComboBox *>(w) && static_cast<QComboBox *>(w)->isEditable());
	};
	auto *takeKey = new QShortcut(QKeySequence(Qt::Key_Space), this);
	connect(takeKey, &QShortcut::activated, this, [this, typing] {
		if (!typing())
			sm_->take();
	});
	auto *cutKey = new QShortcut(QKeySequence(Qt::Key_Return), this);
	connect(cutKey, &QShortcut::activated, this, [this, typing] {
		if (!typing())
			sm_->cut();
	});
	auto *cutKey2 = new QShortcut(QKeySequence(Qt::Key_Enter), this);
	connect(cutKey2, &QShortcut::activated, this, [this, typing] {
		if (!typing())
			sm_->cut();
	});
	for (int i = 1; i <= 9; ++i) {
		auto *sc = new QShortcut(QKeySequence(Qt::Key_0 + i), this);
		connect(sc, &QShortcut::activated, this, [this, i, typing] {
			if (typing())
				return;
			const QStringList names = sm_->sceneNames();
			if (i - 1 < names.size())
				sm_->setPreview(names.at(i - 1));
		});
	}
}

// ------------------------------------------------------------------ refresh

void MainWindow::refreshScenes()
{
	QSignalBlocker block(scenesList_);
	const QString selected = scenesList_->currentItem() ? scenesList_->currentItem()->data(Qt::UserRole).toString()
							     : QString();
	scenesList_->clear();
	int i = 0;
	for (const QString &name : sm_->sceneNames()) {
		auto *it = new QListWidgetItem(QString("%1   %2").arg(++i).arg(name));
		it->setData(Qt::UserRole, name);
		if (name == sm_->programName()) {
			it->setText(it->text() + "     ● PROGRAM");
			it->setBackground(QColor(150, 30, 30));
			it->setForeground(Qt::white);
		} else if (name == sm_->previewName()) {
			it->setText(it->text() + "     ● PREVIEW");
			it->setBackground(QColor(30, 120, 40));
			it->setForeground(Qt::white);
		}
		scenesList_->addItem(it);
		if (name == selected)
			scenesList_->setCurrentItem(it);
	}
}

void MainWindow::refreshItems()
{
	QSignalBlocker block(sourcesList_);
	sourcesList_->clear();
	for (const auto &info : sm_->items(editedScene())) {
		auto *it = new QListWidgetItem(QString("%1   [%2]").arg(info.name, friendlyType(info.typeId)));
		it->setData(Qt::UserRole, info.name);
		it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
		it->setCheckState(info.visible ? Qt::Checked : Qt::Unchecked);
		sourcesList_->addItem(it);
	}
	if (auto *t = sourcesPanel_->findChild<QLabel *>("panelTitle"))
		t->setText(QString("Sumber — %1").arg(editedScene()));
}

void MainWindow::refreshOutputLabels()
{
	previewLabel_->setText(sm_->previewName());
	programLabel_->setText(sm_->programName().startsWith("LS ") ? "BUMPER" : sm_->programName());
}

void MainWindow::refreshTransitionControls()
{
	QSignalBlocker block(transitionGroup_);
	bool any = false;
	for (auto *btn : transitionGroup_->buttons()) {
		const bool on = btn->property("kind").toString() == sm_->transitionKind();
		btn->setChecked(on);
		any = any || on;
	}
	if (!any && !transitionGroup_->buttons().isEmpty())
		transitionGroup_->buttons().first()->setChecked(true);
}

// ------------------------------------------------------------------ mode

void MainWindow::applyMode(bool pro)
{
	pro_ = pro;
	// Mode Sederhana: monitor + deck + audio + lower third. Mode Profesional: tambah daftar Adegan, Sumber, Bumper, Multiview.
	scenesPanel_->setVisible(pro);
	sourcesPanel_->setVisible(pro);
	toolsTabs_->setTabVisible(1, pro);
	if (!pro)
		toolsTabs_->setCurrentIndex(0);
	multiviewMenu_->menuAction()->setVisible(pro);

	QSignalBlocker block(modeAction_);
	modeAction_->setChecked(pro);
	modeAction_->setText(pro ? "Mode Profesional ✓" : "Mode Profesional");
	statusBar()->showMessage(pro ? "Mode Profesional: daftar Adegan, Sumber, Bumper, dan Multiview ditampilkan."
				      : "Mode Sederhana.",
				 3000);
}

// ------------------------------------------------------------------ tambah sumber

void MainWindow::addSourceOfKind(SourceKind kind)
{
	const QString scene = editedScene();
	if (scene.isEmpty())
		return;

	auto pickFromList = [this](const QString &title, const QString &label,
				   const ObsEngine::ListItems &items) -> int {
		if (items.isEmpty())
			return -1;
		QStringList labels;
		for (const auto &p : items)
			labels << p.first;
		bool ok = false;
		const QString chosen = QInputDialog::getItem(this, title, label, labels, 0, false, &ok);
		return ok ? int(labels.indexOf(chosen)) : -2; // -2 = dibatalkan
	};

	QString created;
	switch (kind) {
	case SourceKind::Webcam: {
		if (!ObsEngine::isAvailable(factory::webcamId())) {
			QMessageBox::warning(this, "Kamera", "Plugin win-dshow tidak termuat. Periksa folder obs-plugins.");
			return;
		}
		const auto cams = factory::webcams();
		const int i = pickFromList("Kamera", "Pilih kamera:", cams);
		if (i == -1) {
			QMessageBox::information(this, "Kamera",
						 "Tidak ada kamera terdeteksi. Pastikan kamera terpasang dan tidak dipakai aplikasi lain.");
			return;
		}
		if (i < 0)
			return;
		bool ok = false;
		const QStringList res = {"Otomatis (bawaan perangkat)", "1280x720", "1920x1080", "640x480"};
		const QString r = QInputDialog::getItem(this, "Resolusi kamera", "Resolusi:", res, 1, false, &ok);
		if (!ok)
			return;
		Data d = factory::webcamSettings(cams.at(i).second, r.startsWith("Otomatis") ? QString() : r);
		created = sm_->addSource(scene, factory::webcamId(), "Kamera: " + cams.at(i).first, d.get(), true);
		break;
	}
	case SourceKind::Screen: {
		const auto mons = factory::monitors();
		const int i = pickFromList("Layar", "Pilih monitor yang ditangkap:", mons);
		if (i == -1) {
			QMessageBox::information(this, "Layar", "Tidak ada monitor terdeteksi oleh plugin capture.");
			return;
		}
		if (i < 0)
			return;
		Data d = factory::monitorSettings(mons.at(i).second, true);
		created = sm_->addSource(scene, factory::monitorId(), "Layar: " + mons.at(i).first, d.get(), true);
		break;
	}
	case SourceKind::Window: {
		const auto wins = factory::windows();
		const int i = pickFromList("Jendela", "Pilih jendela:", wins);
		if (i == -1) {
			QMessageBox::information(this, "Jendela", "Tidak ada jendela yang bisa ditangkap.");
			return;
		}
		if (i < 0)
			return;
		Data d = factory::windowSettings(wins.at(i).second);
		created = sm_->addSource(scene, factory::windowId(), "Jendela: " + wins.at(i).first, d.get(), true);
		break;
	}
	case SourceKind::Video: {
		const QString f = QFileDialog::getOpenFileName(this, "Pilih video", QString(),
							       "Video (*.mp4 *.mov *.mkv *.webm *.avi);;Semua file (*)");
		if (f.isEmpty())
			return;
		Data d = factory::mediaSettings(f, false);
		created = sm_->addSource(scene, factory::mediaId(), QFileInfo(f).completeBaseName(), d.get(), true);
		break;
	}
	case SourceKind::Image: {
		const QString f = QFileDialog::getOpenFileName(this, "Pilih gambar", QString(),
							       "Gambar (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;Semua file (*)");
		if (f.isEmpty())
			return;
		Data d = factory::imageSettings(f);
		created = sm_->addSource(scene, factory::imageId(), QFileInfo(f).completeBaseName(), d.get(), true);
		break;
	}
	case SourceKind::Text: {
		bool ok = false;
		const QString t = QInputDialog::getMultiLineText(this, "Teks", "Isi teks:", "Judul Sesi", &ok);
		if (!ok || t.trimmed().isEmpty())
			return;
		Data d = factory::textSettings(t, 72, QColor(255, 255, 255), true);
		created = sm_->addSource(scene, factory::textId(), "Teks: " + t.left(20), d.get(), false);
		break;
	}
	case SourceKind::Mic: {
		const auto devs = factory::micDevices();
		const int i = pickFromList("Mikrofon", "Pilih perangkat:", devs);
		if (i < 0)
			return;
		Data d = factory::audioDeviceSettings(devs.at(i).second);
		created = sm_->addSource(scene, factory::micId(), "Mic: " + devs.at(i).first, d.get(), false);
		break;
	}
	case SourceKind::DesktopAudio: {
		const auto devs = factory::desktopAudioDevices();
		const int i = pickFromList("Audio Desktop", "Pilih perangkat keluaran:", devs);
		if (i < 0)
			return;
		Data d = factory::audioDeviceSettings(devs.at(i).second);
		created = sm_->addSource(scene, factory::desktopAudioId(), "Desktop: " + devs.at(i).first, d.get(), false);
		break;
	}
	}

	if (created.isEmpty()) {
		QMessageBox::warning(this, "Sumber", "Sumber gagal dibuat. Lihat log di %APPDATA%\\LiveStudio\\logs.");
		return;
	}
	audio_->refresh();
	statusBar()->showMessage("Sumber ditambahkan ke scene Preview: " + created, 5000);
}

// ------------------------------------------------------------------ dialog lain

void MainWindow::chooseVideoSettings()
{
	const QStringList opts = {"1920x1080 @ 30", "1920x1080 @ 60", "1280x720 @ 30", "1280x720 @ 60"};
	bool ok = false;
	const QString c = QInputDialog::getItem(this, "Pengaturan video",
						"Kanvas dan frame rate (berlaku setelah aplikasi dibuka ulang):", opts, 0,
						false, &ok);
	if (!ok)
		return;
	QSettings s("LiveStudio", "LiveStudio");
	s.setValue("video/preset", c);
	QMessageBox::information(this, "Pengaturan video", "Tersimpan. Tutup dan buka ulang Live Studio.");
}

void MainWindow::showAbout()
{
	QMessageBox::about(this, "Tentang Live Studio",
			   "<b>Live Studio</b> " + QApplication::applicationVersion() + " (pra-rilis)<br>Produksi live untuk webinar, kuliah, seminar, "
			   "dan konferensi, berbasis libobs.<br><br>Status: kode belum diverifikasi lewat build/uji "
			   "di perangkat nyata. Lihat docs/STATUS.md dan docs/TEST_PLAN.md.<br><br>"
			   "Menggunakan libobs (GPLv2).");
}

} // namespace ls
