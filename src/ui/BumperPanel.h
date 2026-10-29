// Live Studio - panel bumper video webinar.
#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;

namespace ls {

class Bumper;
class SceneManager;

class BumperPanel : public QWidget {
	Q_OBJECT
public:
	BumperPanel(Bumper *bumper, SceneManager *sm, QWidget *parent = nullptr);
	void refreshScenes();

private:
	Bumper *bumper_;
	SceneManager *sm_;
	QLineEdit *path_;
	QComboBox *then_;
	QPushButton *play_;
	QPushButton *stop_;
};

} // namespace ls
