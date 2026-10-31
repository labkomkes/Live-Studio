#include "ObsEngine.h"

#include <QSettings>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>

#include <obs.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ls {

static FILE *g_logFile = nullptr;
static std::mutex g_logMutex;

static void logHandler(int level, const char *fmt, va_list args, void *)
{
	char buf[4096];
	vsnprintf(buf, sizeof(buf), fmt, args);
	const char *lv = level == LOG_ERROR     ? "ERROR"
			 : level == LOG_WARNING ? "WARN"
			 : level == LOG_INFO    ? "INFO"
						: "DEBUG";
	if (level > LOG_INFO)
		return; // abaikan DEBUG agar log tidak berisik

	std::lock_guard<std::mutex> lock(g_logMutex);
	if (g_logFile) {
		fprintf(g_logFile, "[%s] %s\n", lv, buf);
		fflush(g_logFile);
	}
#ifdef _WIN32
	OutputDebugStringA(buf);
	OutputDebugStringA("\n");
#endif
}

ObsEngine &ObsEngine::instance()
{
	static ObsEngine inst;
	return inst;
}

QString ObsEngine::rootDir() const
{
	QDir d(QCoreApplication::applicationDirPath()); // <root>/bin/64bit
	d.cdUp();
	d.cdUp();
	return d.absolutePath();
}

QString ObsEngine::configDir() const
{
	const QString appData = qEnvironmentVariable("APPDATA");
	return QDir::cleanPath((appData.isEmpty() ? QDir::homePath() : appData) + "/LiveStudio");
}

// Modul OBS yang membutuhkan UI/frontend OBS Studio (obs-frontend-api). Live Studio tidak punya
// frontend itu; memuatnya menghasilkan galat dan dapat membuat aplikasi crash.
static bool isDeniedModule(const QString &name)
{
	// obs-vst & plugin *-ui memakai Qt/frontend OBS (versi Qt berbeda dari Qt Live Studio -> "Entry Point Not Found").
	static const QStringList denied = {"frontend-tools",     "obs-websocket",   "obs-browser",
					   "obs-vst",            "decklink-captions", "decklink-output-ui",
					   "aja-output-ui"};
	return denied.contains(name, Qt::CaseInsensitive);
}

struct ModuleLoadCtx {
	int loaded = 0;
	int failed = 0;
	int skipped = 0;
};

static void loadModuleCb(void *param, const obs_module_info2 *info)
{
	auto *ctx = static_cast<ModuleLoadCtx *>(param);
	const QString name = QString::fromUtf8(info->name ? info->name : "");
	if (isDeniedModule(name)) {
		blog(LOG_INFO, "Live Studio: melewati modul %s (butuh frontend OBS)", info->name);
		++ctx->skipped;
		return;
	}
	obs_module_t *module = nullptr;
	const int rc = obs_open_module(&module, info->bin_path, info->data_path);
	if (rc != MODULE_SUCCESS) {
		blog(LOG_WARNING, "Live Studio: gagal membuka modul %s (kode %d)", info->bin_path, rc);
		++ctx->failed;
		return;
	}
	if (!obs_init_module(module)) {
		blog(LOG_WARNING, "Live Studio: gagal inisialisasi modul %s", info->name);
		++ctx->failed;
		return;
	}
	++ctx->loaded;
}

void ObsEngine::loadModules()
{
	ModuleLoadCtx ctx;
#ifdef _WIN32
	// Plugin yang gagal dimuat (DLL/entry point hilang) cukup dicatat di log, bukan dialog modal
	// "Entry Point Not Found" yang menghentikan aplikasi.
	const UINT oldMode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);
#endif
	obs_find_modules2(loadModuleCb, &ctx);
#ifdef _WIN32
	SetErrorMode(oldMode);
#endif
	blog(LOG_INFO, "Live Studio: modul dimuat=%d, gagal=%d, dilewati=%d", ctx.loaded, ctx.failed, ctx.skipped);
}

bool ObsEngine::start(const VideoSettings &vs, QString *error)
{
	auto fail = [&](const QString &msg) {
		if (error)
			*error = msg;
		return false;
	};
	if (running_)
		return true;

	video_ = vs;
	QDir().mkpath(configDir() + "/logs");
	const QString logPath = configDir() + "/logs/livestudio-" +
				QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".log";
	g_logFile = fopen(logPath.toLocal8Bit().constData(), "a");
	base_set_log_handler(logHandler, nullptr);

	if (!obs_startup("en-US", configDir().toUtf8().constData(), nullptr))
		return fail("obs_startup() gagal. Periksa log di " + logPath);

	const QString root = rootDir();
#if LIBOBS_API_MAJOR_VER < 32
	// libobs >= 32 mencari data/libobs sendiri relatif terhadap exe (layout <root>/bin/64bit).
	obs_add_data_path((root + "/data/libobs/").toUtf8().constData());
#endif
	obs_add_module_path((root + "/obs-plugins/64bit").toUtf8().constData(),
			    (root + "/data/obs-plugins/%module%").toUtf8().constData());

	obs_video_info ovi = {};
#ifdef _WIN32
	ovi.graphics_module = "libobs-d3d11";
#else
	ovi.graphics_module = "libobs-opengl"; // hanya pengembangan non-Windows
#endif
	ovi.fps_num = vs.fpsNum;
	ovi.fps_den = vs.fpsDen;
	ovi.base_width = vs.baseW;
	ovi.base_height = vs.baseH;
	ovi.output_width = vs.outW;
	ovi.output_height = vs.outH;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.adapter = 0;
	ovi.gpu_conversion = true;
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.scale_type = OBS_SCALE_BICUBIC;

	const int vr = obs_reset_video(&ovi);
	if (vr != OBS_VIDEO_SUCCESS) {
		obs_shutdown();
		return fail(QString("obs_reset_video() gagal (kode %1). Pastikan GPU mendukung Direct3D 11 "
				    "dan folder data/libobs ada di %2.")
				    .arg(vr)
				    .arg(root));
	}

	obs_audio_info oai = {};
	oai.samples_per_sec = 48000;
	oai.speakers = SPEAKERS_STEREO;
	if (!obs_reset_audio(&oai)) {
		obs_shutdown();
		return fail("obs_reset_audio() gagal.");
	}

	// libobs tidak memutar audio ke speaker kecuali perangkat monitoring diset.
	{
		QSettings qs("LiveStudio", "LiveStudio");
		const QString mid = qs.value("audio/monitorId", "default").toString();
		const QString mname = qs.value("audio/monitorName", "Default").toString();
		if (obs_audio_monitoring_available())
			obs_set_audio_monitoring_device(mname.toUtf8().constData(), mid.toUtf8().constData());
		else
			blog(LOG_WARNING, "Live Studio: monitoring audio tidak tersedia di build ini");
	}

	loadModules();
	obs_log_loaded_modules();
	obs_post_load_modules();

	running_ = true;
	blog(LOG_INFO, "Live Studio: engine siap (root=%s)", root.toUtf8().constData());
	return true;
}

void ObsEngine::shutdown()
{
	if (!running_)
		return;
	obs_shutdown();
	blog(LOG_INFO, "Live Studio: obs_shutdown selesai. Alokasi bocor: %ld", bnum_allocs());
	base_set_log_handler(nullptr, nullptr);
	{
		std::lock_guard<std::mutex> lock(g_logMutex);
		if (g_logFile) {
			fclose(g_logFile);
			g_logFile = nullptr;
		}
	}
	running_ = false;
}

QString ObsEngine::resolveId(const char *base)
{
	// obs_get_latest_input_type_id() men-assert bila tipe tidak terdaftar (plugin gagal dimuat),
	// jadi cari sendiri versi tertinggi: "<base>" atau "<base>_v<N>".
	const QString b = QString::fromUtf8(base);
	QString best = b;
	int bestVer = -1;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_input_types(i, &id); ++i) {
		if (!id)
			continue;
		const QString cur = QString::fromUtf8(id);
		int ver = -1;
		if (cur == b) {
			ver = 0;
		} else if (cur.startsWith(b + "_v")) {
			bool ok = false;
			const int n = cur.mid(b.size() + 2).toInt(&ok);
			if (ok)
				ver = n;
		}
		if (ver > bestVer) {
			bestVer = ver;
			best = cur;
		}
	}
	return best; // bila tidak terdaftar, kembalikan base; isAvailable() akan menolaknya
}

bool ObsEngine::isAvailable(const QString &resolvedId)
{
	return obs_source_get_display_name(resolvedId.toUtf8().constData()) != nullptr;
}

ObsEngine::ListItems ObsEngine::listPropertyItems(const QString &resolvedId, const char *propertyName)
{
	ListItems out;
	obs_properties_t *props = obs_get_source_properties(resolvedId.toUtf8().constData());
	if (!props)
		return out;

	obs_property_t *p = obs_properties_get(props, propertyName);
	if (p && obs_property_get_type(p) == OBS_PROPERTY_LIST) {
		const enum obs_combo_format fmt = obs_property_list_format(p);
		const size_t n = obs_property_list_item_count(p);
		for (size_t i = 0; i < n; ++i) {
			if (obs_property_list_item_disabled(p, i))
				continue;
			const char *name = obs_property_list_item_name(p, i);
			QString value;
			if (fmt == OBS_COMBO_FORMAT_STRING) {
				const char *s = obs_property_list_item_string(p, i);
				value = QString::fromUtf8(s ? s : "");
			} else if (fmt == OBS_COMBO_FORMAT_INT) {
				value = QString::number(obs_property_list_item_int(p, i));
			} else {
				continue;
			}
			out.append({QString::fromUtf8(name ? name : ""), value});
		}
	}
	obs_properties_destroy(props);
	return out;
}

} // namespace ls
