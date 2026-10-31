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
#include "ui/MediaPanel.h"
#include "ui/ProjectorWindow.h"
#include <QTabWidget>

#include <QAction>
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
#include <QSettings>
#include <QTimer>

#include <cstdio>

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
