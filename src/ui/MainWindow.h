// Live Studio - jendela utama.
// Atas (gaya OBS Studio Mode): Preview | CUT/TAKE | Program, lalu panel Adegan/Sumber/Audio/Lower Third.
// Bawah (gaya vMix): deck tile scene dengan thumbnail live + kontrol transisi.
#pragma once

#include "ProjectorWindow.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>

class QAction;
class QScreen;
class QButtonGroup;
class QCheckBox;
class QFrame;
class QLabel;
class QListWidget;
class QMenu;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QToolButton;
class QTimer;
class QSplitter;

namespace ls {

class SceneManager;
class LowerThird;
class Bumper;
class ObsDisplayWidget;
class AudioMixerWidget;
class LowerThirdPanel;
class BumperPanel;
class ProjectorWindow;
class DeckStrip;
class MediaPanel;
class OutputManager;

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() override;

	// Untuk uji otomatis (tests/smoke_gui.cpp).
	SceneManager *sceneManager() const { return sm_; }

protected:
	void closeEvent(QCloseEvent *e) override;

private:
	enum class SourceKind { Webcam, Screen, Window, Video, Image, Text, Mic, DesktopAudio };

	void buildSourceMenu();
	void buildUi();
	QWidget *buildPanels(QWidget *parent);
	QWidget *buildDeck(QWidget *parent);
	void buildMenus();
	void connectSignals();
	void applyMode(bool pro);
	void refreshScenes();
	void refreshItems();
	void refreshOutputLabels();
	void refreshTransitionControls();
	void addSourceOfKind(SourceKind kind);
	void openProjector(int mode, QScreen *screen);
	void fillScreenMenu(QMenu *menu, int mode);
	void refreshOutputButtons();
	ProjectorWindow *projectorFor(int mode) const;
	void chooseVideoSettings();
	void showAbout();
	void buildStatusBar();
	void toggleRecording();
	void toggleStreaming();
	void updateStats();
	QString editedScene() const; // scene yang sedang diedit = Preview
	static QString friendlyType(const QString &typeId);

	SceneManager *sm_ = nullptr;
	LowerThird *lt_ = nullptr;
	Bumper *bumper_ = nullptr;

	ObsDisplayWidget *preview_ = nullptr;
	ObsDisplayWidget *program_ = nullptr;
	DeckStrip *deckStrip_ = nullptr;
	QLabel *previewLabel_ = nullptr;
	QLabel *programLabel_ = nullptr;
	QPushButton *cutBtn_ = nullptr;
	QPushButton *takeBtn_ = nullptr;
	QButtonGroup *transitionGroup_ = nullptr;
	QSpinBox *durationSpin_ = nullptr;
	QCheckBox *swapCheck_ = nullptr;
	QPushButton *ltQuickBtn_ = nullptr;
	QToolButton *programBtn_ = nullptr;
	QToolButton *previewOutBtn_ = nullptr;
	QToolButton *multiviewBtn_ = nullptr;

	QListWidget *scenesList_ = nullptr;
	QListWidget *sourcesList_ = nullptr;
	AudioMixerWidget *audio_ = nullptr;
	LowerThirdPanel *ltPanel_ = nullptr;
	BumperPanel *bumperPanel_ = nullptr;
	MediaPanel *media_ = nullptr;
	OutputManager *out_ = nullptr;
	QPushButton *recBtn_ = nullptr;
	QPushButton *liveBtn_ = nullptr;
	QLabel *statLabel_ = nullptr;
	QTimer *statTimer_ = nullptr;
	qint64 recStartMs_ = 0, liveStartMs_ = 0;
	uint64_t lastStreamBytes_ = 0;
	void *cpuInfo_ = nullptr;

	QFrame *scenesPanel_ = nullptr;
	QFrame *sourcesPanel_ = nullptr;
	QTabWidget *toolsTabs_ = nullptr;
	QSplitter *mainSplit_ = nullptr;

	QMenu *addSourceMenu_ = nullptr;
	QMenu *multiviewMenu_ = nullptr;
	QMenu *programOutMenu_ = nullptr;
	QMenu *previewOutMenu_ = nullptr;
	QAction *modeAction_ = nullptr;
	bool pro_ = false;

	QList<QPointer<ProjectorWindow>> projectors_;
};

} // namespace ls
