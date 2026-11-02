// Live Studio - manajemen scene, sumber, Preview -> Program (CUT/TAKE), simpan/muat.
#pragma once

#include "Raii.h"

#include <QColor>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>

#include <mutex>
#include <utility>
#include <vector>

namespace ls {

class SceneManager : public QObject {
	Q_OBJECT
public:
	struct ItemInfo {
		QString name;
		QString typeId;
		bool visible = true;
		bool locked = false;
	};
	struct ItemRect {
		float x = 0, y = 0, w = 0, h = 0;
	};
	struct ItemTransform {
		ItemRect rect;
		float rot = 0;
		int cropL = 0, cropT = 0, cropR = 0, cropB = 0;
	};
	enum class ItemOp { Fit, Fill, Center, Reset, Rotate90, ToggleLock, ToFront, ToBack };
	struct SceneCell {
		QString name;
		SourceRef source;
	};

	explicit SceneManager(QObject *parent = nullptr);
	~SceneManager() override;

	// Harus dipanggil setelah ObsEngine::start().
	bool init(QString *error);
	// Melepas semua reference (panggil sebelum ObsEngine::shutdown()).
	void shutdown();

	// ---- scene ----
	QStringList sceneNames() const; // hanya scene pengguna (tanpa prefix "LS ")
	QString createScene(const QString &wantedName);
	QString createSlateScene(const QString &wantedName, const QString &headline, const QColor &bg);
	bool removeScene(const QString &name);
	bool renameScene(const QString &oldName, const QString &newName);
	SourceRef findScene(const QString &name) const; // termasuk scene internal "LS ..."
	SourceRef ensureInternalScene(const QString &name, bool withOverlay);
	SourceRef overlaySource() const;

	// ---- Preview / Program ----
	QString previewName() const { return previewName_; }
	QString programName() const { return programName_; }
	void setPreview(const QString &name);
	void cut();  // Preview -> Program seketika
	void take(); // Preview -> Program dengan transisi
	void sendToProgram(const QString &name, bool smooth);
	void setTransition(const QString &kind, int durationMs);
	QString transitionKind() const { return transitionKind_; }
	int transitionMs() const { return transitionMs_; }
	void setSwapAfterTake(bool on) { swapAfterTake_ = on; }
	bool swapAfterTake() const { return swapAfterTake_; }
	static QList<QPair<QString, QString>> availableTransitions(); // (label, id)

	// ---- aman dipanggil dari thread render (graphics thread) ----
	SourceRef previewSourceRef() const;
	SourceRef programSourceRef() const;
	QList<SceneCell> scenesForDraw() const;

	// ---- sumber / item ----
	QList<ItemInfo> items(const QString &scene) const;
	// Mengembalikan nama final sumber (kosong bila gagal). `settings` boleh nullptr.
	QString addSource(const QString &scene, const QString &typeId, const QString &name, obs_data_t *settings,
			  bool fitToCanvas);
	bool removeItem(const QString &scene, const QString &name);
	QList<ItemInfo> allSources() const;                                      // unik, dari semua scene
	bool addExistingSource(const QString &scene, const QString &sourceName); // sumber yang sama di banyak scene
	bool setItemVisible(const QString &scene, const QString &name, bool visible);
	bool moveItem(const QString &scene, const QString &name, bool up);
	static QString uniqueSourceName(const QString &base);
	// ---- edit bebas ala OBS (koordinat piksel kanvas) ----
	bool itemRect(const QString &scene, const QString &name, ItemRect *out) const; // kotak tampak (AABB)
	bool setItemRect(const QString &scene, const QString &name, const ItemRect &r);  // bounds STRETCH persis r
	bool moveItemBy(const QString &scene, const QString &name, float dx, float dy);
	QString itemAt(const QString &scene, float cx, float cy) const; // item teratas yang tidak terkunci
	bool itemOp(const QString &scene, const QString &name, ItemOp op);
	bool itemTransform(const QString &scene, const QString &name, ItemTransform *out) const;
	bool setItemTransform(const QString &scene, const QString &name, const ItemTransform &t);
	bool itemLocked(const QString &scene, const QString &name) const;
	bool renameSource(const QString &scene, const QString &oldName, const QString &newName);
	bool moveScene(const QString &name, bool up);
	QString selectedItem() const { return selected_; }
	void selectItem(const QString &name);

	// Tata letak otomatis untuk sumber video yang terlihat di scene (urutan = urutan ditambah).
	static QStringList availableLayouts();
	bool applyLayout(const QString &scene, const QString &layout);

	// ---- simpan / muat ----
	QString collectionPath() const;
	bool save();

signals:
	void scenesChanged();
	void itemsChanged(const QString &scene);
	void previewChanged(const QString &name);
	void selectionChanged(const QString &name);
	void programChanged(const QString &name);

private:
	bool load();
	void createDefaults();
	void ensureOverlay();
	void ensureGlobalAudio();
	void ensureOverlayOnTop(const QString &scene);
	void fitItemToCanvas(obs_sceneitem_t *item);
	void centerItem(obs_sceneitem_t *item);
	void startTransitionTo(const SourceRef &target, bool smooth);
	SourceRef makeScene(const QString &name);
	static QString uniqueSceneName(const QStringList &existing, const QString &base);

	mutable std::mutex mtx_; // melindungi scenes_ dan previewRef_ (dibaca thread render)
	std::vector<std::pair<QString, SourceRef>> scenes_;
	SourceRef previewRef_;
	SourceRef overlay_;
	SourceRef transition_;

	QString previewName_;
	QString programName_;
	QString selected_;
	QString transitionKind_ = "fade_transition";
	int transitionMs_ = 400;
	bool swapAfterTake_ = true;
	bool initialized_ = false;
};

} // namespace ls
