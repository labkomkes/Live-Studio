// Live Studio - pembuat settings & daftar perangkat untuk sumber libobs.
#pragma once

#include "ObsEngine.h"
#include "Raii.h"

#include <QColor>
#include <QList>
#include <QString>

namespace ls::factory {

// ID tipe sumber (sudah di-resolve ke versi terbaru).
QString webcamId();       // dshow_input
QString monitorId();      // monitor_capture
QString windowId();       // window_capture
QString mediaId();        // ffmpeg_source
QString imageId();        // image_source
QString textId();         // text_gdiplus_v*
QString colorId();        // color_source_v*
QString micId();          // wasapi_input_capture
QString desktopAudioId(); // wasapi_output_capture

// Warna Qt -> format ABGR libobs.
uint32_t toObsColor(const QColor &c, int alpha = 255);

Data webcamSettings(const QString &deviceValue, const QString &resolution /* "" = otomatis */);
Data monitorSettings(const QString &monitorValue, bool captureCursor);
Data windowSettings(const QString &windowValue);
Data mediaSettings(const QString &path, bool loop, bool stayLoaded = false);
Data imageSettings(const QString &path);
// Mengatur warna teks untuk semua varian sumber teks (gdiplus & freetype2).
void setTextColor(obs_data_t *settings, uint32_t abgr);
Data textSettings(const QString &text, int fontSize, const QColor &color, bool bold = false);
Data colorSettings(const QColor &c, int width, int height);
Data audioDeviceSettings(const QString &deviceId);

// Driver input audio yang dikenal. `available` = plugin-nya termuat di build ini.
struct AudioDriver {
	QString key;       // pengenal internal
	QString label;     // ditampilkan
	QString typeId;    // id sumber libobs (sudah di-resolve)
	QString deviceProp; // properti daftar perangkat
	QString note;      // keterangan singkat
	bool available = false;
};
QList<AudioDriver> audioInputDrivers();
// Settings sumber untuk driver + perangkat terpilih.
Data audioInputSettings(const AudioDriver &drv, const QString &deviceValue);
ObsEngine::ListItems audioDriverDevices(const AudioDriver &drv);

// Daftar perangkat dari properti sumber (label, nilai).
ObsEngine::ListItems webcams();
ObsEngine::ListItems monitors();
ObsEngine::ListItems windows();
ObsEngine::ListItems micDevices();
ObsEngine::ListItems desktopAudioDevices();

} // namespace ls::factory
