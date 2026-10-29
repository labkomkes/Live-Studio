#include "LowerThird.h"

#include "SourceFactory.h"

#include <obs.h>

#include <functional>

namespace ls {

static const char *kBar = "LS LT Bar";
static const char *kName = "LS LT Name";
static const char *kTitle = "LS LT Title";
static const uint32_t kFadeMs = 400;

LowerThird::LowerThird(SceneManager *sm, QObject *parent) : QObject(parent), sm_(sm)
{
	autoHide_ = new QTimer(this);
	autoHide_->setSingleShot(true);
	connect(autoHide_, &QTimer::timeout, this, &LowerThird::hide);
}

obs_sceneitem_t *LowerThird::find(const char *name) const
{
	SourceRef ov = sm_->overlaySource();
	if (!ov)
		return nullptr;
	return obs_scene_find_source(obs_scene_from_source(ov.get()), name);
}

void LowerThird::applyFade(obs_sceneitem_t *item)
{
	obs_source_t *showT = obs_source_create_private("fade_transition", "LS LT Show", nullptr);
	obs_source_t *hideT = obs_source_create_private("fade_transition", "LS LT Hide", nullptr);
	if (showT) {
		obs_sceneitem_set_transition(item, true, showT);
		obs_sceneitem_set_transition_duration(item, true, kFadeMs);
		obs_source_release(showT); // item memegang reference sendiri
	}
	if (hideT) {
		obs_sceneitem_set_transition(item, false, hideT);
		obs_sceneitem_set_transition_duration(item, false, kFadeMs);
		obs_source_release(hideT);
	}
}

bool LowerThird::ensureItems()
{
	SourceRef ov = sm_->overlaySource();
	if (!ov)
		return false;
	obs_scene_t *scene = obs_scene_from_source(ov.get());

	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const float barW = ovi.base_width * 0.46f;
	const float barH = ovi.base_height * 0.15f;
	const float x = ovi.base_width * 0.05f;
	const float y = ovi.base_height * 0.76f;

	auto addItem = [&](const char *name, const QString &typeId, obs_data_t *settings, float px,
			   float py) -> obs_sceneitem_t * {
		if (!ObsEngine::isAvailable(typeId))
			return nullptr;
		obs_source_t *src = obs_source_create(typeId.toUtf8().constData(), name, settings, nullptr);
		if (!src)
			return nullptr;
		obs_sceneitem_t *item = obs_scene_add(scene, src);
		obs_source_release(src);
		if (!item)
			return nullptr;
		obs_sceneitem_set_alignment(item, kAlignLeft | kAlignTop);
		vec2 pos = {px, py};
		obs_sceneitem_set_pos(item, &pos);
		obs_sceneitem_set_visible(item, false); // awalnya tersembunyi (sebelum transisi dipasang)
		applyFade(item);
		return item;
	};

	if (!find(kBar)) {
		Data d = factory::colorSettings(barColor_, int(barW), int(barH));
		addItem(kBar, factory::colorId(), d.get(), x, y);
	}
	if (!find(kName)) {
		Data d = factory::textSettings(name_, int(barH * 0.34f), textColor_, true);
		addItem(kName, factory::textId(), d.get(), x + barW * 0.04f, y + barH * 0.08f);
	}
	if (!find(kTitle)) {
		Data d = factory::textSettings(title_, int(barH * 0.22f), textColor_, false);
		addItem(kTitle, factory::textId(), d.get(), x + barW * 0.04f, y + barH * 0.56f);
	}
	return find(kBar) && find(kName) && find(kTitle);
}

static void updateSettings(obs_sceneitem_t *item, const std::function<void(obs_data_t *)> &fn)
{
	if (!item)
		return;
	obs_source_t *src = obs_sceneitem_get_source(item);
	obs_data_t *cur = obs_source_get_settings(src);
	fn(cur);
	obs_source_update(src, cur);
	obs_data_release(cur);
}

void LowerThird::setContent(const QString &name, const QString &title)
{
	name_ = name;
	title_ = title;
	if (!ensureItems())
		return;
	updateSettings(find(kName),
		       [&](obs_data_t *d) { obs_data_set_string(d, "text", name_.toUtf8().constData()); });
	updateSettings(find(kTitle),
		       [&](obs_data_t *d) { obs_data_set_string(d, "text", title_.toUtf8().constData()); });
}

void LowerThird::setBarColor(const QColor &c)
{
	barColor_ = c;
	if (!ensureItems())
		return;
	updateSettings(find(kBar),
		       [&](obs_data_t *d) { obs_data_set_int(d, "color", factory::toObsColor(barColor_)); });
}

void LowerThird::setTextColor(const QColor &c)
{
	textColor_ = c;
	if (!ensureItems())
		return;
	const uint32_t col = factory::toObsColor(textColor_);
	updateSettings(find(kName), [&](obs_data_t *d) { factory::setTextColor(d, col); });
	updateSettings(find(kTitle), [&](obs_data_t *d) { factory::setTextColor(d, col); });
}

void LowerThird::setVisible(bool v)
{
	if (!ensureItems())
		return;
	for (const char *n : {kBar, kName, kTitle})
		if (obs_sceneitem_t *it = find(n))
			obs_sceneitem_set_visible(it, v);
	if (shown_ != v) {
		shown_ = v;
		emit visibilityChanged(shown_);
	}
}

void LowerThird::show()
{
	autoHide_->stop();
	setVisible(true);
}

void LowerThird::showFor(int seconds)
{
	setVisible(true);
	autoHide_->stop();
	if (seconds > 0)
		autoHide_->start(seconds * 1000);
}

void LowerThird::hide()
{
	autoHide_->stop();
	setVisible(false);
}

} // namespace ls
