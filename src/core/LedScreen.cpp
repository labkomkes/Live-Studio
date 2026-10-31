#include "LedScreen.h"

#include "ObsEngine.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

namespace ls {

static QJsonArray arr4(double a, double b, double c, double d) { return QJsonArray{a, b, c, d}; }

QJsonObject LedLayout::toJson() const
{
	QJsonObject o;
	o["name"] = name;
	o["width"] = width;
	o["height"] = height;
	o["testPattern"] = testPattern;
	QJsonArray sl;
	for (const LedSlice &s : slices) {
		QJsonObject so;
		so["name"] = s.name;
		so["scene"] = s.scene;
		so["in"] = arr4(s.inX, s.inY, s.inW, s.inH);
		so["out"] = arr4(s.outX, s.outY, s.outW, s.outH);
		so["fit"] = s.fit;
		so["enabled"] = s.enabled;
		sl.append(so);
	}
	o["slices"] = sl;
	return o;
}

LedLayout LedLayout::fromJson(const QJsonObject &o)
{
	LedLayout l;
	l.name = o["name"].toString("Layar LED");
	l.width = std::clamp(o["width"].toInt(1920), 64, 16384);
	l.height = std::clamp(o["height"].toInt(1080), 64, 16384);
	l.testPattern = o["testPattern"].toBool(false);
	for (const QJsonValue &v : o["slices"].toArray()) {
		const QJsonObject so = v.toObject();
		LedSlice s;
		s.name = so["name"].toString("Slice");
		s.scene = so["scene"].toString();
		const QJsonArray in = so["in"].toArray(), out = so["out"].toArray();
		if (in.size() == 4) {
			s.inX = in[0].toDouble();
			s.inY = in[1].toDouble();
			s.inW = std::clamp(in[2].toDouble(1.0), 0.001, 1.0);
			s.inH = std::clamp(in[3].toDouble(1.0), 0.001, 1.0);
		}
		if (out.size() == 4) {
			s.outX = out[0].toDouble();
			s.outY = out[1].toDouble();
			s.outW = std::max(1.0, out[2].toDouble(1920));
			s.outH = std::max(1.0, out[3].toDouble(1080));
		}
		s.fit = std::clamp(so["fit"].toInt(0), 0, 2);
		s.enabled = so["enabled"].toBool(true);
		l.slices.append(s);
	}
	return l;
}

QString LedScreen::defaultPath()
{
	return ObsEngine::instance().configDir() + "/led-layout.json";
}

LedScreen::LedScreen(QObject *parent) : QObject(parent)
{
	layout_ = presetFull(1920, 1080);
	loadFrom(defaultPath());
}

LedLayout LedScreen::layout() const
{
	std::lock_guard<std::mutex> lock(mtx_);
	return layout_;
}

void LedScreen::setLayout(const LedLayout &l, bool persist)
{
	{
		std::lock_guard<std::mutex> lock(mtx_);
		layout_ = l;
	}
	if (persist)
		saveTo(defaultPath());
	emit layoutChanged();
}

bool LedScreen::saveTo(const QString &path) const
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(QJsonDocument(layout().toJson()).toJson(QJsonDocument::Indented));
	return true;
}

bool LedScreen::loadFrom(const QString &path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly))
		return false;
	QJsonParseError err;
	const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return false;
	LedLayout l = LedLayout::fromJson(doc.object());
	if (l.slices.isEmpty())
		return false;
	setLayout(l, false);
	return true;
}

static QString sceneAt(const QStringList &scenes, int i)
{
	return scenes.isEmpty() ? QString() : scenes.at(i % scenes.size());
}

LedLayout LedScreen::presetFull(int w, int h)
{
	LedLayout l;
	l.width = w;
	l.height = h;
	LedSlice s;
	s.name = "Layar penuh";
	s.outW = w;
	s.outH = h;
	l.slices.append(s);
	return l;
}

LedLayout LedScreen::presetColumns(int w, int h, int n, const QStringList &scenes)
{
	return presetGrid(w, h, n, 1, scenes);
}

LedLayout LedScreen::presetGrid(int w, int h, int cols, int rows, const QStringList &scenes)
{
	LedLayout l;
	l.width = w;
	l.height = h;
	cols = std::max(1, cols);
	rows = std::max(1, rows);
	const double cw = double(w) / cols, ch = double(h) / rows;
	int k = 0;
	for (int r = 0; r < rows; ++r)
		for (int c = 0; c < cols; ++c, ++k) {
			LedSlice s;
			s.name = QString("Bagian %1").arg(k + 1);
			s.scene = sceneAt(scenes, k);
			s.outX = std::round(c * cw);
			s.outY = std::round(r * ch);
			s.outW = std::round((c + 1) * cw) - s.outX;
			s.outH = std::round((r + 1) * ch) - s.outY;
			l.slices.append(s);
		}
	return l;
}

LedLayout LedScreen::presetSplitOne(int w, int h, int n, const QString &scene)
{
	LedLayout l;
	l.width = w;
	l.height = h;
	n = std::max(1, n);
	const double cw = double(w) / n;
	for (int i = 0; i < n; ++i) {
		LedSlice s;
		s.name = QString("Potongan %1").arg(i + 1);
		s.scene = scene;
		s.inX = double(i) / n;
		s.inW = 1.0 / n;
		s.outX = std::round(i * cw);
		s.outY = 0;
		s.outW = std::round((i + 1) * cw) - s.outX;
		s.outH = h;
		l.slices.append(s);
	}
	return l;
}

} // namespace ls
