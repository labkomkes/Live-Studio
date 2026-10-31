// Live Studio - pembungkus inisialisasi libobs (startup, video, audio, modul).
#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <cstdint>

namespace ls {

struct VideoSettings {
	uint32_t baseW = 1920;
	uint32_t baseH = 1080;
	uint32_t outW = 1920;
	uint32_t outH = 1080;
	uint32_t fpsNum = 30;
	uint32_t fpsDen = 1;
};

class ObsEngine {
public:
	using ListItems = QList<QPair<QString, QString>>; // (label, value)

	static ObsEngine &instance();

	// Memulai libobs. Mengembalikan false + pesan error bila gagal.
	bool start(const VideoSettings &vs, QString *error);
	// Harus dipanggil SETELAH semua reference sumber dilepas.
	void shutdown();
	bool isRunning() const { return running_; }

	// Folder root layout OBS: <root>/bin/64bit/LiveStudio.exe, <root>/obs-plugins, <root>/data
	QString rootDir() const;
	// %APPDATA%/LiveStudio
	QString configDir() const;
	const VideoSettings &video() const { return video_; }

	// "dshow_input" -> id terbaru (mis. "text_gdiplus" -> "text_gdiplus_v3").
	static QString resolveId(const char *base);
	// Apakah tipe sumber terdaftar (plugin termuat)?
	static bool isAvailable(const QString &resolvedId);
	// Modul OBS yang sengaja tidak dimuat (butuh frontend/Qt OBS).
	static QStringList deniedModules();
	// Membaca isi properti bertipe list dari sebuah tipe sumber.
	static ListItems listPropertyItems(const QString &resolvedId, const char *propertyName);

private:
	ObsEngine() = default;
	static void loadModules();
	bool running_ = false;
	VideoSettings video_;
};

} // namespace ls
