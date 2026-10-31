// Live Studio - daftar plugin (modul libobs) yang termuat + folder plugin tambahan.
#pragma once

#include <QDialog>

namespace ls {

class PluginsDialog : public QDialog {
	Q_OBJECT
public:
	explicit PluginsDialog(QWidget *parent = nullptr);
	int moduleCount() const { return moduleCount_; }

private:
	int moduleCount_ = 0;
};

} // namespace ls
