#include "SceneManager.h"

#include "ObsEngine.h"
#include "SourceFactory.h"

#include <QFile>
#include <QFileInfo>

#include <obs.h>
#include <graphics/matrix4.h>
#include <graphics/vec3.h>

#include <algorithm>

namespace ls {

static const char *kOverlayName = "LS Overlay";

SceneManager::SceneManager(QObject *parent) : QObject(parent) {}

SceneManager::~SceneManager()
{
	shutdown();
}

// ---------------------------------------------------------------- init / shutdown

bool SceneManager::init(QString *error)
{
	if (initialized_)
		return true;

	// Transisi program (channel output 0).
	obs_source_t *t = obs_source_create_private(transitionKind_.toUtf8().constData(), "LS Transition", nullptr);
	if (!t) {
		if (error)
			*error = "Tidak bisa membuat transisi (plugin obs-transitions termuat?).";
		return false;
	}
	transition_ = SourceRef(t);

	const bool loaded = load();
	if (!loaded)
		createDefaults();

	ensureOverlay();
	ensureGlobalAudio();

	// Pastikan setiap scene pengguna punya overlay.
	for (const QString &n : sceneNames())
		ensureOverlayOnTop(n);

	if (sceneNames().isEmpty())
		createScene("Adegan 1");

	const QStringList names = sceneNames();
	const QString wantedProgram = names.contains(programName_) ? programName_ : names.first();
	const QString wantedPreview = names.contains(previewName_)
					      ? previewName_
					      : (names.size() > 1 ? names.at(1) : names.first());

	SourceRef prog = findScene(wantedProgram);
	obs_transition_set(transition_.get(), prog.get());
	obs_set_output_source(0, transition_.get());
	programName_ = wantedProgram;

	setPreview(wantedPreview);
	initialized_ = true;
	return true;
}

void SceneManager::shutdown()
{
	if (!initialized_ && scenes_.empty())
		return;

	obs_set_output_source(0, nullptr);
	obs_set_output_source(1, nullptr);
	obs_set_output_source(2, nullptr);

	// Jangan memanggil libobs sambil memegang mtx_ (thread grafis membutuhkan mtx_ saat menggambar).
	SourceRef oldPreview;
	std::vector<std::pair<QString, SourceRef>> oldScenes;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		oldPreview = std::move(previewRef_);
		oldScenes.swap(scenes_);
	}
	if (oldPreview)
		obs_source_dec_showing(oldPreview.get());
	oldPreview.reset();
	oldScenes.clear();
	overlay_.reset();
	transition_.reset();
	initialized_ = false;
}

// ---------------------------------------------------------------- scene helpers

SourceRef SceneManager::makeScene(const QString &name)
{
	obs_scene_t *sc = obs_scene_create(name.toUtf8().constData());
	if (!sc)
		return {};
	SourceRef ref = SourceRef::borrow(obs_scene_get_source(sc));
	obs_scene_release(sc);
	return ref;
}

QString SceneManager::uniqueSceneName(const QStringList &existing, const QString &base)
{
	QString name = base.trimmed().isEmpty() ? QString("Adegan") : base.trimmed();
	QString candidate = name;
	int i = 2;
	while (existing.contains(candidate)) {
		candidate = QString("%1 %2").arg(name).arg(i++);
	}
	return candidate;
}

QString SceneManager::uniqueSourceName(const QString &base)
{
	QString candidate = base;
	int i = 2;
	for (;;) {
		obs_source_t *s = obs_get_source_by_name(candidate.toUtf8().constData());
		if (!s)
			return candidate;
		obs_source_release(s);
		candidate = QString("%1 %2").arg(base).arg(i++);
	}
}

QStringList SceneManager::sceneNames() const
{
	std::lock_guard<std::mutex> lock(mtx_);
	QStringList out;
	for (const auto &p : scenes_)
		out << p.first;
	return out;
}

SourceRef SceneManager::findScene(const QString &name) const
{
	{
		std::lock_guard<std::mutex> lock(mtx_);
		for (const auto &p : scenes_)
			if (p.first == name)
				return p.second;
	}
	// Scene internal (mis. "LS Bumper") atau scene yang belum terdaftar.
	obs_source_t *s = obs_get_source_by_name(name.toUtf8().constData());
	if (s && !obs_scene_from_source(s)) {
		obs_source_release(s);
		return {};
	}
	return SourceRef(s);
}

SourceRef SceneManager::ensureInternalScene(const QString &name, bool withOverlay)
{
	SourceRef existing = findScene(name);
	if (existing)
		return existing;
	SourceRef sc = makeScene(name);
	if (sc && withOverlay && overlay_) {
		obs_scene_t *scene = obs_scene_from_source(sc.get());
		obs_scene_add(scene, overlay_.get());
	}
	return sc;
}

SourceRef SceneManager::overlaySource() const
{
	return overlay_;
}

QString SceneManager::createScene(const QString &wantedName)
{
	const QString name = uniqueSceneName(sceneNames(), wantedName);
	// Hindari bentrok dengan sumber lain yang bernama sama.
	const QString finalName = uniqueSourceName(name);

	SourceRef sc = makeScene(finalName);
	if (!sc)
		return {};
	if (overlay_) {
		obs_scene_t *scene = obs_scene_from_source(sc.get());
		obs_scene_add(scene, overlay_.get());
	}
	{
		std::lock_guard<std::mutex> lock(mtx_);
		scenes_.emplace_back(finalName, sc);
	}
	emit scenesChanged();
	return finalName;
}

QString SceneManager::createSlateScene(const QString &wantedName, const QString &headline, const QColor &bg)
{
	const QString name = createScene(wantedName);
	if (name.isEmpty())
		return {};

	obs_video_info ovi = {};
	obs_get_video_info(&ovi);

	{
		Data d = factory::colorSettings(bg, int(ovi.base_width), int(ovi.base_height));
		addSource(name, factory::colorId(), name + " Latar", d.get(), false);
	}
	{
		Data d = factory::textSettings(headline, 110, QColor(255, 255, 255), true);
		addSource(name, factory::textId(), name + " Teks", d.get(), false);
	}
	// Posisikan latar di (0,0) dan teks di tengah kanvas.
	SourceRef sc = findScene(name);
	if (sc) {
		obs_scene_t *scene = obs_scene_from_source(sc.get());
		if (obs_sceneitem_t *bgItem = obs_scene_find_source(scene, (name + " Latar").toUtf8().constData())) {
			vec2 p = {0.f, 0.f};
			obs_sceneitem_set_pos(bgItem, &p);
			obs_sceneitem_set_alignment(bgItem, kAlignLeft | kAlignTop);
		}
		if (obs_sceneitem_t *txt = obs_scene_find_source(scene, (name + " Teks").toUtf8().constData())) {
			vec2 p = {ovi.base_width / 2.f, ovi.base_height / 2.f};
			obs_sceneitem_set_pos(txt, &p);
			obs_sceneitem_set_alignment(txt, kAlignCenter);
		}
	}
	emit itemsChanged(name);
	return name;
}

bool SceneManager::removeScene(const QString &name)
{
	if (name == programName_)
		return false;
	if (sceneNames().size() <= 1)
		return false;

	SourceRef target;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		auto it = std::find_if(scenes_.begin(), scenes_.end(),
				       [&](const auto &p) { return p.first == name; });
		if (it == scenes_.end())
			return false;
		target = it->second;
		scenes_.erase(it);
	}
	if (name == previewName_) {
		const QStringList names = sceneNames();
		setPreview(names.first());
	}
	// Lepaskan sumber anak (bukan item internal "LS ...") agar tidak tertinggal di mixer.
	{
		QList<SourceRef> children;
		struct Ctx {
			QList<SourceRef> *out;
		} ctx{&children};
		obs_scene_enum_items(
			obs_scene_from_source(target.get()),
			[](obs_scene_t *, obs_sceneitem_t *item, void *p) {
				obs_source_t *src = obs_sceneitem_get_source(item);
				if (!QString::fromUtf8(obs_source_get_name(src)).startsWith("LS "))
					static_cast<Ctx *>(p)->out->append(SourceRef::borrow(src));
				return true;
			},
			&ctx);
		for (const SourceRef &c : children)
			obs_source_remove(c.get());
	}
	obs_source_remove(target.get());
	emit scenesChanged();
	return true;
}

bool SceneManager::renameScene(const QString &oldName, const QString &newName)
{
	const QString trimmed = newName.trimmed();
	if (trimmed.isEmpty() || trimmed == oldName || trimmed.startsWith("LS "))
		return false;
	if (sceneNames().contains(trimmed))
		return false;
	obs_source_t *probe = obs_get_source_by_name(trimmed.toUtf8().constData());
	if (probe) {
		obs_source_release(probe);
		return false;
	}

	{
		std::lock_guard<std::mutex> lock(mtx_);
		for (auto &p : scenes_) {
			if (p.first == oldName) {
				obs_source_set_name(p.second.get(), trimmed.toUtf8().constData());
				p.first = trimmed;
				if (previewName_ == oldName)
					previewName_ = trimmed;
				if (programName_ == oldName)
					programName_ = trimmed;
				goto done;
			}
		}
		return false;
	}
done:
	emit scenesChanged();
	return true;
}

// ---------------------------------------------------------------- overlay & audio global

void SceneManager::ensureOverlay()
{
	if (overlay_)
		return;
	obs_source_t *existing = obs_get_source_by_name(kOverlayName);
	if (existing && obs_scene_from_source(existing)) {
		overlay_ = SourceRef(existing);
		return;
	}
	if (existing)
		obs_source_release(existing);
	overlay_ = makeScene(kOverlayName);
}

void SceneManager::ensureOverlayOnTop(const QString &scene)
{
	SourceRef sc = findScene(scene);
	if (!sc || !overlay_)
		return;
	obs_scene_t *s = obs_scene_from_source(sc.get());
	obs_sceneitem_t *item = obs_scene_find_source(s, kOverlayName);
	if (!item)
		item = obs_scene_add(s, overlay_.get());
	if (item)
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
}

void SceneManager::ensureGlobalAudio()
{
	auto ensure = [](const char *name, const QString &typeId, uint32_t channel) {
		if (!ObsEngine::isAvailable(typeId))
			return;
		obs_source_t *s = obs_get_source_by_name(name);
		if (!s) {
			Data d = factory::audioDeviceSettings("default");
			s = obs_source_create(typeId.toUtf8().constData(), name, d.get(), nullptr);
		}
		if (s) {
			obs_set_output_source(channel, s);
			obs_source_release(s);
		}
	};
	ensure("Audio Desktop", factory::desktopAudioId(), 1);
	ensure("Audio Mic", factory::micId(), 2);
}

// ---------------------------------------------------------------- Preview / Program

void SceneManager::setPreview(const QString &name)
{
	SourceRef ns = findScene(name);
	if (!ns)
		return;
	// Tukar pointer di dalam mutex; panggilan libobs (inc/dec showing) di luar mutex
	// agar tidak deadlock dengan thread grafis yang memanggil previewSourceRef().
	SourceRef old;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		old = previewRef_;
		previewRef_ = ns;
		previewName_ = name;
	}
	obs_source_inc_showing(ns.get());
	if (old)
		obs_source_dec_showing(old.get());
	selected_.clear();
	emit selectionChanged(QString());
	emit previewChanged(name);
}

SourceRef SceneManager::previewSourceRef() const
{
	std::lock_guard<std::mutex> lock(mtx_);
	return previewRef_;
}

SourceRef SceneManager::programSourceRef() const
{
	return SourceRef(obs_get_output_source(0));
}

QList<SceneManager::SceneCell> SceneManager::scenesForDraw() const
{
	std::lock_guard<std::mutex> lock(mtx_);
	QList<SceneCell> out;
	for (const auto &p : scenes_)
		out.append({p.first, p.second});
	return out;
}

void SceneManager::startTransitionTo(const SourceRef &target, bool smooth)
{
	if (!target || !transition_)
		return;
	const bool isCut = transitionKind_ == "cut_transition";
	if (smooth && !isCut && transitionMs_ > 0)
		obs_transition_start(transition_.get(), OBS_TRANSITION_MODE_AUTO, uint32_t(transitionMs_),
				     target.get());
	else
		obs_transition_set(transition_.get(), target.get());
}

void SceneManager::cut()
{
	if (previewName_.isEmpty() || previewName_ == programName_)
		return;
	SourceRef target = findScene(previewName_);
	startTransitionTo(target, false);

	const QString old = programName_;
	programName_ = previewName_;
	emit programChanged(programName_);
	if (swapAfterTake_ && sceneNames().contains(old))
		setPreview(old);
}

void SceneManager::take()
{
	if (previewName_.isEmpty() || previewName_ == programName_)
		return;
	SourceRef target = findScene(previewName_);
	startTransitionTo(target, true);

	const QString old = programName_;
	programName_ = previewName_;
	emit programChanged(programName_);
	if (swapAfterTake_ && sceneNames().contains(old))
		setPreview(old);
}

void SceneManager::sendToProgram(const QString &name, bool smooth)
{
	SourceRef target = findScene(name);
	if (!target)
		return;
	startTransitionTo(target, smooth);
	programName_ = name;
	emit programChanged(programName_);
}

QList<QPair<QString, QString>> SceneManager::availableTransitions()
{
	const QList<QPair<QString, QString>> all = {
		{"Cut", "cut_transition"},
		{"Fade", "fade_transition"},
		{"Swipe", "swipe_transition"},
		{"Slide", "slide_transition"},
		{"Fade ke Warna", "fade_to_color_transition"},
	};
	QList<QPair<QString, QString>> out;
	for (const auto &p : all)
		if (obs_source_get_display_name(p.second.toUtf8().constData()))
			out.append(p);
	return out;
}

void SceneManager::setTransition(const QString &kind, int durationMs)
{
	transitionMs_ = std::max(0, durationMs);
	if (kind == transitionKind_ || kind.isEmpty())
		return;

	obs_source_t *nt = obs_source_create_private(kind.toUtf8().constData(), "LS Transition", nullptr);
	if (!nt)
		return;
	SourceRef newT(nt);
	SourceRef current = findScene(programName_);
	if (current)
		obs_transition_set(newT.get(), current.get());
	obs_set_output_source(0, newT.get());
	transition_ = newT;
	transitionKind_ = kind;
}

// ---------------------------------------------------------------- item / sumber

struct EnumCtx {
	QList<SceneManager::ItemInfo> *out;
};

static bool enumItemsCb(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *ctx = static_cast<EnumCtx *>(param);
	obs_source_t *src = obs_sceneitem_get_source(item);
	const QString name = QString::fromUtf8(obs_source_get_name(src));
	if (name.startsWith("LS "))
		return true; // item internal (overlay)
	SceneManager::ItemInfo info;
	info.name = name;
	info.typeId = QString::fromUtf8(obs_source_get_id(src));
	info.visible = obs_sceneitem_visible(item);
	info.locked = obs_sceneitem_locked(item);
	ctx->out->prepend(info); // urutan atas -> bawah
	return true;
}

QList<SceneManager::ItemInfo> SceneManager::items(const QString &scene) const
{
	QList<ItemInfo> out;
	SourceRef sc = findScene(scene);
	if (!sc)
		return out;
	EnumCtx ctx{&out};
	obs_scene_enum_items(obs_scene_from_source(sc.get()), enumItemsCb, &ctx);
	return out;
}

void SceneManager::fitItemToCanvas(obs_sceneitem_t *item)
{
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

void SceneManager::centerItem(obs_sceneitem_t *item)
{
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	obs_sceneitem_set_alignment(item, kAlignCenter);
	vec2 pos = {ovi.base_width / 2.f, ovi.base_height / 2.f};
	obs_sceneitem_set_pos(item, &pos);
}

QString SceneManager::addSource(const QString &scene, const QString &typeId, const QString &name,
				obs_data_t *settings, bool fitToCanvas)
{
	if (!ObsEngine::isAvailable(typeId))
		return {};
	SourceRef sc = findScene(scene);
	if (!sc)
		return {};

	const QString finalName = uniqueSourceName(name);
	obs_source_t *src = obs_source_create(typeId.toUtf8().constData(), finalName.toUtf8().constData(),
					      settings, nullptr);
	if (!src)
		return {};
	// Video/media: terdengar di speaker operator DAN ikut ke siaran/rekaman. libobs tidak memutar
	// audio ke speaker kecuali "monitoring" aktif; tanpa ini video terlihat tetapi senyap.
	if (typeId == factory::mediaId())
		obs_source_set_monitoring_type(src, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);

	obs_sceneitem_t *item = obs_scene_add(obs_scene_from_source(sc.get()), src);
	obs_source_release(src); // scene memegang reference sendiri
	if (!item)
		return {};

	if (fitToCanvas)
		fitItemToCanvas(item);
	else
		centerItem(item);

	ensureOverlayOnTop(scene);
	emit itemsChanged(scene);
	return finalName;
}

bool SceneManager::removeItem(const QString &scene, const QString &name)
{
	SourceRef sc = findScene(scene);
	if (!sc)
		return false;
	obs_sceneitem_t *item =
		obs_scene_find_source(obs_scene_from_source(sc.get()), name.toUtf8().constData());
	if (!item)
		return false;
	// Setiap item punya sumber unik (nama unik dari addSource), jadi aman ditandai removed:
	// ini melepas sumber dari daftar global sehingga mixer audio tidak menahannya.
	SourceRef src = SourceRef::borrow(obs_sceneitem_get_source(item));
	obs_sceneitem_remove(item);
	if (src && !QString::fromUtf8(obs_source_get_name(src.get())).startsWith("LS "))
		obs_source_remove(src.get());
	emit itemsChanged(scene);
	return true;
}

bool SceneManager::setItemVisible(const QString &scene, const QString &name, bool visible)
{
	SourceRef sc = findScene(scene);
	if (!sc)
		return false;
	obs_sceneitem_t *item =
		obs_scene_find_source(obs_scene_from_source(sc.get()), name.toUtf8().constData());
	if (!item)
		return false;
	obs_sceneitem_set_visible(item, visible);
	return true;
}

bool SceneManager::moveItem(const QString &scene, const QString &name, bool up)
{
	SourceRef sc = findScene(scene);
	if (!sc)
		return false;
	obs_sceneitem_t *item =
		obs_scene_find_source(obs_scene_from_source(sc.get()), name.toUtf8().constData());
	if (!item)
		return false;
	obs_sceneitem_set_order(item, up ? OBS_ORDER_MOVE_UP : OBS_ORDER_MOVE_DOWN);
	ensureOverlayOnTop(scene);
	emit itemsChanged(scene);
	return true;
}

// ---------------------------------------------------------------- default / simpan / muat

void SceneManager::createDefaults()
{
	ensureOverlay();
	createScene("Adegan 1");
	createSlateScene("Segera Dimulai", "Webinar akan segera dimulai", QColor(20, 40, 90));
	createSlateScene("Segera Kembali", "Kami segera kembali", QColor(70, 40, 20));
	createSlateScene("Terima Kasih", "Terima kasih telah bergabung", QColor(20, 70, 50));
	programName_ = "Segera Dimulai";
	previewName_ = "Adegan 1";
}

QString SceneManager::collectionPath() const
{
	return ObsEngine::instance().configDir() + "/scenes.json";
}

static bool saveFilter(void *, obs_source_t *source)
{
	const obs_source_type t = obs_source_get_type(source);
	return t == OBS_SOURCE_TYPE_INPUT || t == OBS_SOURCE_TYPE_SCENE;
}

bool SceneManager::save()
{
	if (!initialized_)
		return false;

	Data root;
	obs_data_array_t *sources = obs_save_sources_filtered(saveFilter, nullptr);
	obs_data_set_array(root.get(), "sources", sources);
	obs_data_array_release(sources);

	obs_data_array_t *order = obs_data_array_create();
	for (const QString &n : sceneNames()) {
		Data entry;
		obs_data_set_string(entry.get(), "name", n.toUtf8().constData());
		obs_data_array_push_back(order, entry.get());
	}
	obs_data_set_array(root.get(), "order", order);
	obs_data_array_release(order);

	obs_data_set_string(root.get(), "program", programName_.toUtf8().constData());
	obs_data_set_string(root.get(), "preview", previewName_.toUtf8().constData());
	obs_data_set_string(root.get(), "transition", transitionKind_.toUtf8().constData());
	obs_data_set_int(root.get(), "transition_ms", transitionMs_);

	const QByteArray path = collectionPath().toUtf8();
	return obs_data_save_json_safe(root.get(), path.constData(), "tmp", "bak");
}

static void loadSourceNoop(void *, obs_source_t *) {}

bool SceneManager::load()
{
	if (!QFileInfo::exists(collectionPath()))
		return false;

	obs_data_t *root = obs_data_create_from_json_file(collectionPath().toUtf8().constData());
	if (!root)
		return false;

	obs_data_array_t *sources = obs_data_get_array(root, "sources");
	if (!sources) {
		obs_data_release(root);
		return false;
	}
	obs_load_sources(sources, loadSourceNoop, nullptr);
	obs_data_array_release(sources);

	// Susun ulang scene sesuai urutan tersimpan.
	obs_data_array_t *order = obs_data_get_array(root, "order");
	if (order) {
		const size_t n = obs_data_array_count(order);
		for (size_t i = 0; i < n; ++i) {
			obs_data_t *e = obs_data_array_item(order, i);
			const QString name = QString::fromUtf8(obs_data_get_string(e, "name"));
			obs_data_release(e);
			if (name.startsWith("LS "))
				continue;
			SourceRef sc = findScene(name);
			if (sc) {
				std::lock_guard<std::mutex> lock(mtx_);
				scenes_.emplace_back(name, sc);
			}
		}
		obs_data_array_release(order);
	}

	programName_ = QString::fromUtf8(obs_data_get_string(root, "program"));
	previewName_ = QString::fromUtf8(obs_data_get_string(root, "preview"));
	const QString tk = QString::fromUtf8(obs_data_get_string(root, "transition"));
	if (!tk.isEmpty())
		transitionKind_ = tk;
	transitionMs_ = int(obs_data_get_int(root, "transition_ms"));
	if (transitionMs_ <= 0)
		transitionMs_ = 400;
	obs_data_release(root);

	if (transitionKind_ != "fade_transition") {
		obs_source_t *nt =
			obs_source_create_private(transitionKind_.toUtf8().constData(), "LS Transition", nullptr);
		if (nt)
			transition_ = SourceRef(nt);
		else
			transitionKind_ = "fade_transition";
	}

	{
		std::lock_guard<std::mutex> lock(mtx_);
		if (scenes_.empty())
			return false;
	}
	return true;
}

} // namespace ls

namespace ls {


// ---------------------------------------------------------------- tata letak

namespace {
struct LayoutCtx {
	std::vector<obs_sceneitem_t *> items;
};
bool collectLayoutItems(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *ctx = static_cast<LayoutCtx *>(param);
	obs_source_t *s = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(s);
	if (name && QString::fromUtf8(name).startsWith("LS "))
		return true; // overlay lower third
	if (!(obs_source_get_output_flags(s) & OBS_SOURCE_VIDEO))
		return true; // sumber audio saja
	if (!obs_sceneitem_visible(item))
		return true;
	ctx->items.push_back(item);
	return true;
}
void placeBox(obs_sceneitem_t *item, float x, float y, float w, float h)
{
	obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
	obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
	vec2 b{w, h}, pos{x, y};
	obs_sceneitem_set_bounds(item, &b);
	obs_sceneitem_set_pos(item, &pos);
}
} // namespace

QStringList SceneManager::availableLayouts()
{
	return {"Layar penuh", "Dua kolom", "Pembicara + slide", "Gambar dalam gambar"};
}

bool SceneManager::applyLayout(const QString &scene, const QString &layout)
{
	SourceRef sc = findScene(scene);
	if (!sc)
		return false;
	obs_video_info ovi;
	if (!obs_get_video_info(&ovi))
		return false;
	const float W = float(ovi.base_width), H = float(ovi.base_height);

	LayoutCtx ctx;
	obs_scene_enum_items(obs_scene_from_source(sc.get()), collectLayoutItems, &ctx);
	// obs_scene_enum_items mengurutkan dari lapisan terbawah ke teratas = urutan sumber ditambahkan.
	const size_t n = ctx.items.size();
	if (n == 0)
		return false;

	if (layout == "Layar penuh") {
		for (auto *it : ctx.items)
			placeBox(it, 0, 0, W, H);
	} else if (layout == "Dua kolom") {
		const float gap = W * 0.01f;
		const float cw = (W - gap) / 2.f;
		for (size_t i = 0; i < n; ++i) {
			if (i == 0)
				placeBox(ctx.items[i], 0, 0, n == 1 ? W : cw, H);
			else
				placeBox(ctx.items[i], cw + gap, 0, cw, H);
		}
	} else if (layout == "Pembicara + slide") {
		// item pertama (slide/layar) 70% kiri; sisanya (kamera) tumpuk di kolom kanan.
		const float gap = W * 0.01f;
		const float lw = W * 0.7f;
		const float rw = W - lw - gap;
		placeBox(ctx.items[0], 0, 0, n == 1 ? W : lw, H);
		if (n > 1) {
			const float rh = H / float(n - 1);
			for (size_t i = 1; i < n; ++i)
				placeBox(ctx.items[i], lw + gap, rh * float(i - 1), rw, rh);
		}
	} else if (layout == "Gambar dalam gambar") {
		placeBox(ctx.items[0], 0, 0, W, H);
		const float pw = W * 0.28f, ph = H * 0.28f, m = W * 0.02f;
		for (size_t i = 1; i < n; ++i)
			placeBox(ctx.items[i], W - pw - m, H - ph * float(i) - m * float(i), pw, ph);
	} else {
		return false;
	}
	emit itemsChanged(scene);
	return true;
}


// ---------------------------------------------------------------- edit bebas

static obs_sceneitem_t *findItem(const SourceRef &sc, const QString &name)
{
	return sc ? obs_scene_find_source(obs_scene_from_source(sc.get()), name.toUtf8().constData()) : nullptr;
}

static bool boxRect(obs_sceneitem_t *item, SceneManager::ItemRect *r)
{
	matrix4 m;
	obs_sceneitem_get_box_transform(item, &m);
	const float px[4][2] = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
	float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
	for (auto &p : px) {
		vec3 in, out;
		vec3_set(&in, p[0], p[1], 0.f);
		vec3_transform(&out, &in, &m);
		x0 = std::min(x0, out.x);
		y0 = std::min(y0, out.y);
		x1 = std::max(x1, out.x);
		y1 = std::max(y1, out.y);
	}
	r->x = x0;
	r->y = y0;
	r->w = x1 - x0;
	r->h = y1 - y0;
	return r->w > 0 && r->h > 0;
}

bool SceneManager::itemRect(const QString &scene, const QString &name, ItemRect *out) const
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	return it && boxRect(it, out);
}

bool SceneManager::setItemRect(const QString &scene, const QString &name, const ItemRect &r)
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	if (!it || r.w < 1.f || r.h < 1.f)
		return false;
	obs_sceneitem_set_rot(it, 0.f);
	obs_sceneitem_set_alignment(it, kAlignLeft | kAlignTop);
	obs_sceneitem_set_bounds_type(it, OBS_BOUNDS_STRETCH);
	obs_sceneitem_set_bounds_alignment(it, kAlignCenter);
	vec2 b = {r.w, r.h}, pos = {r.x, r.y};
	obs_sceneitem_set_bounds(it, &b);
	obs_sceneitem_set_pos(it, &pos);
	return true;
}

bool SceneManager::moveItemBy(const QString &scene, const QString &name, float dx, float dy)
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	if (!it)
		return false;
	vec2 pos;
	obs_sceneitem_get_pos(it, &pos);
	pos.x += dx;
	pos.y += dy;
	obs_sceneitem_set_pos(it, &pos);
	return true;
}

namespace {
struct HitCtx {
	float x, y;
	obs_sceneitem_t *hit = nullptr;
};
bool hitCb(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *c = static_cast<HitCtx *>(param);
	const char *n = obs_source_get_name(obs_sceneitem_get_source(item));
	if (n && QString::fromUtf8(n).startsWith("LS "))
		return true;
	if (!obs_sceneitem_visible(item) || obs_sceneitem_locked(item))
		return true;
	if (!(obs_source_get_output_flags(obs_sceneitem_get_source(item)) & OBS_SOURCE_VIDEO))
		return true;
	SceneManager::ItemRect r;
	if (boxRect(item, &r) && c->x >= r.x && c->x <= r.x + r.w && c->y >= r.y && c->y <= r.y + r.h)
		c->hit = item; // enumerasi bawah -> atas; yang terakhir cocok = paling atas
	return true;
}
} // namespace

QString SceneManager::itemAt(const QString &scene, float cx, float cy) const
{
	SourceRef sc = findScene(scene);
	if (!sc)
		return {};
	HitCtx c{cx, cy};
	obs_scene_enum_items(obs_scene_from_source(sc.get()), hitCb, &c);
	return c.hit ? QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(c.hit))) : QString();
}

bool SceneManager::itemLocked(const QString &scene, const QString &name) const
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	return it && obs_sceneitem_locked(it);
}

bool SceneManager::itemOp(const QString &scene, const QString &name, ItemOp op)
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	if (!it)
		return false;
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	const float W = float(ovi.base_width), H = float(ovi.base_height);
	auto place = [&](obs_bounds_type bt) {
		obs_sceneitem_set_rot(it, 0.f);
		obs_sceneitem_set_alignment(it, kAlignCenter);
		vec2 pos = {W / 2.f, H / 2.f}, b = {W, H};
		obs_sceneitem_set_pos(it, &pos);
		obs_sceneitem_set_bounds_type(it, bt);
		obs_sceneitem_set_bounds_alignment(it, kAlignCenter);
		obs_sceneitem_set_bounds(it, &b);
	};
	switch (op) {
	case ItemOp::Fit:
		place(OBS_BOUNDS_SCALE_INNER);
		break;
	case ItemOp::Fill:
		place(OBS_BOUNDS_SCALE_OUTER);
		break;
	case ItemOp::Center: {
		ItemRect r;
		if (boxRect(it, &r))
			moveItemBy(scene, name, W / 2.f - (r.x + r.w / 2.f), H / 2.f - (r.y + r.h / 2.f));
		break;
	}
	case ItemOp::Reset: { // ukuran asli 1:1, di tengah
		obs_sceneitem_set_rot(it, 0.f);
		obs_sceneitem_set_bounds_type(it, OBS_BOUNDS_NONE);
		obs_sceneitem_set_alignment(it, kAlignCenter);
		vec2 sc1 = {1.f, 1.f}, pos = {W / 2.f, H / 2.f};
		obs_sceneitem_set_scale(it, &sc1);
		obs_sceneitem_set_pos(it, &pos);
		break;
	}
	case ItemOp::Rotate90: {
		ItemRect r;
		if (!boxRect(it, &r))
			break;
		// putar mengelilingi titik tengah kotak
		const float cxp = r.x + r.w / 2.f, cyp = r.y + r.h / 2.f;
		obs_bounds_type bt = obs_sceneitem_get_bounds_type(it);
		if (bt == OBS_BOUNDS_NONE) {
			obs_sceneitem_set_alignment(it, kAlignCenter);
			vec2 pos = {cxp, cyp};
			obs_sceneitem_set_pos(it, &pos);
		} else {
			obs_sceneitem_set_alignment(it, kAlignCenter);
			vec2 pos = {cxp, cyp};
			obs_sceneitem_set_pos(it, &pos);
		}
		float rot = obs_sceneitem_get_rot(it) + 90.f;
		if (rot >= 360.f)
			rot -= 360.f;
		obs_sceneitem_set_rot(it, rot);
		break;
	}
	case ItemOp::ToggleLock:
		obs_sceneitem_set_locked(it, !obs_sceneitem_locked(it));
		break;
	case ItemOp::ToFront:
		obs_sceneitem_set_order(it, OBS_ORDER_MOVE_TOP);
		ensureOverlayOnTop(scene);
		break;
	case ItemOp::ToBack:
		obs_sceneitem_set_order(it, OBS_ORDER_MOVE_BOTTOM);
		break;
	}
	emit itemsChanged(scene);
	return true;
}

bool SceneManager::itemTransform(const QString &scene, const QString &name, ItemTransform *out) const
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	if (!it || !boxRect(it, &out->rect))
		return false;
	out->rot = obs_sceneitem_get_rot(it);
	obs_sceneitem_crop c;
	obs_sceneitem_get_crop(it, &c);
	out->cropL = c.left;
	out->cropT = c.top;
	out->cropR = c.right;
	out->cropB = c.bottom;
	return true;
}

bool SceneManager::setItemTransform(const QString &scene, const QString &name, const ItemTransform &t)
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, name);
	if (!it)
		return false;
	obs_sceneitem_crop c = {t.cropL, t.cropT, t.cropR, t.cropB};
	obs_sceneitem_set_crop(it, &c);
	// Tempatkan kotak (sudah memperhitungkan crop karena bounds mengatur ukuran akhir).
	obs_sceneitem_set_alignment(it, kAlignCenter);
	obs_sceneitem_set_bounds_type(it, OBS_BOUNDS_STRETCH);
	obs_sceneitem_set_bounds_alignment(it, kAlignCenter);
	vec2 b = {std::max(1.f, t.rect.w), std::max(1.f, t.rect.h)};
	vec2 pos = {t.rect.x + t.rect.w / 2.f, t.rect.y + t.rect.h / 2.f};
	obs_sceneitem_set_bounds(it, &b);
	obs_sceneitem_set_pos(it, &pos);
	obs_sceneitem_set_rot(it, t.rot);
	return true;
}

bool SceneManager::renameSource(const QString &scene, const QString &oldName, const QString &newName)
{
	SourceRef sc = findScene(scene);
	obs_sceneitem_t *it = findItem(sc, oldName);
	const QString n = newName.trimmed();
	if (!it || n.isEmpty() || n.startsWith("LS "))
		return false;
	if (n == oldName)
		return true;
	obs_source_t *exists = obs_get_source_by_name(n.toUtf8().constData());
	if (exists) {
		obs_source_release(exists);
		return false;
	}
	obs_source_set_name(obs_sceneitem_get_source(it), n.toUtf8().constData());
	if (selected_ == oldName)
		selected_ = n;
	emit itemsChanged(scene);
	return true;
}

bool SceneManager::moveScene(const QString &name, bool up)
{
	{
		std::lock_guard<std::mutex> lock(mtx_);
		int i = -1;
		for (size_t k = 0; k < scenes_.size(); ++k)
			if (scenes_[k].first == name)
				i = int(k);
		const int j = up ? i - 1 : i + 1;
		if (i < 0 || j < 0 || j >= int(scenes_.size()))
			return false;
		std::swap(scenes_[size_t(i)], scenes_[size_t(j)]);
	}
	emit scenesChanged();
	return true;
}

void SceneManager::selectItem(const QString &name)
{
	if (selected_ == name)
		return;
	selected_ = name;
	emit selectionChanged(name);
}

} // namespace ls
