#include "core/ObsEngine.h"
#include "core/SelfTest.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#ifndef LIVESTUDIO_VERSION
#define LIVESTUDIO_VERSION "0.0.0-dev"
#endif

static ls::VideoSettings videoFromPreset(const QString &preset)
{
	ls::VideoSettings vs; // default 1920x1080 @ 30
	if (preset.startsWith("1280x720")) {
		vs.baseW = vs.outW = 1280;
		vs.baseH = vs.outH = 720;
	}
	if (preset.contains("@ 60"))
		vs.fpsNum = 60;
	return vs;
}

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationName("Live Studio");
	QApplication::setOrganizationName("LiveStudio");
	QApplication::setApplicationVersion(LIVESTUDIO_VERSION);

	QFile qss(":/style.qss");
	if (qss.open(QIODevice::ReadOnly))
		app.setStyleSheet(QString::fromUtf8(qss.readAll()));

	QSettings settings("LiveStudio", "LiveStudio");
	const ls::VideoSettings vs = videoFromPreset(settings.value("video/preset").toString());

	QString error;
	if (!ls::ObsEngine::instance().start(vs, &error)) {
		QMessageBox::critical(nullptr, "Live Studio", "Mesin libobs gagal dimulai:\n\n" + error);
		return 1;
	}

	if (app.arguments().contains("--selftest")) {
		const int selfRc = ls::runSelfTest(!app.arguments().contains("--quiet"));
		ls::ObsEngine::instance().shutdown();
		return selfRc;
	}

	int rc = 0;
	{
		auto *window = new ls::MainWindow();
		window->show();
		// Flag pengembangan/CI: --autoquit=<ms> menutup jendela otomatis (menguji jalur shutdown).
		for (const QString &a : app.arguments()) {
			if (a.startsWith("--autoquit=")) {
				const int ms = a.mid(11).toInt();
				QTimer::singleShot(ms, window, &QWidget::close);
			}
		}
		rc = app.exec();
		delete window; // melepas semua display & reference sumber
	}

	ls::ObsEngine::instance().shutdown();
	return rc;
}
