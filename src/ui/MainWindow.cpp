#include "MainWindow.h"

#include "../core/Bumper.h"
#include "../core/LowerThird.h"
#include "../core/ObsEngine.h"
#include "../core/SceneManager.h"
#include "../core/SourceFactory.h"
#include "AudioMixerWidget.h"
#include "BumperPanel.h"
#include "LowerThirdPanel.h"
#include "ObsDisplayWidget.h"
#include "ProjectorWindow.h"
#include "Render.h"

#include <QAbstractSpinBox>
#include <QAction>
#include <QApplication>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace ls {

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle("Live Studio");
	resize(1500, 900);

	sm_ = new SceneManager(this);
	QString err;
	if (!sm_->init(&err))
		QMessageBox::critical(this, "Live Studio", "Gagal menyiapkan scene:\n" + err);
	lt_ = new LowerThird(sm_, this);
	lt_->ensureItems();
	bumper_ = new Bumper(sm_, this);

	buildUi();
	buildDocks();
	buildMenusAndToolbar();
	connectSignals();

	QSettings s("LiveStudio", "LiveStudio");
	restoreGeometry(s.value("window/geometry").toByteArray());
	restoreState(s.value("window/state").toByteArray());
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
	s.setValue("window/state", saveState());
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

void MainWindow::buildUi()
{
	auto *central = new QWidget(this);
	auto *root = new QHBoxLayout(central);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	auto makeBox = [&](const QString &title, ObsDisplayWidget *&display, QLabel *&label, const QString &obj) {
		auto *box = new QGroupBox(title, central);
		box->setObjectName(obj);
		auto *lay = new QVBoxLayout(box);
		label = new QLabel("-", box);
		label->setAlignment(Qt::AlignCenter);
		display = new ObsDisplayWidget(box);
		lay->addWidget(label);
		lay->addWidget(display, 1);
		return box;
	};

	QGroupBox *previewBox = makeBox("PREVIEW", preview_, previewLabel_, "previewBox");
	QGroupBox *programBox = makeBox("PROGRAM", program_, programLabel_, "programBox");

	auto *center = new QVBoxLayout();
	center->addStretch(1);
	cutBtn_ = new QPushButton("CUT", central);
	cutBtn_->setObjectName("cutButton");
	cutBtn_->setMinimumSize(110, 56);
	cutBtn_->setToolTip("Preview -> Program seketika (Enter)");
	takeBtn_ = new QPushButton("TAKE", central);
	takeBtn_->setObjectName("takeButton");
	takeBtn_->setMinimumSize(110, 72);
	takeBtn_->setToolTip("Preview -> Program dengan transisi (Spasi)");
	transitionCombo_ = new QComboBox(central);
	durationSpin_ = new QSpinBox(central);
	durationSpin_->setRange(0, 5000);
	durationSpin_->setSingleStep(100);
	durationSpin_->setSuffix(" ms");
	durationSpin_->setValue(sm_->transitionMs());
	swapCheck_ = new QCheckBox("Tukar Preview\nsetelah TAKE", central);
	swapCheck_->setChecked(sm_->swapAfterTake());

	center->addWidget(cutBtn_);
	center->addWidget(takeBtn_);
	center->addSpacing(12);
	center->addWidget(new QLabel("Transisi", central));
	center->addWidget(transitionCombo_);
	center->addWidget(durationSpin_);
	center->addWidget(swapCheck_);
	center->addStretch(1);

	root->addWidget(previewBox, 5);
	root->addLayout(center, 0);
	root->addWidget(programBox, 5);
	setCentralWidget(central);

	preview_->setDrawFn([this](uint32_t cx, uint32_t cy) { render::drawPreview(sm_, cx, cy); });
	program_->setDrawFn([this](uint32_t cx, uint32_t cy) { render::drawProgram(sm_, cx, cy); });
}

void MainWindow::buildDocks()
{
	// --- Scenes ---
	scenesDock_ = new QDockWidget("Adegan (Scenes)", this);
	scenesDock_->setObjectName("scenesDock");
	{
		auto *w = new QWidget(scenesDock_);
		auto *lay = new QVBoxLayout(w);
		scenesList_ = new QListWidget(w);
		scenesList_->setToolTip("Klik = Preview, klik dua kali = CUT ke Program. Tombol 1–9 memilih scene.");
		lay->addWidget(scenesList_, 1);

		auto *row = new QHBoxLayout();
		auto *add = new QPushButton("+", w);
		auto *del = new QPushButton("−", w);
		auto *ren = new QPushButton("Ganti nama", w);
		auto *tpl = new QToolButton(w);
		tpl->setText("Template ▾");
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
		row->addWidget(add);
		row->addWidget(del);
		row->addWidget(ren, 1);
		row->addWidget(tpl);
		lay->addLayout(row);
		scenesDock_->setWidget(w);

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
	}
	addDockWidget(Qt::LeftDockWidgetArea, scenesDock_);

	// --- Sources ---
	sourcesDock_ = new QDockWidget("Sumber (scene Preview)", this);
	sourcesDock_->setObjectName("sourcesDock");
	{
		auto *w = new QWidget(sourcesDock_);
		auto *lay = new QVBoxLayout(w);
		sourcesList_ = new QListWidget(w);
		lay->addWidget(sourcesList_, 1);

		auto *row = new QHBoxLayout();
		auto *addBtn = new QToolButton(w);
		addBtn->setText("Tambah ▾");
		addBtn->setPopupMode(QToolButton::InstantPopup);
		addBtn->setMenu(new QMenu(addBtn)); // diisi di buildMenusAndToolbar()
		auto *up = new QPushButton("↑", w);
		auto *down = new QPushButton("↓", w);
		auto *rm = new QPushButton("Hapus", w);
		row->addWidget(addBtn);
		row->addWidget(up);
		row->addWidget(down);
		row->addWidget(rm, 1);
		lay->addLayout(row);
		sourcesDock_->setWidget(w);
		sourcesDock_->setProperty("addButton", QVariant::fromValue<QObject *>(addBtn));

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
	}
	addDockWidget(Qt::LeftDockWidgetArea, sourcesDock_);

	// --- Audio mixer ---
	audioDock_ = new QDockWidget("Audio Mixer", this);
	audioDock_->setObjectName("audioDock");
	audio_ = new AudioMixerWidget(audioDock_);
	audioDock_->setWidget(audio_);
	addDockWidget(Qt::RightDockWidgetArea, audioDock_);

	// --- Lower third ---
	ltDock_ = new QDockWidget("Lower Third", this);
	ltDock_->setObjectName("ltDock");
	ltPanel_ = new LowerThirdPanel(lt_, ltDock_);
	ltDock_->setWidget(ltPanel_);
	addDockWidget(Qt::RightDockWidgetArea, ltDock_);

	// --- Bumper ---
	bumperDock_ = new QDockWidget("Bumper Webinar", this);
	bumperDock_->setObjectName("bumperDock");
	bumperPanel_ = new BumperPanel(bumper_, sm_, bumperDock_);
	bumperDock_->setWidget(bumperPanel_);
	addDockWidget(Qt::BottomDockWidgetArea, bumperDock_);
}

void MainWindow::buildMenusAndToolbar()
{
	// ---- menu "Sumber" (dipakai toolbar + dock Sumber) ----
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
	if (auto *btn = qobject_cast<QToolButton *>(sourcesDock_->property("addButton").value<QObject *>()))
		btn->setMenu(addSourceMenu_);

	// ---- menu bar ----
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
	view->addAction(scenesDock_->toggleViewAction());
	view->addAction(sourcesDock_->toggleViewAction());
	view->addAction(audioDock_->toggleViewAction());
	view->addAction(ltDock_->toggleViewAction());
	view->addAction(bumperDock_->toggleViewAction());

	multiviewMenu_ = menuBar()->addMenu("&Multiview");
	programOutMenu_ = view->addMenu("Program Output ke monitor");
	previewOutMenu_ = view->addMenu("Preview Output ke monitor");
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

	QMenu *help = menuBar()->addMenu("&Bantuan");
	help->addAction("Tentang Live Studio", this, &MainWindow::showAbout);

	// ---- toolbar ----
	QToolBar *tb = addToolBar("Utama");
	tb->setObjectName("mainToolbar");
	tb->setMovable(false);
	auto *addTool = new QToolButton(tb);
	addTool->setText("＋ Tambah Sumber");
	addTool->setPopupMode(QToolButton::InstantPopup);
	addTool->setMenu(addSourceMenu_);
	tb->addWidget(addTool);
	tb->addSeparator();
	tb->addAction(modeAction_);
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

	auto applyTransition = [this] {
		sm_->setTransition(transitionCombo_->currentData().toString(), durationSpin_->value());
	};
	connect(transitionCombo_, &QComboBox::currentIndexChanged, this, applyTransition);
	connect(durationSpin_, &QSpinBox::valueChanged, this, applyTransition);

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
	sourcesDock_->setWindowTitle(QString("Sumber — %1").arg(editedScene()));
}

void MainWindow::refreshOutputLabels()
{
	previewLabel_->setText(sm_->previewName());
	programLabel_->setText(sm_->programName().startsWith("LS ") ? "BUMPER" : sm_->programName());
}

void MainWindow::refreshTransitionControls()
{
	QSignalBlocker b1(transitionCombo_);
	transitionCombo_->clear();
	for (const auto &p : SceneManager::availableTransitions())
		transitionCombo_->addItem(p.first, p.second);
	const int idx = transitionCombo_->findData(sm_->transitionKind());
	transitionCombo_->setCurrentIndex(idx >= 0 ? idx : 0);
}

// ------------------------------------------------------------------ mode

void MainWindow::applyMode(bool pro)
{
	pro_ = pro;
	sourcesDock_->setVisible(pro);
	bumperDock_->setVisible(pro);
	scenesDock_->setVisible(true);
	audioDock_->setVisible(true);
	ltDock_->setVisible(true);
	multiviewMenu_->menuAction()->setVisible(pro);

	QSignalBlocker block(modeAction_);
	modeAction_->setChecked(pro);
	modeAction_->setText(pro ? "Mode Profesional ✓" : "Mode Sederhana");
	statusBar()->showMessage(pro ? "Mode Profesional: semua panel + Multiview." : "Mode Sederhana.", 3000);
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
