#include "SourceFactory.h"

namespace ls::factory {

QString webcamId() { return ObsEngine::resolveId("dshow_input"); }
QString monitorId() { return ObsEngine::resolveId("monitor_capture"); }
QString windowId() { return ObsEngine::resolveId("window_capture"); }
QString mediaId() { return ObsEngine::resolveId("ffmpeg_source"); }
QString imageId() { return ObsEngine::resolveId("image_source"); }
QString textId()
{
	// Utama: GDI+ (Windows). Cadangan: FreeType2 (plugin text-freetype2, ada di Windows & Linux).
	const QString gdi = ObsEngine::resolveId("text_gdiplus");
	if (ObsEngine::isAvailable(gdi))
		return gdi;
	const QString ft2 = ObsEngine::resolveId("text_ft2_source");
	if (ObsEngine::isAvailable(ft2))
		return ft2;
	return gdi;
}
QString colorId() { return ObsEngine::resolveId("color_source"); }
QString micId() { return ObsEngine::resolveId("wasapi_input_capture"); }
QString desktopAudioId() { return ObsEngine::resolveId("wasapi_output_capture"); }

uint32_t toObsColor(const QColor &c, int alpha)
{
	return (uint32_t(alpha & 0xFF) << 24) | (uint32_t(c.blue()) << 16) | (uint32_t(c.green()) << 8) |
	       uint32_t(c.red());
}

Data webcamSettings(const QString &deviceValue, const QString &resolution)
{
	Data d;
	obs_data_set_string(d.get(), "video_device_id", deviceValue.toUtf8().constData());
	if (!resolution.isEmpty()) {
		obs_data_set_int(d.get(), "res_type", 1); // 1 = custom
		obs_data_set_string(d.get(), "resolution", resolution.toUtf8().constData());
	}
	return d;
}

Data monitorSettings(const QString &monitorValue, bool captureCursor)
{
	Data d;
	// OBS terbaru memakai "monitor_id" (string); versi lama memakai "monitor" (indeks).
	obs_data_set_string(d.get(), "monitor_id", monitorValue.toUtf8().constData());
	bool ok = false;
	const int idx = monitorValue.toInt(&ok);
	if (ok)
		obs_data_set_int(d.get(), "monitor", idx);
	obs_data_set_bool(d.get(), "capture_cursor", captureCursor);
	return d;
}

Data windowSettings(const QString &windowValue)
{
	Data d;
	obs_data_set_string(d.get(), "window", windowValue.toUtf8().constData());
	obs_data_set_bool(d.get(), "cursor", true);
	return d;
}

Data mediaSettings(const QString &path, bool loop, bool stayLoaded)
{
	Data d;
	obs_data_set_bool(d.get(), "is_local_file", true);
	obs_data_set_string(d.get(), "local_file", path.toUtf8().constData());
	obs_data_set_bool(d.get(), "looping", loop);
	// stayLoaded: video tetap terbuka/diputar walau scene-nya hanya di Preview (bisa dilatih & dikontrol
	// dengan pemutar video). Bumper memakai false: mulai dari awal saat tayang.
	obs_data_set_bool(d.get(), "restart_when_active", !stayLoaded);
	obs_data_set_bool(d.get(), "close_when_inactive", !stayLoaded);
	obs_data_set_bool(d.get(), "hw_decode", true);
	return d;
}

Data imageSettings(const QString &path)
{
	Data d;
	obs_data_set_string(d.get(), "file", path.toUtf8().constData());
	return d;
}

Data textSettings(const QString &text, int fontSize, const QColor &color, bool bold)
{
	Data d;
	obs_data_set_string(d.get(), "text", text.toUtf8().constData());

	Data font;
	obs_data_set_string(font.get(), "face", "Segoe UI");
	obs_data_set_int(font.get(), "size", fontSize);
	obs_data_set_string(font.get(), "style", bold ? "Bold" : "Regular");
	obs_data_set_int(font.get(), "flags", bold ? 1 : 0); // 1 = bold
	obs_data_set_obj(d.get(), "font", font.get());

	setTextColor(d.get(), toObsColor(color));
	obs_data_set_int(d.get(), "opacity", 100);
	return d;
}

void setTextColor(obs_data_t *settings, uint32_t abgr)
{
	obs_data_set_int(settings, "color", abgr);  // text_gdiplus
	obs_data_set_int(settings, "color1", abgr); // text_ft2_source (atas)
	obs_data_set_int(settings, "color2", abgr); // text_ft2_source (bawah)
}

Data colorSettings(const QColor &c, int width, int height)
{
	Data d;
	obs_data_set_int(d.get(), "color", toObsColor(c));
	obs_data_set_int(d.get(), "width", width);
	obs_data_set_int(d.get(), "height", height);
	return d;
}

Data audioDeviceSettings(const QString &deviceId)
{
	Data d;
	obs_data_set_string(d.get(), "device_id", deviceId.toUtf8().constData());
	return d;
}

ObsEngine::ListItems webcams() { return ObsEngine::listPropertyItems(webcamId(), "video_device_id"); }

ObsEngine::ListItems monitors()
{
	auto items = ObsEngine::listPropertyItems(monitorId(), "monitor_id");
	if (items.isEmpty())
		items = ObsEngine::listPropertyItems(monitorId(), "monitor");
	return items;
}

ObsEngine::ListItems windows() { return ObsEngine::listPropertyItems(windowId(), "window"); }
ObsEngine::ListItems micDevices() { return ObsEngine::listPropertyItems(micId(), "device_id"); }
ObsEngine::ListItems desktopAudioDevices() { return ObsEngine::listPropertyItems(desktopAudioId(), "device_id"); }

} // namespace ls::factory
