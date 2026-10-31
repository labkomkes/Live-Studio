// Live Studio - pemetaan layar LED ala Resolume "Advanced Output": satu layar keluaran dengan ukuran bebas,
// dibagi menjadi beberapa slice; tiap slice menampilkan potongan dari satu scene (atau Program/Preview).
#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <mutex>

namespace ls {

struct LedSlice {
	QString name = "Slice";
	QString scene;       // "" = Program (live), "@preview" = Preview, selain itu nama scene
	double inX = 0, inY = 0, inW = 1, inH = 1; // potongan scene sumber (pecahan 0..1 dari kanvas)
	double outX = 0, outY = 0, outW = 1920, outH = 1080; // posisi di layar LED (piksel)
	int fit = 0;         // 0 = Regang, 1 = Muat (letterbox), 2 = Isi (potong tepi)
	bool enabled = true;
};

struct LedLayout {
	QString name = "Layar LED";
	int width = 1920, height = 1080;
	bool testPattern = false;
	QList<LedSlice> slices;

	QJsonObject toJson() const;
	static LedLayout fromJson(const QJsonObject &o);
};

class LedScreen : public QObject {
	Q_OBJECT
public:
	explicit LedScreen(QObject *parent = nullptr);

	LedLayout layout() const; // salinan aman dibaca dari thread render
	void setLayout(const LedLayout &l, bool persist = true);

	bool saveTo(const QString &path) const;
	bool loadFrom(const QString &path);
	static QString defaultPath();

	// Preset tata letak. `scenes` dipakai untuk mengisi slice berurutan.
	static LedLayout presetFull(int w, int h);
	static LedLayout presetColumns(int w, int h, int n, const QStringList &scenes);
	static LedLayout presetGrid(int w, int h, int cols, int rows, const QStringList &scenes);
	// Satu scene dibelah menjadi n potongan yang disusun menjadi satu gambar lebar di layar LED.
	static LedLayout presetSplitOne(int w, int h, int n, const QString &scene);

signals:
	void layoutChanged();

private:
	mutable std::mutex mtx_;
	LedLayout layout_;
};

} // namespace ls
