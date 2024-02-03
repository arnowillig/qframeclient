/*
 * main.cpp
 *
 * Description: Demo for using the FrameClient component
 *
 * This file is part of qframeclient.
 *
 * qframeclient is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * qframeclient is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with qframeclient. If not, see <https://www.gnu.org/licenses/>.
 *
 * Author: Arno Willig
 * Email: akw@thinkwiki.org
 */

#include "qframeclient.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
	QGuiApplication app(argc, argv);

	QFrameClient::registerQml();

	QQmlApplicationEngine engine;

	engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
	if (engine.rootObjects().isEmpty()) {
		return -1;
	}

	return app.exec();
}
