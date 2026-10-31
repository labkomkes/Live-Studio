#include "PluginsDialog.h"

#include "../core/ObsEngine.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QVBoxLayout>

#include <obs.h>

namespace ls {

static void collect(void *param, obs_module_t *m)
{
	auto *t = static_cast<QTableWidget *>(param);
	const int r = t->rowCount();
	t->insertRow(r);
	const char *name = obs_get_module_name(m);
	const char *desc = obs_get_module_description(m);
	const char *file = obs_get_module_file_name(m);
	t->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(name ? name : file ? file : "?")));
	t->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(desc ? desc : "")));
	t->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(file ? file : "")));
}

PluginsDialog::PluginsDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle("Plugin");
	resize(720, 520);
	auto *v = new QVBoxLayout(this);

	auto *table = new QTableWidget(0, 3, this);
	table->setObjectName("pluginTable");
	table->setHorizontalHeaderLabels({"Plugin", "Fungsi", "Berkas"});
	table->horizontalHeader()->setStretchLastSection(true);
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	obs_enum_modules(collect, table);
	table->sortItems(0);
	moduleCount_ = table->rowCount();
	v->addWidget(new QLabel(QString("%1 plugin termuat:").arg(moduleCount_), this));
	v->addWidget(table, 1);

	auto *denied = new QLabel("Sengaja tidak dimuat (butuh UI/Qt OBS): " + ObsEngine::deniedModules().join(", "), this);
	denied->setWordWrap(true);
	denied->setStyleSheet("color:#8a8a8a;");
	v->addWidget(denied);

	QSettings qs("LiveStudio", "LiveStudio");
	auto *row = new QHBoxLayout();
	row->addWidget(new QLabel("Folder plugin tambahan:", this));
	auto *dir = new QLineEdit(qs.value("plugins/extraDir").toString(), this);
	dir->setObjectName("extraPluginDir");
	auto *browse = new QPushButton("Pilih…", this);
	row->addWidget(dir, 1);
	row->addWidget(browse);
	v->addLayout(row);
	auto *warn = new QLabel("Isi folder dengan *.dll plugin OBS 64-bit (mis. plugin ASIO) dan subfolder data/<nama plugin>. "
				"Berlaku setelah Live Studio dibuka ulang. Plugin yang memakai Qt/UI OBS tidak cocok dan bisa gagal dimuat.",
				this);
	warn->setWordWrap(true);
	warn->setStyleSheet("color:#8a8a8a;");
	v->addWidget(warn);
	connect(browse, &QPushButton::clicked, this, [this, dir] {
		const QString d = QFileDialog::getExistingDirectory(this, "Folder plugin", dir->text());
		if (!d.isEmpty())
			dir->setText(d);
	});

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	v->addWidget(bb);
	connect(bb, &QDialogButtonBox::accepted, this, [this, dir] {
		QSettings s("LiveStudio", "LiveStudio");
		s.setValue("plugins/extraDir", dir->text().trimmed());
		accept();
	});
	connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

} // namespace ls
