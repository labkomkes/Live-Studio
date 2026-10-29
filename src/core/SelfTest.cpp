#include "SelfTest.h"

#include "ObsEngine.h"
#include "SceneManager.h"
#include "SourceFactory.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>

#include <obs.h>

#include <cstdio>

namespace ls {

namespace {

struct Report {
	QStringList lines;
	int pass = 0, warn = 0, fail = 0;

	void ok(const QString &name, const QString &detail = {})
	{
		++pass;
		lines << "[PASS] " + name + (detail.isEmpty() ? "" : " - " + detail);
	}
	void warning(const QString &name, const QString &detail)
	{
		++warn;
		lines << "[WARN] " + name + " - " + detail;
	}
	void bad(const QString &name, const QString &detail)
	{
		++fail;
		lines << "[FAIL] " + name + " - " + detail;
	}
	void check(bool cond, const QString &name, const QString &failDetail)
	{
		cond ? ok(name) : bad(name, failDetail);
	}
};

void checkSourceTypes(Report &r)
{
	struct T {
		const char *label;
		QString id;
	};
	const T types[] = {
		{"Kamera (dshow_input)", factory::webcamId()},
		{"Capture monitor", factory::monitorId()},
		{"Capture jendela", factory::windowId()},
		{"Video (ffmpeg_source)", factory::mediaId()},
		{"Gambar (image_source)", factory::imageId()},
		{"Teks (text_gdiplus)", factory::textId()},
		{"Warna (color_source)", factory::colorId()},
		{"Mikrofon (wasapi)", factory::micId()},
		{"Audio desktop (wasapi)", factory::desktopAudioId()},
	};
	for (const T &t : types) {
		if (ObsEngine::isAvailable(t.id))
			r.ok(QString("Tipe sumber: %1").arg(t.label), t.id);
#ifndef _WIN32
		else if (t.id.startsWith("dshow_") || t.id.startsWith("monitor_") || t.id.startsWith("window_") ||
			 t.id.startsWith("wasapi_"))
			r.warning(QString("Tipe sumber: %1").arg(t.label),
				  t.id + " khusus Windows - tidak dapat diuji di platform ini");
#endif
		else
			r.bad(QString("Tipe sumber: %1").arg(t.label),
			      t.id + " tidak terdaftar - plugin tidak termuat (cek folder obs-plugins & log)");
	}
	const auto tr = SceneManager::availableTransitions();
	r.check(!tr.isEmpty(), "Transisi tersedia", "tidak ada plugin obs-transitions");
	QStringList names;
	for (const auto &p : tr)
		names << p.first;
	if (!tr.isEmpty())
		r.ok("Daftar transisi", names.join(", "));
}

void checkDevices(Report &r)
{
	auto report = [&](const char *label, const ObsEngine::ListItems &items) {
		if (items.isEmpty())
			r.warning(QString("Perangkat: %1").arg(label), "0 ditemukan (wajar bila memang tidak ada)");
		else
			r.ok(QString("Perangkat: %1").arg(label),
			     QString("%1 ditemukan, pertama: %2").arg(items.size()).arg(items.first().first));
	};
	report("kamera", factory::webcams());
	report("monitor", factory::monitors());
	report("jendela", factory::windows());
	report("mikrofon", factory::micDevices());
	report("audio desktop", factory::desktopAudioDevices());
}

void checkScenes(Report &r)
{
	SceneManager sm;
	QString err;
	if (!sm.init(&err)) {
		r.bad("SceneManager.init", err);
		return;
	}
	r.ok("SceneManager.init");

	const QStringList names = sm.sceneNames();
	r.check(names.size() >= 2, "Minimal 2 scene tersedia", QString("hanya %1 scene").arg(names.size()));
	if (names.size() < 2) {
		sm.shutdown();
		return;
	}

	// Sumber warna: tidak membutuhkan perangkat apa pun.
	const QString a = sm.createScene("SelfTest A");
	const QString b = sm.createScene("SelfTest B");
	r.check(!a.isEmpty() && !b.isEmpty(), "Buat scene", "createScene gagal");

	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	{
		Data d = factory::colorSettings(QColor(200, 30, 30), int(ovi.base_width), int(ovi.base_height));
		const QString n = sm.addSource(a, factory::colorId(), "SelfTest Warna", d.get(), true);
		r.check(!n.isEmpty(), "Tambah sumber warna ke scene", "addSource gagal");
		r.check(sm.items(a).size() == 1, "Item scene terhitung", "jumlah item != 1");
	}
	{
		Data d = factory::textSettings("Uji", 48, QColor(255, 255, 255), true);
		const QString n = sm.addSource(a, factory::textId(), "SelfTest Teks", d.get(), false);
		r.check(!n.isEmpty(), "Tambah sumber teks", "addSource gagal");
	}

	sm.setPreview(a);
	r.check(sm.previewName() == a, "setPreview", "preview tidak berubah");
	sm.sendToProgram(b, false);
	r.check(sm.programName() == b, "sendToProgram", "program tidak berubah");

	sm.setSwapAfterTake(true);
	sm.cut();
	r.check(sm.programName() == a && sm.previewName() == b, "CUT: Preview -> Program (dan tukar)",
		QString("program=%1 preview=%2").arg(sm.programName(), sm.previewName()));
	sm.take();
	r.check(sm.programName() == b && sm.previewName() == a, "TAKE: Preview -> Program (dan tukar)",
		QString("program=%1 preview=%2").arg(sm.programName(), sm.previewName()));

	sm.setPreview(a);
	r.check(sm.removeItem(a, "SelfTest Warna"), "Hapus item", "removeItem gagal");
	r.check(sm.removeScene(a), "Hapus scene non-Program", "removeScene gagal");
	sm.shutdown();
	r.ok("SceneManager.shutdown");
}

} // namespace

int runSelfTest(bool showDialog)
{
	Report r;
	const VideoSettings &v = ObsEngine::instance().video();
	r.ok("Mesin libobs aktif",
	     QString("%1x%2 @ %3 fps").arg(v.baseW).arg(v.baseH).arg(double(v.fpsNum) / double(v.fpsDen), 0, 'f', 0));

	obs_video_info ovi = {};
	r.check(obs_get_video_info(&ovi), "Video libobs terinisialisasi", "obs_get_video_info gagal");

	checkSourceTypes(r);
	checkDevices(r);
	checkScenes(r);

	QString text = QString("Live Studio %1 - laporan uji mandiri\n").arg(QApplication::applicationVersion());
	text += r.lines.join("\n");
	text += QString("\n\nRingkasan: %1 PASS, %2 WARN, %3 FAIL\n").arg(r.pass).arg(r.warn).arg(r.fail);
	text += "Uji ini TIDAK memeriksa: tampilan visual, kualitas kamera, level audio, bumper, "
		"lower third, Multiview. Gunakan docs/TEST_PLAN.md untuk uji manual.\n";

	const QString path = ObsEngine::instance().configDir() + "/selftest.txt";
	QDir().mkpath(ObsEngine::instance().configDir());
	QFile f(path);
	if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream(&f) << text;
		f.close();
	}

	if (!showDialog) {
		fputs(text.toUtf8().constData(), stdout);
		fflush(stdout);
		return r.fail ? 2 : 0;
	}

	QMessageBox box(r.fail ? QMessageBox::Warning : QMessageBox::Information, "Live Studio - Uji mandiri",
			QString("%1 PASS, %2 WARN, %3 FAIL\n\nLaporan lengkap:\n%4").arg(r.pass).arg(r.warn).arg(r.fail).arg(path));
	box.setDetailedText(text);
	box.exec();
	return r.fail ? 2 : 0;
}

} // namespace ls
