#include "LowerThirdPanel.h"

#include "../core/LowerThird.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

namespace ls {

LowerThirdPanel::LowerThirdPanel(LowerThird *lt, QWidget *parent) : QWidget(parent), lt_(lt)
{
	auto *lay = new QVBoxLayout(this);

	auto *form = new QFormLayout();
	name_ = new QLineEdit(this);
	name_->setPlaceholderText("Nama pembicara");
	title_ = new QLineEdit(this);
	title_->setPlaceholderText("Jabatan / institusi");
	autoHide_ = new QSpinBox(this);
	autoHide_->setRange(0, 120);
	autoHide_->setSuffix(" dtk");
	autoHide_->setSpecialValueText("Manual");
	autoHide_->setValue(8);
	form->addRow("Nama", name_);
	form->addRow("Jabatan", title_);
	form->addRow("Sembunyi otomatis", autoHide_);
	lay->addLayout(form);

	auto *colors = new QHBoxLayout();
	auto *barBtn = new QPushButton("Warna bar…", this);
	auto *txtBtn = new QPushButton("Warna teks…", this);
	colors->addWidget(barBtn);
	colors->addWidget(txtBtn);
	lay->addLayout(colors);

	toggleBtn_ = new QPushButton("TAMPILKAN", this);
	toggleBtn_->setObjectName("ltToggle");
	toggleBtn_->setMinimumHeight(36);
	lay->addWidget(toggleBtn_);

	presets_ = new QListWidget(this);
	presets_->setToolTip("Klik dua kali untuk memuat dan menayangkan");
	lay->addWidget(presets_, 1);

	auto *presetRow = new QHBoxLayout();
	auto *addBtn = new QPushButton("Simpan preset", this);
	auto *delBtn = new QPushButton("Hapus", this);
	presetRow->addWidget(addBtn);
	presetRow->addWidget(delBtn);
	lay->addLayout(presetRow);

	loadPresets();

	connect(toggleBtn_, &QPushButton::clicked, this, &LowerThirdPanel::toggle);
	connect(lt_, &LowerThird::visibilityChanged, this, [this](bool shown) {
		toggleBtn_->setText(shown ? "SEMBUNYIKAN" : "TAMPILKAN");
		toggleBtn_->setProperty("live", shown);
		toggleBtn_->style()->unpolish(toggleBtn_);
		toggleBtn_->style()->polish(toggleBtn_);
	});
	connect(barBtn, &QPushButton::clicked, this, [this] {
		const QColor c = QColorDialog::getColor(QColor(20, 90, 180), this, "Warna bar");
		if (c.isValid())
			lt_->setBarColor(c);
	});
	connect(txtBtn, &QPushButton::clicked, this, [this] {
		const QColor c = QColorDialog::getColor(Qt::white, this, "Warna teks");
		if (c.isValid())
			lt_->setTextColor(c);
	});
	connect(addBtn, &QPushButton::clicked, this, &LowerThirdPanel::addPreset);
	connect(delBtn, &QPushButton::clicked, this, [this] {
		delete presets_->takeItem(presets_->currentRow());
		savePresets();
	});
	connect(presets_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
		const QStringList parts = it->data(Qt::UserRole).toString().split('\n');
		name_->setText(parts.value(0));
		title_->setText(parts.value(1));
		applyFields();
		lt_->showFor(autoHide_->value());
	});
}

void LowerThirdPanel::applyFields()
{
	lt_->setContent(name_->text(), title_->text());
}

void LowerThirdPanel::toggle()
{
	if (lt_->isShown()) {
		lt_->hide();
		return;
	}
	if (!lt_->ensureItems()) {
		QMessageBox::warning(this, "Lower Third",
				     "Lower third tidak bisa dibuat: plugin sumber teks atau warna tidak termuat. "
				     "Periksa log di %APPDATA%\\LiveStudio\\logs.");
		return;
	}
	applyFields();
	lt_->showFor(autoHide_->value());
}

void LowerThirdPanel::addPreset()
{
	if (name_->text().trimmed().isEmpty())
		return;
	auto *it = new QListWidgetItem(QString("%1 — %2").arg(name_->text(), title_->text()), presets_);
	it->setData(Qt::UserRole, name_->text() + "\n" + title_->text());
	savePresets();
}

void LowerThirdPanel::savePresets() const
{
	QSettings s("LiveStudio", "LiveStudio");
	s.beginWriteArray("lowerThirdPresets");
	for (int i = 0; i < presets_->count(); ++i) {
		s.setArrayIndex(i);
		s.setValue("data", presets_->item(i)->data(Qt::UserRole));
		s.setValue("label", presets_->item(i)->text());
	}
	s.endArray();
}

void LowerThirdPanel::loadPresets()
{
	QSettings s("LiveStudio", "LiveStudio");
	const int n = s.beginReadArray("lowerThirdPresets");
	for (int i = 0; i < n; ++i) {
		s.setArrayIndex(i);
		auto *it = new QListWidgetItem(s.value("label").toString(), presets_);
		it->setData(Qt::UserRole, s.value("data"));
	}
	s.endArray();
}

} // namespace ls
