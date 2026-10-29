#include "Bumper.h"

#include "SourceFactory.h"

#include <QFileInfo>
#include <QMetaObject>

#include <obs.h>

namespace ls {

static const char *kBumperScene = "LS Bumper";
static const char *kBumperMedia = "LS Bumper Media";

Bumper::Bumper(SceneManager *sm, QObject *parent) : QObject(parent), sm_(sm)
{
	fallback_ = new QTimer(this);
	fallback_->setSingleShot(true);
	connect(fallback_, &QTimer::timeout, this, &Bumper::onMediaEnded);
}

Bumper::~Bumper()
{
	releaseAll();
}

void Bumper::releaseAll()
{
	if (media_ && signalConnected_) {
		signal_handler_disconnect(obs_source_get_signal_handler(media_.get()), "media_ended", &Bumper::mediaEndedCb,
					  this);
		signalConnected_ = false;
	}
	media_.reset();
	scene_.reset();
}

void Bumper::mediaEndedCb(void *param, calldata_t *)
{
	// Dipanggil dari thread OBS -> teruskan ke thread UI.
	auto *self = static_cast<Bumper *>(param);
	QMetaObject::invokeMethod(self, "onMediaEnded", Qt::QueuedConnection);
}

bool Bumper::ensureScene()
{
	if (scene_ && media_)
		return true;

	scene_ = sm_->ensureInternalScene(kBumperScene, false);
	if (!scene_)
		return false;
	obs_scene_t *sc = obs_scene_from_source(scene_.get());

	obs_sceneitem_t *item = obs_scene_find_source(sc, kBumperMedia);
	if (!item) {
		Data d = factory::mediaSettings(QString(), false);
		obs_source_t *src = obs_source_create(factory::mediaId().toUtf8().constData(), kBumperMedia,
						      d.get(), nullptr);
		if (!src)
			return false;
		item = obs_scene_add(sc, src);
		obs_source_release(src);
		if (!item)
			return false;

		obs_video_info ovi = {};
		obs_get_video_info(&ovi);
		obs_sceneitem_set_alignment(item, kAlignCenter);
		vec2 pos = {ovi.base_width / 2.f, ovi.base_height / 2.f};
		obs_sceneitem_set_pos(item, &pos);
		vec2 bounds = {float(ovi.base_width), float(ovi.base_height)};
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds_alignment(item, kAlignCenter);
		obs_sceneitem_set_bounds(item, &bounds);
	}
	media_ = SourceRef::borrow(obs_sceneitem_get_source(item));
	if (!media_)
		return false;

	if (!signalConnected_) {
		signal_handler_connect(obs_source_get_signal_handler(media_.get()), "media_ended",
				       &Bumper::mediaEndedCb, this);
		signalConnected_ = true;
	}
	return true;
}

bool Bumper::play(const QString &file, const QString &thenScene)
{
	if (playing_ || !QFileInfo::exists(file))
		return false;
	if (!ensureScene())
		return false;

	Data d = factory::mediaSettings(file, false);
	obs_source_update(media_.get(), d.get());

	QString back = thenScene;
	if (back.isEmpty())
		back = sm_->programName().startsWith("LS ") ? sm_->previewName() : sm_->programName();
	thenScene_ = back;

	playing_ = true;
	sm_->sendToProgram(kBumperScene, true);
	QTimer::singleShot(800, this, &Bumper::armFallback);
	emit started();
	return true;
}

void Bumper::armFallback()
{
	if (!playing_ || !media_)
		return;
	const int64_t dur = obs_source_media_get_duration(media_.get()); // ms; 0 bila belum diketahui
	// Cadangan bila sinyal media_ended tidak datang (mis. file rusak).
	fallback_->start(int(dur > 0 ? dur + 2000 : 30000));
}

void Bumper::onMediaEnded()
{
	if (!playing_)
		return;
	finish();
}

void Bumper::stop()
{
	if (!playing_)
		return;
	finish();
}

void Bumper::finish()
{
	playing_ = false;
	fallback_->stop();
	sm_->sendToProgram(thenScene_, true);
	emit finished();
}

} // namespace ls
