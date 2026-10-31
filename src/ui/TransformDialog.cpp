#include "TransformDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ls {

TransformDialog::TransformDialog(SceneManager *sm, const QString &scene, const QString &item, QWidget *parent)
	: QDialog(parent), sm_(sm), scene_(scene), item_(item)
{
	setWindowTitle("Transform — " + item);
	sm_->itemTransform(scene_, item_, &orig_);
	ratio_ = orig_.rect.h > 0 ? orig_.rect.w / orig_.rect.h : 1.f;

	auto *v = new QVBoxLayout(this);
	auto dbl = [this](double val, double lo, double hi) {
		auto *s = new QDoubleSpinBox(this);
		s->setRange(lo, hi);
		s->setDecimals(1);
		s->setValue(val);
		s->setSuffix(" px");
		return s;
	};
	auto form = new QFormLayout();
	x_ = dbl(orig_.rect.x, -20000, 20000);
	y_ = dbl(orig_.rect.y, -20000, 20000);
	w_ = dbl(orig_.rect.w, 1, 20000);
	h_ = dbl(orig_.rect.h, 1, 20000);
	rot_ = new QDoubleSpinBox(this);
	rot_->setRange(0, 359.9);
	rot_->setSuffix(" °");
	rot_->setValue(orig_.rot);
	aspect_ = new QCheckBox("Kunci rasio lebar:tinggi", this);
	aspect_->setChecked(true);
	form->addRow("Posisi X:", x_);
	form->addRow("Posisi Y:", y_);
	form->addRow("Lebar:", w_);
	form->addRow("Tinggi:", h_);
	form->addRow("", aspect_);
	form->addRow("Rotasi:", rot_);
	v->addLayout(form);

	v->addWidget(new QLabel("Crop (potong tepi sumber, dalam piksel sumber):", this));
	auto *cf = new QFormLayout();
	auto spin = [this](int val) {
		auto *s = new QSpinBox(this);
		s->setRange(0, 20000);
		s->setValue(val);
		return s;
	};
	cl_ = spin(orig_.cropL);
	ct_ = spin(orig_.cropT);
	cr_ = spin(orig_.cropR);
	cb_ = spin(orig_.cropB);
	cf->addRow("Kiri:", cl_);
	cf->addRow("Atas:", ct_);
	cf->addRow("Kanan:", cr_);
	cf->addRow("Bawah:", cb_);
	v->addLayout(cf);

	auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	auto *fit = bb->addButton("Muat layar", QDialogButtonBox::ActionRole);
	v->addWidget(bb);
	connect(fit, &QPushButton::clicked, this, [this] {
		sm_->itemOp(scene_, item_, SceneManager::ItemOp::Fit);
		SceneManager::ItemTransform t;
		if (sm_->itemTransform(scene_, item_, &t)) {
			loading_ = true;
			x_->setValue(t.rect.x);
			y_->setValue(t.rect.y);
			w_->setValue(t.rect.w);
			h_->setValue(t.rect.h);
			rot_->setValue(t.rot);
			loading_ = false;
		}
	});
	connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(bb, &QDialogButtonBox::rejected, this, [this] {
		sm_->setItemTransform(scene_, item_, orig_); // batal: pulihkan
		reject();
	});

	// Terapkan langsung saat diubah (hasil terlihat di Preview).
	connect(w_, &QDoubleSpinBox::valueChanged, this, [this](double nv) {
		if (loading_ || !aspect_->isChecked())
			return;
		QSignalBlocker b(h_);
		h_->setValue(nv / std::max(0.01f, ratio_));
		apply();
	});
	connect(h_, &QDoubleSpinBox::valueChanged, this, [this](double nv) {
		if (loading_ || !aspect_->isChecked())
			return;
		QSignalBlocker b(w_);
		w_->setValue(nv * ratio_);
		apply();
	});
	for (auto *s : {x_, y_, rot_})
		connect(s, &QDoubleSpinBox::valueChanged, this, [this] { apply(); });
	for (auto *s : {cl_, ct_, cr_, cb_})
		connect(s, &QSpinBox::valueChanged, this, [this] { apply(); });
	loading_ = false;
}

void TransformDialog::apply()
{
	if (loading_)
		return;
	SceneManager::ItemTransform t;
	t.rect = {float(x_->value()), float(y_->value()), float(w_->value()), float(h_->value())};
	t.rot = float(rot_->value());
	t.cropL = cl_->value();
	t.cropT = ct_->value();
	t.cropR = cr_->value();
	t.cropB = cb_->value();
	sm_->setItemTransform(scene_, item_, t);
}

} // namespace ls
