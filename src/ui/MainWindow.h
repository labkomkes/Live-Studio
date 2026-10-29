// Live Studio - jendela utama: Preview | CUT/TAKE | Program, dock scene/sumber/audio/lower third/bumper.
#pragma once

#include "ProjectorWindow.h"

#include <QList>
#include <QMainWindow>
#include <QPointer>

class QAction;
class QScreen;
class QCheckBox;
class QComboBox;
class QDockWidget;
class QLabel;
class QListWidget;
class QMenu;
class QPushButton;
class QSpinBox;

namespace ls {

class SceneManager;
class LowerThird;
class Bumper;
class ObsDisplayWidget;
class AudioMixerWidget;
class LowerThirdPanel;
class BumperPanel;
class ProjectorWindow;

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

	void buildUi();
	void buildDocks();
	void buildMenusAndToolbar();
	void connectSignals();
	void applyMode(bool pro);
	void refreshScenes();
	void refreshItems();
	void refreshOutputLabels();
	void refreshTransitionControls();
	void addSourceOfKind(SourceKind kind);
	void openProjector(int mode, QScreen *screen);
	void fillScreenMenu(QMenu *menu, int mode);
	void chooseVideoSettings();
	void showAbout();
	QString editedScene() const; // scene yang sedang diedit = Preview
	static QString friendlyType(const QString &typeId);

	SceneManager *sm_ = nullptr;
	LowerThird *lt_ = nullptr;
	Bumper *bumper_ = nullptr;

	ObsDisplayWidget *preview_ = nullptr;
	ObsDisplayWidget *program_ = nullptr;
	QLabel *previewLabel_ = nullptr;
	QLabel *programLabel_ = nullptr;
	QPushButton *cutBtn_ = nullptr;
	QPushButton *takeBtn_ = nullptr;
	QComboBox *transitionCombo_ = nullptr;
	QSpinBox *durationSpin_ = nullptr;
	QCheckBox *swapCheck_ = nullptr;

	QListWidget *scenesList_ = nullptr;
	QListWidget *sourcesList_ = nullptr;
	AudioMixerWidget *audio_ = nullptr;
	LowerThirdPanel *ltPanel_ = nullptr;
	BumperPanel *bumperPanel_ = nullptr;

	QDockWidget *scenesDock_ = nullptr;
	QDockWidget *sourcesDock_ = nullptr;
	QDockWidget *audioDock_ = nullptr;
	QDockWidget *ltDock_ = nullptr;
	QDockWidget *bumperDock_ = nullptr;

	QMenu *addSourceMenu_ = nullptr;
	QMenu *multiviewMenu_ = nullptr;
	QMenu *programOutMenu_ = nullptr;
	QMenu *previewOutMenu_ = nullptr;
	QAction *modeAction_ = nullptr;
	bool pro_ = false;

	QList<QPointer<ProjectorWindow>> projectors_;
};

} // namespace ls
