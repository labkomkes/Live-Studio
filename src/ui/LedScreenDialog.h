// Live Studio - editor Layar LED (mapping): ukuran layar, slice, scene per slice, pola uji, preset.
#pragma once

#include "../core/LedScreen.h"

#include <QDialog>
#include <QPointer>

class QCheckBox;
class QComboBox;
class QSpinBox;
class QTableWidget;
class QScreen;

namespace ls {

class SceneManager;
class ObsDisplayWidget;
class LedScreenWindow;

class LedScreenDialog : public QDialog {
	Q_OBJECT
public:
	LedScreenDialog(SceneManager *sm, LedScreen *led, QWidget *parent = nullptr);
	~LedScreenDialog() override;

	LedScreenWindow *outputWindow() const { return window_.data(); }
	void openOn(QScreen *screen); // buka jendela keluaran di monitor
	void closeOutput();

signals:
	void outputChanged();

public:
	// Untuk uji / pemanggil luar.
	void rebuildTable();
	void applyPreset(int kind, int n);

private:
	void collectFromTable();
	void applySize();
	QStringList sceneChoices() const;

	SceneManager *sm_;
	LedScreen *led_;
	ObsDisplayWidget *preview_;
	QSpinBox *w_, *h_;
	QComboBox *sizeCombo_, *monitor_;
	QCheckBox *test_;
	QTableWidget *table_;
	QPointer<LedScreenWindow> window_;
	bool building_ = false;
};

} // namespace ls
