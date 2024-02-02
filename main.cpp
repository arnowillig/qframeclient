#include "qframeclient.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
	QGuiApplication app(argc, argv);
	qmlRegisterType<QFrameClient>("qframeclient", 1, 0, "FrameClient");

	QQmlApplicationEngine engine;

	engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
	if (engine.rootObjects().isEmpty()) {
		return -1;
	}

	return app.exec();
}
