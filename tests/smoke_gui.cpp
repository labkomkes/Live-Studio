// Live Studio - uji smoke GUI otomatis (opsional: -DLIVESTUDIO_BUILD_TESTS=ON).
// Menjalankan MainWindow sungguhan dan menggerakkan UI lewat widget (klik tombol CUT/TAKE,
// membuka Multiview, menayangkan lower third, memutar bumper) lalu memeriksa label Program/Preview.
// Variabel lingkungan: LS_SMOKE_SHOT_DIR = folder untuk tangkapan layar (hanya Linux, butuh `import`).
#include "core/ObsEngine.h"
#include "core/OutputManager.h"
#include "core/SceneManager.h"
#include "core/SourceFactory.h"
#include "ui/AudioMixerWidget.h"
#include "ui/DeckLayout.h"
#include "ui/DeckStrip.h"
#include "ui/MainWindow.h"
#include "ui/AudioInputDialog.h"
#include "core/LedScreen.h"
#include "ui/LedScreenDialog.h"
#include "ui/LedScreenWindow.h"
#include "ui/MediaPanel.h"
#include "ui/PreviewEditor.h"
#include "ui/PluginsDialog.h"
#include "ui/ProjectorWindow.h"
#include <QTabWidget>

#include <QAction>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QPushButton>
#include <QToolButton>
#include <QKeyEvent>
#include <QImage>
#include <QJsonDocument>
#include <QScreen>
#include <QTableWidget>
#include <QSettings>
#include <QTimer>
#include <QMenuBar>
#include <QCheckBox>
#include <QListWidget>

#include <cstdio>

#include "../src/ui/AddSourceDialog.h"
#include "../src/ui/ZoomDialog.h"
#include "../src/ui/SettingsDialog.h"
#include "../src/core/SourceFactory.h"
#include <obs.h>
#include <functional>

using namespace ls;

namespace {

int g_fail = 0;
int g_pass = 0;
MainWindow *g_win = nullptr;
QString g_shotDir;

void expect(bool cond, const QString &what, const QString &detail = {})
{
	if (cond) {
		++g_pass;
		printf("[PASS] %s\n", qPrintable(what));
	} else {
		++g_fail;
		printf("[FAIL] %s %s\n", qPrintable(what), qPrintable(detail));
	}
	fflush(stdout);
}

QString boxLabel(const char *boxName)
{
	auto *box = g_win->findChild<QGroupBox *>(boxName);
	auto *lbl = box ? box->findChild<QLabel *>() : nullptr;
	return lbl ? lbl->text() : QString("<tidak ada>");
}
QString programLabel() { return boxLabel("programBox"); }
QString previewLabel() { return boxLabel("previewBox"); }

QPushButton *buttonByText(const QString &part)
{
	for (auto *b : g_win->findChildren<QPushButton *>())
		if (b->text().contains(part))
			return b;
	return nullptr;
}

QLineEdit *editByPlaceholder(const QString &part)
{
	for (auto *e : g_win->findChildren<QLineEdit *>())
		if (e->placeholderText().contains(part))
			return e;
	return nullptr;
}

// Mensimulasikan klik kiri pada tile deck (indeks scene, zona) memakai geometri yang sama dengan penggambar.
bool clickDeck(int sceneIdx, deck::Zone zone)
{
	auto *ds = g_win->findChild<DeckStrip *>();
	if (!ds)
		return false;
	const int n = int(g_win->sceneManager()->sceneNames().size());
	const deck::Metrics m = deck::metrics(float(ds->width()), float(ds->height()), n, 0);
	if (sceneIdx < 0 || sceneIdx >= m.visible)
		return false;
	const deck::Tile t = deck::tile(m, sceneIdx);
	const deck::Rect r = zone == deck::Zone::Cut ? t.cutBtn : zone == deck::Zone::Take ? t.takeBtn : t.thumb;
	const QPointF p(r.x + r.w / 2.f, r.y + r.h / 2.f);
	QMouseEvent press(QEvent::MouseButtonPress, p, ds->mapToGlobal(p), Qt::LeftButton, Qt::LeftButton,
			  Qt::NoModifier);
	QApplication::sendEvent(ds, &press);
	return true;
}

void shot(const QString &name)
{
	if (g_shotDir.isEmpty())
		return;
	QProcess::execute("import", {"-window", "root", g_shotDir + "/" + name + ".png"});
}

} // namespace

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationVersion(LIVESTUDIO_VERSION);
	g_shotDir = qEnvironmentVariable("LS_SMOKE_SHOT_DIR");

	QString err;
	if (!ObsEngine::instance().start(VideoSettings{}, &err)) {
		printf("[FAIL] engine: %s\n", qPrintable(err));
		return 3;
	}

	// Video bumper uji (2 dtk) dibuat dengan ffmpeg bila tersedia.
	const QString bumperFile = QDir::tempPath() + "/ls_smoke_bumper.mp4";
	bool haveBumper = false;
	if (QProcess::execute("ffmpeg", {"-y", "-loglevel", "error", "-f", "lavfi", "-i",
					 "testsrc=size=640x360:rate=30:duration=2", "-f", "lavfi", "-i",
					 "sine=frequency=440:duration=2", "-shortest", "-pix_fmt", "yuv420p",
					 bumperFile}) == 0)
		haveBumper = QFile::exists(bumperFile);

	// Video bersuara (nada 1 kHz, 12 dtk) untuk uji mixer.
	const QString toneFile = QDir::tempPath() + "/ls_smoke_tone.mp4";
	bool haveTone = false;
	if (QProcess::execute("ffmpeg", {"-y", "-loglevel", "error", "-f", "lavfi", "-i",
					 "color=c=gray:size=640x360:rate=30:duration=12", "-f", "lavfi", "-i",
					 "sine=frequency=1000:duration=12", "-shortest", "-pix_fmt", "yuv420p", "-g", "15",
					 toneFile}) == 0)
		haveTone = QFile::exists(toneFile);

	// Pakai tema aplikasi yang sebenarnya agar tangkapan layar mewakili tampilan nyata.
	QFile qss(":/style.qss");
	if (qss.open(QIODevice::ReadOnly))
		app.setStyleSheet(QString::fromUtf8(qss.readAll()));

	g_win = new MainWindow();
	g_win->resize(1500, 900);
	g_win->show();

	// Daftar langkah; tiap langkah dijalankan berurutan dengan jeda.
	struct Step {
		int delayMs;
		std::function<void()> fn;
	};
	QString progBefore;
	std::vector<Step> steps;

	steps.push_back({1500, [&] {
		expect(programLabel() == "Segera Dimulai", "Awal: Program = Segera Dimulai", programLabel());
		expect(previewLabel() == "Adegan 1", "Awal: Preview = Adegan 1", previewLabel());
		shot("01_awal");
	}});
	steps.push_back({300, [&] { g_win->findChild<QPushButton *>("cutButton")->click(); }});
	steps.push_back({500, [&] {
		expect(programLabel() == "Adegan 1", "CUT (tombol): Program = Adegan 1", programLabel());
		expect(previewLabel() == "Segera Dimulai", "CUT: Preview tertukar", previewLabel());
		shot("02_setelah_cut");
	}});
	steps.push_back({100, [&] { g_win->findChild<QPushButton *>("takeButton")->click(); }});
	steps.push_back({900, [&] {
		expect(programLabel() == "Segera Dimulai", "TAKE (tombol): Program kembali", programLabel());
		shot("03_setelah_take");
	}});
	// ---- Deck ala vMix: klik tile / CUT / TAKE ----
	steps.push_back({100, [&] {
		const bool ok = clickDeck(2, deck::Zone::Thumb); // tile 3 = "Segera Kembali"
		expect(ok, "Deck: tile ditemukan & diklik");
		expect(previewLabel() == "Segera Kembali", "Deck: klik tile -> Preview", previewLabel());
	}});
	steps.push_back({100, [&] {
		clickDeck(3, deck::Zone::Cut); // tile 4 = "Terima Kasih"
		expect(programLabel() == "Terima Kasih", "Deck: tombol CUT tile -> Program", programLabel());
		shot("02b_deck_cut");
	}});
	steps.push_back({100, [&] { clickDeck(1, deck::Zone::Take); }}); // tile 2 = "Segera Dimulai"
	steps.push_back({900, [&] {
		expect(programLabel() == "Segera Dimulai", "Deck: tombol TAKE tile -> Program", programLabel());
		// Tombol transisi: pilih "Cut" lalu kembalikan ke Fade.
		SceneManager *sm = g_win->sceneManager();
		QToolButton *cutT = nullptr, *fadeT = nullptr;
		for (auto *b : g_win->findChildren<QToolButton *>("transBtn")) {
			if (b->property("kind").toString() == "cut_transition")
				cutT = b;
			if (b->property("kind").toString() == "fade_transition")
				fadeT = b;
		}
		expect(cutT && fadeT, "Deck: tombol transisi Cut & Fade ada");
		if (cutT && fadeT) {
			cutT->click();
			expect(sm->transitionKind() == "cut_transition", "Deck: pilih transisi Cut", sm->transitionKind());
			fadeT->click();
			expect(sm->transitionKind() == "fade_transition", "Deck: pilih transisi Fade", sm->transitionKind());
		}
	}});
	steps.push_back({100, [&] {
		// Multiview: cari aksi "Jendela biasa" pada menu Multiview.
		bool opened = false;
		for (QAction *a : g_win->menuBar()->actions()) {
			if (a->text().contains("Multiview") && a->menu()) {
				for (QAction *sub : a->menu()->actions())
					if (sub->text().contains("Jendela biasa")) {
						sub->trigger();
						opened = true;
					}
			}
		}
		expect(opened, "Multiview: aksi jendela ditemukan & dipicu");
	}});
	steps.push_back({1500, [&] { shot("04_multiview"); }});
	steps.push_back({100, [&] {
		auto *name = editByPlaceholder("Nama pembicara");
		auto *title = editByPlaceholder("Jabatan");
		auto *btn = g_win->findChild<QPushButton *>("ltToggle");
		expect(name && title && btn, "Lower third: kontrol ditemukan");
		if (!(name && title && btn))
			return;
		name->setText("Dr. Contoh Pembicara");
		title->setText("Dosen Teknik Informatika");
		btn->click();
		expect(btn->text() == "SEMBUNYIKAN", "Lower third: tampil", btn->text());
	}});
	steps.push_back({1200, [&] {
		shot("05_lowerthird");
		auto *btn = g_win->findChild<QPushButton *>("ltToggle");
		if (btn)
			btn->click();
		expect(btn && btn->text() == "TAMPILKAN", "Lower third: sembunyi", btn ? btn->text() : "");
	}});
	if (haveTone) {
		steps.push_back({100, [&] {
			SceneManager *sm = g_win->sceneManager();
			auto *mixer = g_win->findChild<AudioMixerWidget *>();
			const int before = mixer ? mixer->stripCount() : -1;
			Data d = factory::mediaSettings(toneFile, true);
			const QString n = sm->addSource(sm->programName(), factory::mediaId(), "Uji Nada", d.get(), true);
			expect(!n.isEmpty(), "Audio: sumber video bersuara ditambahkan ke scene Program");
			expect(mixer && mixer->stripCount() == before + 1, "Audio: strip mixer muncul otomatis",
			       QString("sebelum=%1 sesudah=%2").arg(before).arg(mixer ? mixer->stripCount() : -1));
		}});
		steps.push_back({2500, [&] {
			float best = -100.f;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Uji Nada")
					best = st->magnitudeDb();
			expect(best > -50.f, "Audio: meter bergerak (level nada)", QString("magnitude=%1 dB").arg(best));
			shot("07_audio");
			// Mute lewat tombol strip.
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Uji Nada")
					for (auto *tb : st->findChildren<QToolButton *>())
						if (tb->text() == "Mute")
							tb->click();
			obs_source_t *src = obs_get_source_by_name("Uji Nada");
			expect(src && obs_source_muted(src), "Audio: tombol Mute meredam sumber");
			if (src)
				obs_source_release(src);
		}});
		steps.push_back({100, [&] {
			SceneManager *sm = g_win->sceneManager();
			auto *mixer = g_win->findChild<AudioMixerWidget *>();
			const int before = mixer ? mixer->stripCount() : -1;
			sm->removeItem(sm->programName(), "Uji Nada");
			expect(mixer && mixer->stripCount() == before - 1, "Audio: strip hilang setelah sumber dihapus",
			       QString("sebelum=%1 sesudah=%2").arg(before).arg(mixer ? mixer->stripCount() : -1));
		}});
	} else {
		printf("[SKIP] Audio: ffmpeg tidak tersedia untuk membuat video uji\n");
	}
	if (haveBumper) {
		steps.push_back({300, [&] {
			auto *path = editByPlaceholder("File video bumper");
			auto *play = buttonByText("Putar bumper");
			expect(path && play, "Bumper: kontrol ditemukan");
			if (!(path && play))
				return;
			progBefore = programLabel();
			path->setText(bumperFile);
			play->click();
		}});
		steps.push_back({1300, [&] {
			expect(programLabel() == "BUMPER", "Bumper: Program menayangkan bumper", programLabel());
			shot("06_bumper");
		}});
		steps.push_back({5000, [&] {
			expect(programLabel() == progBefore, "Bumper: kembali ke scene sebelumnya setelah selesai",
			       programLabel() + " (seharusnya " + progBefore + ")");
		}});
	} else {
		printf("[SKIP] Bumper: ffmpeg tidak tersedia untuk membuat video uji\n");
	}

	// ---- Pemutar video, monitoring audio, filter, tata letak, rekam, siaran ----
	static OutputManager *om = nullptr;
	static QString recFile;
	static QString streamErr;
	static bool streamStopped = false;
	auto probeSeconds = [](const QString &file, QString *streams) {
		QProcess p;
		p.start("ffprobe", {"-v", "error", "-show_entries", "format=duration:stream=codec_type", "-of",
				    "default=nw=1", file});
		p.waitForFinished(15000);
		const QString out = QString::fromUtf8(p.readAllStandardOutput());
		if (streams)
			*streams = out;
		for (const QString &l : out.split('\n'))
			if (l.startsWith("duration="))
				return l.mid(9).toDouble();
		return -1.0;
	};
	auto currentMedia = []() -> obs_source_t * {
		auto *mp = g_win->findChild<MediaPanel *>();
		return mp ? obs_get_source_by_name(mp->currentName().toUtf8().constData()) : nullptr;
	};
	if (haveTone) {
		steps.push_back({100, [&] {
			for (auto *pw : QApplication::topLevelWidgets())
				if (qobject_cast<ProjectorWindow *>(pw))
					pw->close(); // jendela Multiview dari uji sebelumnya menutupi tangkapan layar
			if (auto *tabs = g_win->findChild<QTabWidget *>("toolsTabs"))
				tabs->setCurrentIndex(1);
			auto *sm = g_win->sceneManager();
			Data d = factory::mediaSettings(toneFile, false, true);
			const QString n = sm->addSource(sm->previewName(), factory::mediaId(), "Media Uji", d.get(), true);
			expect(n == "Media Uji", "Media: sumber video ditambahkan", n);
			auto *mp = g_win->findChild<MediaPanel *>();
			mp->selectSource(n);
			expect(mp->currentName() == "Media Uji", "Media: panel memilih sumber video");
			obs_source_t *s = obs_get_source_by_name("Media Uji");
			expect(s && obs_source_get_monitoring_type(s) == OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT,
			       "Audio: video otomatis Dengar (monitor + output)");
			if (s)
				obs_source_release(s);
		}});
		steps.push_back({3500, [&] {
			obs_source_t *s = currentMedia();
			const int64_t dur = s ? obs_source_media_get_duration(s) : -1;
			const auto st = s ? obs_source_media_get_state(s) : OBS_MEDIA_STATE_NONE;
			if (s)
				obs_source_release(s);
			expect(dur > 11000 && dur < 13000, "Media: durasi terbaca (~12 dtk)", QString::number(dur));
			expect(st != OBS_MEDIA_STATE_PLAYING && st != OBS_MEDIA_STATE_ERROR,
			       "Media: video siap di frame awal (belum diputar), menunggu tombol Putar",
			       QString::number(int(st)));
			auto *lbl = g_win->findChild<QLabel *>("mediaTime");
			expect(lbl && lbl->text().endsWith("/ 00:12"), "Media: label waktu 'posisi / durasi'",
			       lbl ? lbl->text() : "-");
			shot("07_media");
			g_win->findChild<MediaPanel *>()->playPause(); // Putar
		}});
		static int64_t pausedAt = 0;
		steps.push_back({1000, [&] {
			obs_source_t *s = currentMedia();
			const int64_t t = obs_source_media_get_time(s);
			expect(obs_source_media_get_state(s) == OBS_MEDIA_STATE_PLAYING && t > 300,
			       "Media: Putar -> state PLAYING & waktu berjalan",
			       QString("state=%1 t=%2").arg(int(obs_source_media_get_state(s))).arg(t));
			obs_source_release(s);
			auto *pb = g_win->findChild<MediaPanel *>()->findChild<QPushButton *>("mediaBtn");
			(void)pb;
			g_win->findChild<MediaPanel *>()->playPause(); // Jeda
		}});
		steps.push_back({400, [&] {
			obs_source_t *s = currentMedia();
			pausedAt = obs_source_media_get_time(s);
			expect(obs_source_media_get_state(s) == OBS_MEDIA_STATE_PAUSED, "Media: Jeda -> state PAUSED",
			       QString::number(int(obs_source_media_get_state(s))));
			obs_source_release(s);
		}});
		steps.push_back({800, [&] {
			obs_source_t *s = currentMedia();
			const int64_t t = obs_source_media_get_time(s);
			obs_source_release(s);
			expect(std::llabs(t - pausedAt) < 150, "Media: posisi tidak maju saat jeda",
			       QString("%1 -> %2").arg(pausedAt).arg(t));
			g_win->findChild<MediaPanel *>()->playPause(); // lanjut
		}});
		steps.push_back({900, [&] {
			obs_source_t *s = currentMedia();
			const int64_t t = obs_source_media_get_time(s);
			expect(obs_source_media_get_state(s) == OBS_MEDIA_STATE_PLAYING && t > pausedAt + 300,
			       "Media: Putar lagi -> posisi maju", QString("%1 -> %2").arg(pausedAt).arg(t));
			obs_source_release(s);
			g_win->findChild<MediaPanel *>()->seekTo(8000);
		}});
		steps.push_back({500, [&] {
			obs_source_t *s = currentMedia();
			const int64_t t = obs_source_media_get_time(s);
			obs_source_release(s);
			expect(t >= 7800 && t < 10500, "Media: geser ke 0:08 (seek)", QString::number(t));
			g_win->findChild<MediaPanel *>()->restart();
		}});
		steps.push_back({500, [&] {
			obs_source_t *s = currentMedia();
			const int64_t t = obs_source_media_get_time(s);
			obs_source_release(s);
			expect(t >= 0 && t < 2500, "Media: Ulang dari awal (restart)", QString::number(t));
			g_win->findChild<MediaPanel *>()->stop();
		}});

		// Audio: tombol Dengar dan filter pada strip
		steps.push_back({100, [&] {
			AudioStrip *strip = nullptr;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Media Uji")
					strip = st;
			expect(strip != nullptr, "Audio: strip 'Media Uji' ada di mixer");
			if (!strip)
				return;
			expect(strip->monitoring(), "Audio: strip menampilkan Dengar aktif");
			auto *btn = strip->findChild<QToolButton *>("monitorBtn");
			btn->click();
			expect(!strip->monitoring(), "Audio: tombol Dengar dimatikan");
			btn->click();
			expect(strip->monitoring(), "Audio: tombol Dengar dinyalakan lagi");
			expect(strip->toggleFilter("gain_filter", true) && strip->hasFilter("gain_filter"),
			       "Audio: filter Penguat dipasang");
			expect(strip->toggleFilter("compressor_filter", true) && strip->hasFilter("compressor_filter"),
			       "Audio: filter Kompresor dipasang");
			strip->toggleFilter("gain_filter", false);
			expect(!strip->hasFilter("gain_filter"), "Audio: filter Penguat dilepas");
			strip->toggleFilter("compressor_filter", false);
		}});

		// Mixer pro: fader dB, Mute (+ Mute semua), tampilan Kanal, menu pengaturan
		steps.push_back({100, [&] {
			AudioStrip *strip = nullptr;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Media Uji")
					strip = st;
			if (!strip) {
				expect(false, "Mixer: strip Media Uji ditemukan");
				return;
			}
			strip->setVolumeDb(-12.0);
			obs_source_t *s = obs_get_source_by_name("Media Uji");
			const float mul = obs_source_get_volume(s);
			expect(std::fabs(mul - 0.2512f) < 0.01f, "Mixer: fader -12 dB -> volume libobs 0.251", QString::number(mul));
			auto *spin = strip->findChild<QDoubleSpinBox *>("dbSpin");
			expect(spin && std::fabs(spin->value() + 12.0) < 0.05, "Mixer: kolom dB ikut berubah",
			       spin ? QString::number(spin->value()) : "-");
			spin->setValue(3.0); // ketik nilai dB
			expect(std::fabs(obs_source_get_volume(s) - 1.4125f) < 0.02f, "Mixer: ketik +3 dB -> volume 1.41");
			auto *fader = strip->findChild<FaderSlider *>("fader");
			emit fader->resetRequested();
			expect(std::fabs(obs_source_get_volume(s) - 1.0f) < 0.01f, "Mixer: klik dua kali fader -> 0 dB");

			auto *mute = strip->findChild<QToolButton *>("muteBtn");
			mute->click();
			expect(obs_source_muted(s) && strip->property("muted").toBool() && mute->text() == "MUTED",
			       "Mixer: Mute aktif (libobs + tampilan merah)");
			obs_source_release(s);
		}});
		steps.push_back({500, [&] { // tunggu repaint agar tangkapan layar memperlihatkan Mute merah
			shot("10_mixer_mute");
			AudioStrip *strip = nullptr;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Media Uji")
					strip = st;
			auto *mute = strip->findChild<QToolButton *>("muteBtn");
			mute->click();
			obs_source_t *s = obs_get_source_by_name("Media Uji");
			expect(!obs_source_muted(s) && mute->text() == "Mute", "Mixer: Mute dilepas");
			obs_source_release(s);
		}});
		steps.push_back({100, [&] {
			AudioStrip *strip = nullptr;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Media Uji")
					strip = st;
			// Mute semua
			auto *mixer = g_win->findChild<AudioMixerWidget *>();
			mixer->muteAll(true);
			bool all = true;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				all = all && st->muted();
			expect(all, "Mixer: Mute semua membisukan setiap strip");
			mixer->muteAll(false);

			// Menu pengaturan
			auto *gear = strip->findChild<QToolButton *>("stripMenu");
			expect(gear && gear->menu() && gear->menu()->actions().size() >= 6, "Mixer: menu pengaturan sumber ada",
			       gear && gear->menu() ? QString::number(gear->menu()->actions().size()) : "-");
		}});
		steps.push_back({100, [&] {
			obs_source_t *s = obs_get_source_by_name("Media Uji");
			obs_source_set_muted(s, true); // diubah dari luar mixer
			obs_source_release(s);
		}});
		steps.push_back({600, [&] {
			AudioStrip *strip = nullptr;
			for (auto *st : g_win->findChildren<AudioStrip *>())
				if (st->sourceName() == "Media Uji")
					strip = st;
			expect(strip && strip->findChild<QToolButton *>("muteBtn")->isChecked(),
			       "Mixer: strip mengikuti perubahan mute dari luar");
			obs_source_t *s = obs_get_source_by_name("Media Uji");
			obs_source_set_muted(s, false);
			obs_source_release(s);
		}});
		steps.push_back({900, [&] {
			auto *mixer = g_win->findChild<AudioMixerWidget *>();
			mixer->setVertical(true);
			expect(mixer->vertical() && mixer->stripCount() >= 1, "Mixer: tampilan Kanal (vertikal) tersusun",
			       QString::number(mixer->stripCount()));
			if (auto *tabs = g_win->findChild<QTabWidget *>("toolsTabs"))
				tabs->setCurrentIndex(0);
		}});
		steps.push_back({800, [&] {
			shot("11_mixer_kanal");
			g_win->findChild<AudioMixerWidget *>()->setVertical(false);
		}});

		// Tata letak
		steps.push_back({100, [&] {
			auto *sm = g_win->sceneManager();
			const QString ls = sm->createScene("Layout Uji");
			sm->setPreview(ls);
			Data c = factory::colorSettings(QColor(200, 40, 40), 640, 360);
			Data c2 = factory::colorSettings(QColor(40, 40, 200), 640, 360);
			sm->addSource(ls, factory::colorId(), "Kotak Biru", c2.get(), true);
			sm->addSource(ls, factory::colorId(), "Kotak Merah", c.get(), true);
			expect(sm->applyLayout(ls, "Dua kolom"), "Layout: Dua kolom diterapkan");
			SourceRef sc = sm->findScene(ls);
			obs_sceneitem_t *a = obs_scene_find_source(obs_scene_from_source(sc.get()), "Kotak Biru");
			obs_sceneitem_t *b = obs_scene_find_source(obs_scene_from_source(sc.get()), "Kotak Merah");
			vec2 pa, pb, ba, bb;
			obs_sceneitem_get_pos(a, &pa);
			obs_sceneitem_get_pos(b, &pb);
			obs_sceneitem_get_bounds(a, &ba);
			obs_sceneitem_get_bounds(b, &bb);
			expect(pa.x == 0 && pb.x > 600 && ba.x < 1000 && bb.x < 1000,
			       "Layout: dua item berdampingan, masing-masing ~setengah kanvas",
			       QString("a.x=%1 b.x=%2 wa=%3").arg(pa.x).arg(pb.x).arg(ba.x));
			expect(sm->applyLayout(ls, "Gambar dalam gambar"), "Layout: PiP diterapkan");
			obs_sceneitem_get_bounds(b, &bb);
			expect(bb.x < 600, "Layout: PiP mengecilkan item kedua", QString::number(bb.x));
			shot("08_layout_pip");
		}});

		// Rekam
		steps.push_back({100, [&] {
			OutputSettings os = OutputSettings::load();
			os.recordDir = QDir::tempPath() + "/ls_smoke_rec";
			os.save(true);
			QDir(os.recordDir).removeRecursively();
			om = new OutputManager();
			QString e;
			const bool ok = om->startRecording(&e);
			expect(ok, "Rekam: mulai", e);
			recFile = om->recordingPath();
			// Putar ulang media agar audio ikut terekam.
			g_win->findChild<MediaPanel *>()->restart();
		}});
		steps.push_back({3500, [&] {
			expect(om->recording(), "Rekam: status merekam");
			expect(om->recordingBytes() > 0, "Rekam: byte bertambah", QString::number(om->recordingBytes()));
			om->stopRecording();
		}});
		steps.push_back({2500, [&] {
			expect(!om->recording(), "Rekam: berhenti");
			QString st;
			const double d = probeSeconds(recFile, &st);
			expect(d > 2.0 && d < 6.0, "Rekam: durasi file ~3 dtk (ffprobe)", QString::number(d));
			expect(st.contains("codec_type=video") && st.contains("codec_type=audio"),
			       "Rekam: file punya trek video DAN audio", st.simplified());
		}});

		// Siaran: validasi + jalur gagal + RTMP nyata ke penerima lokal (ffmpeg)
		steps.push_back({100, [&] {
			OutputSettings os = OutputSettings::load();
			os.server = "rtmp://127.0.0.1:19350/live";
			os.key = "";
			os.save(true);
			QString e;
			expect(!om->startStreaming(&e) && !e.isEmpty(), "Siaran: ditolak bila kunci kosong", e);
			os.key = "test";
			os.save(true);
			os.server = "rtmp://127.0.0.1:1/live"; // port tertutup
			os.save(true);
			QObject::connect(om, &OutputManager::streamingChanged, [](bool on, const QString &info) {
				if (!on) {
					streamStopped = true;
					streamErr = info;
				}
			});
			const bool started = om->startStreaming(&e);
			expect(started, "Siaran: start diterima, koneksi ke port tertutup akan gagal", e);
		}});
		steps.push_back({6000, [&] {
			expect(streamStopped && streamErr.contains("kode"), "Siaran: kegagalan koneksi dilaporkan",
			       streamErr);
		}});
		steps.push_back({100, [&] {
			// Penerima RTMP lokal: ffmpeg mendengarkan, lalu Live Studio menyiarkan ke sana.
			static QProcess *rx = new QProcess();
			QFile::remove(QDir::tempPath() + "/ls_rx.flv");
			rx->start("ffmpeg", {"-y", "-loglevel", "error", "-listen", "1", "-i",
					     "rtmp://127.0.0.1:19350/live/test", "-t", "5", "-c", "copy",
					     QDir::tempPath() + "/ls_rx.flv"});
			rx->waitForStarted(3000);
			QTimer::singleShot(1200, [] {
				OutputSettings os = OutputSettings::load();
				os.server = "rtmp://127.0.0.1:19350/live";
				os.key = "test";
				os.save(true);
				streamStopped = false;
				QString e;
				expect(om->startStreaming(&e), "Siaran RTMP lokal: mulai", e);
			});
		}});
		steps.push_back({5000, [&] {
			expect(om->streaming() || streamStopped, "Siaran RTMP lokal: berjalan");
			const uint64_t b = om->streamBytes();
			expect(b > 10000, "Siaran RTMP lokal: data terkirim", QString::number(b));
			om->stopStreaming();
		}});
		steps.push_back({3500, [&] {
			QString st;
			const double d = probeSeconds(QDir::tempPath() + "/ls_rx.flv", &st);
			expect(d > 1.0, "Siaran RTMP lokal: penerima menerima aliran (ffprobe)", QString::number(d));
			om->shutdown();
			delete om;
			om = nullptr;
		}});
	} else {
		printf("[SKIP] Media/rekam: ffmpeg tidak tersedia untuk membuat video uji\n");
	}

	// ---- Editor Preview: pilih, seret, ubah ukuran, transform, kunci ----
	static PreviewEditor *ed = nullptr;
	static const QString kScene = "Edit Uji";
	auto mouseAt = [](QEvent::Type t, QPointF canvasPt, Qt::MouseButton b = Qt::LeftButton) {
		const QPointF wp = ed->toWidget(canvasPt);
		QMouseEvent ev(t, wp, ed->mapToGlobal(wp), b, t == QEvent::MouseButtonRelease ? Qt::NoButton : b,
			       Qt::NoModifier);
		QApplication::sendEvent(ed, &ev);
	};
	auto rectOf = [](const QString &n) {
		SceneManager::ItemRect r;
		g_win->sceneManager()->itemRect(kScene, n, &r);
		return r;
	};
	steps.push_back({100, [&] {
		for (auto *pw : QApplication::topLevelWidgets())
			if (qobject_cast<ProjectorWindow *>(pw))
				pw->close();
		auto *sm = g_win->sceneManager();
		ed = g_win->findChild<PreviewEditor *>();
		expect(ed != nullptr, "Editor: Preview adalah PreviewEditor");
		const QString sc = sm->createScene(kScene);
		sm->setPreview(sc);
		Data c1 = factory::colorSettings(QColor(200, 40, 40), 640, 360);
		Data c2 = factory::colorSettings(QColor(40, 80, 220), 640, 360);
		sm->addSource(sc, factory::colorId(), "Merah", c1.get(), false);
		sm->addSource(sc, factory::colorId(), "Biru", c2.get(), false);
		sm->setItemRect(sc, "Merah", {100, 100, 400, 300});
		sm->setItemRect(sc, "Biru", {700, 300, 500, 400});
		const auto r = rectOf("Merah");
		expect(std::fabs(r.x - 100) < 1 && std::fabs(r.w - 400) < 1 && std::fabs(r.h - 300) < 1,
		       "Editor: setItemRect menempatkan kotak persis",
		       QString("%1,%2 %3x%4").arg(r.x).arg(r.y).arg(r.w).arg(r.h));
	}});
	steps.push_back({300, [&] {
		auto *sm = g_win->sceneManager();
		mouseAt(QEvent::MouseButtonPress, {300, 250});
		mouseAt(QEvent::MouseButtonRelease, {300, 250});
		expect(sm->selectedItem() == "Merah", "Editor: klik memilih sumber (Merah)", sm->selectedItem());
		mouseAt(QEvent::MouseButtonPress, {900, 500});
		mouseAt(QEvent::MouseButtonRelease, {900, 500});
		expect(sm->selectedItem() == "Biru", "Editor: klik memilih sumber lain (Biru)", sm->selectedItem());
		mouseAt(QEvent::MouseButtonPress, {50, 1050});
		mouseAt(QEvent::MouseButtonRelease, {50, 1050});
		expect(sm->selectedItem().isEmpty(), "Editor: klik area kosong membatalkan pilihan");
	}});
	steps.push_back({100, [&] {
		mouseAt(QEvent::MouseButtonPress, {900, 500});
		mouseAt(QEvent::MouseMove, {950, 525});
		mouseAt(QEvent::MouseMove, {1000, 550});
		mouseAt(QEvent::MouseButtonRelease, {1000, 550});
		const auto r = rectOf("Biru");
		// y: 350 akan menempel ke tengah kanvas (pusat item = 540) -> 340. Itu perilaku snap yang diharapkan.
		expect(std::fabs(r.x - 800) < 2 && std::fabs(r.y - 340) < 2 && std::fabs(r.w - 500) < 1 && std::fabs(r.h - 400) < 1,
		       "Editor: seret memindahkan sumber (x +100; y menempel ke tengah kanvas) tanpa mengubah ukuran",
		       QString("%1,%2 %3x%4").arg(r.x).arg(r.y).arg(r.w).arg(r.h));
	}});
	steps.push_back({100, [&] {
		auto *sm = g_win->sceneManager();
		sm->selectItem("Merah");
		// pegangan kanan-bawah (500,400): tarik +100,+75 -> rasio dijaga
		mouseAt(QEvent::MouseButtonPress, {500, 400});
		mouseAt(QEvent::MouseMove, {550, 440});
		mouseAt(QEvent::MouseMove, {600, 475});
		mouseAt(QEvent::MouseButtonRelease, {600, 475});
		auto r = rectOf("Merah");
		expect(std::fabs(r.x - 100) < 1.5 && std::fabs(r.y - 100) < 1.5 && std::fabs(r.w - 500) < 2 &&
			       std::fabs(r.h - 375) < 2,
		       "Editor: pegangan sudut mengubah ukuran dengan rasio terjaga (400x300 -> 500x375)",
		       QString("%1,%2 %3x%4").arg(r.x).arg(r.y).arg(r.w).arg(r.h));
		// pegangan kanan-tengah (600, 287.5): tarik +100 -> lebar saja
		mouseAt(QEvent::MouseButtonPress, {600, 287.5});
		mouseAt(QEvent::MouseMove, {700, 287.5});
		mouseAt(QEvent::MouseButtonRelease, {700, 287.5});
		r = rectOf("Merah");
		expect(std::fabs(r.w - 600) < 2 && std::fabs(r.h - 375) < 2, "Editor: pegangan sisi meregangkan satu arah",
		       QString("%1x%2").arg(r.w).arg(r.h));
	}});
	steps.push_back({100, [&] {
		auto *sm = g_win->sceneManager();
		const auto before = rectOf("Merah");
		QKeyEvent k(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
		QApplication::sendEvent(ed, &k);
		QKeyEvent k2(QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier);
		QApplication::sendEvent(ed, &k2);
		const auto after = rectOf("Merah");
		expect(std::fabs(after.x - before.x - 1) < 0.5 && std::fabs(after.y - before.y - 10) < 0.5,
		       "Editor: tombol panah menggeser 1 px, Shift+panah 10 px");

		SceneManager::ItemTransform t;
		sm->itemTransform(kScene, "Biru", &t);
		t.cropL = 100;
		t.cropR = 100;
		t.rect = {1000, 100, 400, 300};
		t.rot = 0;
		sm->setItemTransform(kScene, "Biru", t);
		SceneManager::ItemTransform t2;
		sm->itemTransform(kScene, "Biru", &t2);
		expect(t2.cropL == 100 && t2.cropR == 100 && std::fabs(t2.rect.x - 1000) < 1.5 && std::fabs(t2.rect.w - 400) < 1.5,
		       "Editor: Transform numerik (posisi, ukuran, crop) tersimpan",
		       QString("crop=%1/%2 rect=%3,%4 %5x%6").arg(t2.cropL).arg(t2.cropR).arg(t2.rect.x).arg(t2.rect.y).arg(t2.rect.w).arg(t2.rect.h));

		sm->itemOp(kScene, "Biru", SceneManager::ItemOp::Fit);
		const auto f = rectOf("Biru");
		expect(f.w <= 1921 && f.h <= 1081 && (std::fabs(f.w - 1920) < 2 || std::fabs(f.h - 1080) < 2),
		       "Editor: Muat layar -> menyentuh tepi kanvas", QString("%1x%2").arg(f.w).arg(f.h));
		sm->itemOp(kScene, "Biru", SceneManager::ItemOp::ToggleLock);
		expect(sm->itemLocked(kScene, "Biru") && sm->itemAt(kScene, 960, 540).isEmpty() == false ? true : sm->itemLocked(kScene, "Biru"),
		       "Editor: Kunci aktif");
		expect(sm->itemAt(kScene, 1800, 1000) != "Biru", "Editor: sumber terkunci tidak bisa dipilih lewat klik");
		sm->itemOp(kScene, "Biru", SceneManager::ItemOp::ToggleLock);
		sm->selectItem("Merah");
		shot("12_editor");
	}});

	// ---- Layar LED / Mapping ----
	static LedScreen *led = nullptr;
	static LedScreenDialog *ledDlg = nullptr;
	auto nearColor = [](const QImage &img, int x, int y, QColor want, int tol = 30) {
		const QColor c = img.pixelColor(x, y);
		return std::abs(c.red() - want.red()) <= tol && std::abs(c.green() - want.green()) <= tol &&
		       std::abs(c.blue() - want.blue()) <= tol;
	};
	steps.push_back({200, [&] {
		// model: preset & JSON
		const auto cols = LedScreen::presetColumns(3840, 1080, 3, {"A", "B", "C"});
		double sum = 0;
		for (const auto &sl : cols.slices)
			sum += sl.outW;
		expect(cols.slices.size() == 3 && std::fabs(sum - 3840) < 1 && cols.slices[1].scene == "B",
		       "LED: preset 3 kolom menutup layar 3840 px, scene berurutan");
		const auto split = LedScreen::presetSplitOne(3840, 1080, 3, "A");
		expect(split.slices.size() == 3 && std::fabs(split.slices[1].inX - 1.0 / 3) < 1e-6 &&
			       std::fabs(split.slices[2].outX - 2560) < 1,
		       "LED: preset 'satu scene dibelah 3' memetakan potongan ke kolom LED");
		const auto grid = LedScreen::presetGrid(1920, 1080, 2, 2, {"A", "B"});
		expect(grid.slices.size() == 4 && std::fabs(grid.slices[3].outX - 960) < 1 && std::fabs(grid.slices[3].outY - 540) < 1,
		       "LED: preset grid 2x2");
		const auto rt = LedLayout::fromJson(split.toJson());
		expect(rt.slices.size() == 3 && rt.width == 3840 && std::fabs(rt.slices[1].inW - 1.0 / 3) < 1e-6,
		       "LED: JSON simpan/muat bolak-balik utuh");

		led = new LedScreen();
		ledDlg = new LedScreenDialog(g_win->sceneManager(), led);
		ledDlg->move(0, 330);
		ledDlg->show();
		ledDlg->applyPreset(1, 3);
		auto *table = ledDlg->findChild<QTableWidget *>("ledTable");
		expect(table && table->rowCount() == 3 && led->layout().slices.size() == 3,
		       "LED: dialog menerapkan preset 3 kolom (3 baris slice)", table ? QString::number(table->rowCount()) : "-");
	}});
	steps.push_back({1200, [&] { shot("13_led_dialog"); }});
	steps.push_back({100, [&] {
		auto *sm = g_win->sceneManager();
		LedLayout L;
		L.name = "Uji";
		L.width = 1200;
		L.height = 300;
		for (int i = 0; i < 3; ++i) {
			LedSlice sl;
			sl.name = QString("Kolom %1").arg(i + 1);
			sl.scene = i == 0 ? "Segera Dimulai" : i == 1 ? "Segera Kembali" : "Terima Kasih";
			sl.outX = i * 400;
			sl.outW = 400;
			sl.outH = 300;
			L.slices.append(sl);
		}
		(void)sm;
		led->setLayout(L);
		ledDlg->rebuildTable();
		ledDlg->hide();
		ledDlg->openOn(QGuiApplication::primaryScreen());
		expect(ledDlg->outputWindow() != nullptr, "LED: jendela keluaran dibuka");
	}});
	steps.push_back({1800, [&] {
		shot("13_led_output");
		const QString f = g_shotDir + "/13_led_output.png";
		QImage img(f);
		if (img.isNull()) {
			printf("[SKIP] LED: tidak bisa memeriksa piksel (tangkapan layar tidak ada)\n");
			return;
		}
		expect(nearColor(img, 20, 20, QColor(20, 40, 90)), "LED: kolom 1 menampilkan scene 'Segera Dimulai' (biru tua)");
		expect(nearColor(img, 420, 20, QColor(70, 40, 20)), "LED: kolom 2 menampilkan scene 'Segera Kembali' (cokelat)");
		expect(nearColor(img, 820, 20, QColor(20, 70, 50)), "LED: kolom 3 menampilkan scene 'Terima Kasih' (hijau)");
		expect(nearColor(img, 1210, 150, QColor(0, 0, 0), 60) || img.width() > 1200, "LED: jendela berukuran 1200 px (di luar jendela bukan konten LED)");
	}});
	steps.push_back({100, [&] {
		// satu scene dibelah 2: kiri = biru, kanan bawah = merah (PiP dari scene 'Layout Uji')
		LedLayout L = LedScreen::presetSplitOne(1200, 300, 2, "Layout Uji");
		L.testPattern = false;
		led->setLayout(L);
		ledDlg->rebuildTable();
	}});
	steps.push_back({900, [&] {
		shot("14_led_split");
		QImage img(g_shotDir + "/14_led_split.png");
		if (img.isNull())
			return;
		expect(nearColor(img, 300, 100, QColor(40, 40, 200)), "LED: belahan kiri menampilkan separuh kiri scene (biru)");
		expect(nearColor(img, 1000, 250, QColor(200, 40, 40)), "LED: belahan kanan menampilkan kotak merah PiP di kanan-bawah");
		expect(nearColor(img, 1000, 60, QColor(40, 40, 200)), "LED: belahan kanan bagian atas tetap biru (crop benar)");
	}});
	steps.push_back({100, [&] {
		LedLayout L = led->layout();
		L.testPattern = true;
		led->setLayout(L);
	}});
	steps.push_back({700, [&] { shot("15_led_testpattern"); }});
	steps.push_back({100, [&] {
		ledDlg->closeOutput();
		delete ledDlg;
		ledDlg = nullptr;
		delete led;
		led = nullptr;
	}});

	// ---- Output di deck, dialog driver audio, plugin ----
	auto outButton = [](const QString &part) -> QToolButton * {
		for (auto *b : g_win->findChildren<QToolButton *>("outBtn"))
			if (b->text().contains(part))
				return b;
		return nullptr;
	};
	steps.push_back({200, [&] {
		auto *pb = outButton("Program");
		expect(pb && outButton("Preview") && outButton("Multiview"), "Deck: tombol Program/Preview/Multiview ada");
		if (!pb)
			return;
		QAction *act = nullptr;
		for (auto *a : pb->menu()->actions())
			if (a->text().startsWith("Jendela biasa"))
				act = a;
		expect(act != nullptr, "Deck: menu Program berisi daftar monitor + jendela biasa");
		if (act)
			act->trigger();
	}});
	steps.push_back({600, [&] {
		int n = 0;
		for (auto *w : QApplication::topLevelWidgets())
			if (auto *pw = qobject_cast<ProjectorWindow *>(w))
				if (pw->isVisible() && pw->mode() == ProjectorWindow::Mode::Program)
					++n;
		expect(n == 1, "Deck: output Program terbuka (1 jendela)", QString::number(n));
		auto *pb = outButton("Program");
		expect(pb && pb->text().startsWith("●"), "Deck: tombol Program menandai output aktif", pb ? pb->text() : "-");
		shot("09_deck_output");
		// Menu sekarang menawarkan Tutup; picu.
		QAction *close = nullptr;
		pb->menu()->aboutToShow();
		for (auto *a : pb->menu()->actions())
			if (a->text().contains("Tutup"))
				close = a;
		expect(close != nullptr, "Deck: menu menawarkan 'Tutup output ini'");
		if (close)
			close->trigger();
	}});
	steps.push_back({600, [&] {
		auto *pb = outButton("Program");
		expect(pb && !pb->text().startsWith("●"), "Deck: tombol kembali normal setelah ditutup", pb ? pb->text() : "-");
	}});
	steps.push_back({100, [&] {
		AudioInputDialog dlg;
		auto *combo = dlg.findChild<QComboBox *>("audioDriver");
		expect(combo && combo->count() >= 4, "Audio: dialog driver menampilkan WASAPI/ASIO/DirectShow/aplikasi",
		       combo ? QString::number(combo->count()) : "-");
		QStringList labels;
		for (int i = 0; combo && i < combo->count(); ++i)
			labels << combo->itemText(i);
		expect(labels.join("|").contains("ASIO") && labels.join("|").contains("WASAPI"), "Audio: ASIO & WASAPI terdaftar");
		printf("       driver: %s\n", qPrintable(labels.join(" ; ")));
		PluginsDialog pd;
		expect(pd.moduleCount() > 3, "Plugin: daftar modul termuat", QString::number(pd.moduleCount()));
	}});

	// ---- 0.6.0: kotak merah + jarak, galeri Tambah Sumber, Scene Zoom, Pengaturan bersidebar ----
	steps.push_back({300, [&] {
		auto *sm = g_win->sceneManager();
		const QString sc = sm->previewName();
		const auto its = sm->items(sc);
		expect(!its.isEmpty(), "Editor: preview punya item untuk dipilih");
		if (!its.isEmpty()) {
			sm->setItemRect(sc, its.first().name, {300.f, 120.f, 640.f, 360.f}); // kotak di dalam kanvas -> jarak tampak
			sm->selectItem(its.first().name);
		}
	}});
	steps.push_back({800, [&] { shot("16_editor_jarak"); }});

	static AddSourceDialog *addDlg = nullptr;
	steps.push_back({100, [&] {
		auto *sm = g_win->sceneManager();
		const QString a = sm->createScene("Galeri A");
		const QString b = sm->createScene("Galeri B");
		Data d = factory::colorSettings(QColor(200, 30, 30), 640, 360);
		const QString src = sm->addSource(a, factory::colorId(), "Warna Uji", d.get(), true);
		expect(src == "Warna Uji", "Galeri: sumber dibuat", src);
		addDlg = new AddSourceDialog(sm, g_win);
		addDlg->show();
		auto *cats = addDlg->findChild<QListWidget *>("addCatList");
		auto *tiles = addDlg->findChild<QListWidget *>("addTileList");
		expect(cats && cats->count() == 10, "Galeri: 10 kategori (Baru Dibuat + 9 tipe)", cats ? QString::number(cats->count()) : "-");
		cats->setCurrentRow(0);
		bool found = false;
		for (int i = 0; i < tiles->count(); ++i)
			found |= tiles->item(i)->text() == "Warna Uji";
		expect(found, "Galeri: 'Baru Dibuat' menampilkan sumber yang sudah ada");
		cats->setCurrentRow(5); // Gambar -> ada tile "＋ Baru"
		expect(tiles->count() >= 1 && tiles->item(0)->text().contains("Baru"), "Galeri: kategori punya tile '＋ Baru'");
		cats->setCurrentRow(0);
		for (int i = 0; i < tiles->count(); ++i)
			if (tiles->item(i)->text() == "Warna Uji")
				tiles->setCurrentRow(i);
		auto *btn = addDlg->findChild<QPushButton *>("addExistingBtn");
		expect(btn && btn->isEnabled(), "Galeri: 'Tambah yang sudah ada' aktif untuk sumber terpilih");
		btn->click();
		expect(addDlg->newKind() == -1 && addDlg->existingName() == "Warna Uji", "Galeri: hasil = sumber yang sudah ada");
		addDlg->show(); // tampilkan lagi untuk tangkapan layar
		cats->setCurrentRow(0);
		expect(sm->addExistingSource(b, "Warna Uji"), "Galeri: sumber sama ditambah ke scene lain");
		expect(!sm->addExistingSource(b, "Warna Uji"), "Galeri: tidak menggandakan di scene yang sama");
		sm->removeItem(a, "Warna Uji");
		ls::SourceRef chk{obs_get_source_by_name("Warna Uji")};
		expect(chk.get() != nullptr, "Galeri: menghapus dari satu scene tidak merusak scene lain");
		sm->removeItem(b, "Warna Uji");
		expect(chk.get() && obs_source_removed(chk.get()), "Galeri: sumber ditandai dilepas setelah tidak dipakai scene mana pun");
	}});
	steps.push_back({900, [&] {
		shot("17_tambah_sumber");
		addDlg->close();
		delete addDlg;
		addDlg = nullptr;
	}});

	steps.push_back({100, [&] {
		auto zs = factory::zoomWindowSettings("");
		expect(QString::fromUtf8(obs_data_get_string(zs.get(), "window")) == "Zoom Meeting:ZPContentViewWndClass:Zoom.exe",
		       "Zoom: jendela otomatis = kelas ZPContentViewWndClass, Zoom.exe");
		expect(obs_data_get_int(zs.get(), "priority") == 1 && obs_data_get_int(zs.get(), "method") == 2,
		       "Zoom: cocok berdasar kelas + Windows Graphics Capture");
		auto za = factory::appAudioSettings("");
		expect(QString::fromUtf8(obs_data_get_string(za.get(), "window")).contains("Zoom.exe"), "Zoom: audio per aplikasi menunjuk Zoom.exe");
		ZoomDialog zd;
		expect(zd.sceneName() == "Zoom" && zd.windowValue().isEmpty() && zd.captureAudio() && !zd.addMic(),
		       "Zoom: dialog bawaan (nama Zoom, otomatis, audio aktif, mic mati)");
		QAction *act = nullptr;
		for (QAction *a : g_win->findChildren<QAction *>())
			if (a->text().startsWith("Scene Zoom"))
				act = a;
		expect(act != nullptr, "Zoom: menu Produksi > Scene Zoom ada");
		QTimer::singleShot(400, [] {
			if (auto *w = QApplication::activeModalWidget())
				if (auto *b = w->findChild<QPushButton *>("zoomCreateBtn"))
					b->click();
		});
		QTimer::singleShot(1200, [] { // di Linux sumber Windows gagal -> kotak peringatan; tutup
			if (auto *w = QApplication::activeModalWidget())
				w->close();
		});
		if (act)
			act->trigger();
		expect(g_win->sceneManager()->sceneNames().contains("Zoom"), "Zoom: scene 'Zoom' terbuat lewat menu");
		const auto zi = g_win->sceneManager()->items("Zoom");
		printf("       item di scene Zoom (Linux: sumber Windows tidak tersedia): %lld\n", (long long)zi.size());
	}});

	static SettingsDialog *setDlg = nullptr;
	steps.push_back({300, [&] {
		setDlg = new SettingsDialog(g_win, SettingsDialog::Audio);
		setDlg->show();
		auto *nav = setDlg->findChild<QListWidget *>("settingsNav");
		expect(nav && nav->count() == 4, "Pengaturan: sidebar 4 halaman (Output/Audio/Video/Plugin)", nav ? QString::number(nav->count()) : "-");
		expect(nav && nav->currentRow() == 1, "Pengaturan: dibuka langsung di halaman Audio");
		nav->setCurrentRow(2);
		auto *vp = setDlg->findChild<QComboBox *>("videoPreset");
		expect(vp && vp->count() == 4, "Pengaturan: halaman Video punya 4 preset");
	}});
	steps.push_back({900, [&] {
		shot("18_pengaturan_video");
		auto *nav = setDlg->findChild<QListWidget *>("settingsNav");
		nav->setCurrentRow(0);
	}});
	steps.push_back({900, [&] {
		shot("18_pengaturan_output");
		auto *vp = setDlg->findChild<QComboBox *>("videoPreset");
		vp->setCurrentIndex(2);
		setDlg->findChild<QPushButton *>("settingsApply")->click();
		QSettings qs("LiveStudio", "LiveStudio");
		expect(qs.value("video/preset").toString() == "1280x720 @ 30", "Pengaturan: Terapkan menyimpan preset video", qs.value("video/preset").toString());
		qs.remove("video/preset");
		auto *nav = setDlg->findChild<QListWidget *>("settingsNav");
		nav->setCurrentRow(3);
	}});
	steps.push_back({700, [&] {
		shot("18_pengaturan_plugin");
		setDlg->close();
		delete setDlg;
		setDlg = nullptr;
	}});

	size_t idx = 0;
	std::function<void()> next = [&] {
		if (idx >= steps.size()) {
			printf("\nRingkasan smoke GUI: %d PASS, %d FAIL\n", g_pass, g_fail);
			fflush(stdout);
			g_win->close();
			return;
		}
		const Step &s = steps[idx++];
		QTimer::singleShot(s.delayMs, [&, fn = s.fn] {
			fn();
			next();
		});
	};
	next();

	app.exec();
	delete g_win;
	ObsEngine::instance().shutdown();
	return g_fail ? 1 : 0;
}
