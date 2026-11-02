#include "AddSourceDialog.h"

#include "../core/SceneManager.h"
#include "../core/SourceFactory.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace ls {

namespace {
struct CatInfo {
	const char *label;
	const char *glyph;
	const char *hint;
};
const CatInfo kCats[AddSourceDialog::Count] = {
	{"Kamera / Webcam", "◉", "Kamera USB, HDMI capture card, atau kamera virtual."},
	{"Layar (Monitor)", "▭", "Tangkap seluruh layar komputer."},
	{"Jendela Aplikasi", "❏", "Tangkap satu jendela aplikasi (mis. PowerPoint, browser)."},
	{"File Video", "▶", "Putar file video (mp4, mov, mkv, ...)."},
	{"Gambar", "▣", "Gambar latar, logo, poster (png, jpg, ...)."},
	{"Teks", "T", "Judul atau teks sederhana."},
	{"Input Audio", "♪", "Mikrofon / line-in: WASAPI, ASIO, DirectShow, audio per aplikasi."},
	{"Audio Desktop", "♫", "Suara komputer (apa yang keluar di speaker)."},
	{"Zoom", "Z", "Jendela rapat Zoom + audio Zoom, langsung jadi satu scene."},
};

QPixmap tileIcon(const QString &glyph, const QColor &bg)
{
	QPixmap pm(160, 90);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	p.setBrush(bg);
	p.setPen(Qt::NoPen);
	p.drawRoundedRect(pm.rect(), 4, 4);
	QFont f = p.font();
	f.setPixelSize(44);
	p.setFont(f);
	p.setPen(QColor(235, 235, 235));
	p.drawText(pm.rect(), Qt::AlignCenter, glyph);
	return pm;
}
} // namespace

int AddSourceDialog::categoryOf(const QString &typeId, const QString &name)
{
	if (name.startsWith("Zoom"))
		return Zoom;
	if (typeId == factory::webcamId())
		return Webcam;
	if (typeId == factory::monitorId())
		return Screen;
	if (typeId == factory::windowId())
		return Window;
	if (typeId == factory::mediaId())
		return Video;
	if (typeId == factory::imageId())
		return Image;
	if (typeId.startsWith("text_"))
		return Text;
	if (typeId.startsWith("wasapi_output"))
		return DesktopAudio;
	if (typeId.startsWith("wasapi_") || typeId.startsWith("asio") || typeId.startsWith("dshow_audio"))
		return Mic;
	return -1;
}

AddSourceDialog::AddSourceDialog(SceneManager *sm, QWidget *parent) : QDialog(parent), sm_(sm)
{
	setObjectName("addSourceDialog");
	setWindowTitle("Tambah Sumber");
	resize(900, 560);

	auto *root = new QVBoxLayout(this);
	auto *row = new QHBoxLayout;
	root->addLayout(row, 1);

	cats_ = new QListWidget(this);
	cats_->setObjectName("addCatList");
	cats_->setFixedWidth(230);
	cats_->addItem("☆  Baru Dibuat");
	for (const CatInfo &c : kCats)
		cats_->addItem(QString("%1  %2").arg(c.glyph, c.label));
	row->addWidget(cats_);

	auto *right = new QVBoxLayout;
	row->addLayout(right, 1);
	title_ = new QLabel(this);
	title_->setObjectName("addTitle");
	hint_ = new QLabel(this);
	hint_->setObjectName("addHint");
	hint_->setWordWrap(true);
	tiles_ = new QListWidget(this);
	tiles_->setObjectName("addTileList");
	tiles_->setViewMode(QListView::IconMode);
	tiles_->setIconSize(QSize(160, 90));
	tiles_->setGridSize(QSize(176, 128));
	tiles_->setResizeMode(QListView::Adjust);
	tiles_->setMovement(QListView::Static);
	tiles_->setWordWrap(true);
	tiles_->setSpacing(4);
	right->addWidget(title_);
	right->addWidget(hint_);
	right->addWidget(tiles_, 1);

	auto *bottom = new QHBoxLayout;
	addExistingBtn_ = new QPushButton("Tambah yang sudah ada", this);
	addExistingBtn_->setObjectName("addExistingBtn");
	addExistingBtn_->setEnabled(false);
	auto *close = new QPushButton("Tutup", this);
	bottom->addWidget(addExistingBtn_);
	bottom->addStretch(1);
	bottom->addWidget(close);
	right->addLayout(bottom);

	connect(cats_, &QListWidget::currentRowChanged, this, [this] { fillTiles(); });
	connect(tiles_, &QListWidget::itemSelectionChanged, this, [this] {
		auto *it = tiles_->currentItem();
		addExistingBtn_->setEnabled(it && it->data(Qt::UserRole).toInt() == 2);
	});
	connect(tiles_, &QListWidget::itemDoubleClicked, this, [this] { chooseCurrent(); });
	connect(addExistingBtn_, &QPushButton::clicked, this, [this] { chooseCurrent(); });
	connect(close, &QPushButton::clicked, this, &QDialog::reject);

	cats_->setCurrentRow(0);
}

void AddSourceDialog::fillTiles()
{
	tiles_->clear();
	const int row = cats_->currentRow(); // 0 = Baru Dibuat, 1.. = kategori
	const int cat = row - 1;
	title_->setText(row == 0 ? "Baru Dibuat" : kCats[cat].label);
	hint_->setText(row == 0 ? "Pilih sumber yang sudah pernah dibuat untuk dipakai lagi di scene ini."
				: QString(kCats[cat].hint) + "  Klik dua kali \"＋ Baru\" untuk membuat sumber baru.");

	if (row > 0) {
		auto *n = new QListWidgetItem(QIcon(tileIcon("＋", QColor(40, 70, 120))), QString("＋ Baru: ") + kCats[cat].label);
		n->setData(Qt::UserRole, 1);
		n->setData(Qt::UserRole + 1, cat);
		tiles_->addItem(n);
	}
	const auto list = sm_->allSources();
	for (auto it = list.crbegin(); it != list.crend(); ++it) { // terbaru dulu
		const int c = categoryOf(it->typeId, it->name);
		if (row > 0 && c != cat)
			continue;
		auto *t = new QListWidgetItem(QIcon(tileIcon(c >= 0 ? kCats[c].glyph : "■", QColor(48, 50, 58))), it->name);
		t->setData(Qt::UserRole, 2);
		t->setData(Qt::UserRole + 2, it->name);
		tiles_->addItem(t);
	}
	if (tiles_->count() > 0)
		tiles_->setCurrentRow(0);
}

void AddSourceDialog::chooseCurrent()
{
	auto *it = tiles_->currentItem();
	if (!it)
		return;
	if (it->data(Qt::UserRole).toInt() == 1) {
		newKind_ = it->data(Qt::UserRole + 1).toInt();
		existing_.clear();
	} else {
		newKind_ = -1;
		existing_ = it->data(Qt::UserRole + 2).toString();
	}
	accept();
}

} // namespace ls
