/*
 * qframeclient.cpp
 *
 * Description: Implementation for the QFrameClient class
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

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QThread>
#include <QUdpSocket>
#include <QtEndian>
#include <QWebSocket>

#define FRAME_API_URL				"http://%1:8001/api/v2/"
#define D2D_SERVICE_MESSAGE_EVENT		"d2d_service_message"
#define MS_CHANNEL_CONNECT_EVENT		"ms.channel.connect"
#define MS_CHANNEL_READY_EVENT			"ms.channel.ready"
#define MS_CHANNEL_UNAUTHORIZED			"ms.channel.unauthorized"
#define MS_ERROR_EVENT				"ms.error"
#define ED_APPS_LAUNCH_EVENT			"ed.apps.launch"
#define ED_EDENTV_UPDATE_EVENT			"ed.edenTV.update"
#define ED_INSTALLED_APP_EVENT			"ed.installedApp.get"
#define MS_CHANNEL_CLIENT_CONNECT_EVENT		"ms.channel.clientConnect"
#define MS_CHANNEL_CLIENT_DISCONNECT_EVENT	"ms.channel.clientDisconnect"
#define MS_VOICEAPP_HIDE_EVENT			"ms.voiceApp.hide"

#define FRAME_EVENT_ERROR			"error"
#define FRAME_EVENT_GO_TO_STANDBY		"go_to_standby"
#define FRAME_EVENT_IMAGE_ADDED			"image_added"
#define FRAME_EVENT_READY_TO_USE		"ready_to_use"
#define FRAME_EVENT_THUMBNAIL			"thumbnail"
#define FRAME_EVENT_IMAGE_SELECTED		"image_selected"
#define FRAME_EVENT_GET_PHOTO_FILTER_LIST	"get_photo_filter_list"
#define FRAME_EVENT_MATTE_LIST			"matte_list"
#define FRAME_EVENT_CONTENT_LIST		"content_list"
#define FRAME_EVENT_CURRENT_ARTWORK		"current_artwork"
#define FRAME_EVENT_API_VERSION			"api_version"
#define FRAME_EVENT_AUTO_ROTATION_IMAGE_CHANGED	"auto_rotation_image_changed"
#define FRAME_EVENT_GET_DEVICE_INFO		"get_device_info"
#define FRAME_EVENT_ART_MODE_CHANGED		"art_mode_changed"
#define FRAME_EVENT_ARTMODE_STATUS		"artmode_status"
#define FRAME_EVENT_FAVORITE_CHANGED		"favorite_changed"
#define FRAME_EVENT_IMAGE_LIST_DELETED		"image_list_deleted"


QFrameClient::QFrameClient(QObject *parent) : QObject{parent}
{
	_uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
	_manager = new QNetworkAccessManager(this);
	_websocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
	// connect(_websocket, &QWebSocket::connected,    this, [this]() { emit connectedChanged(true);  });
	connect(_websocket, &QWebSocket::disconnected, this, [this]() { emit connectedChanged(false); });
	connect(_websocket, &QWebSocket::textMessageReceived, this, &QFrameClient::messageReceived);
	QDir().mkpath(tempPath());
}

QFrameClient::~QFrameClient()
{
	disconnectFromFrame();
	delete _websocket;
	delete _manager;
}

void QFrameClient::connectToFrame()
{
	if (_connecting) {
		return;
	}
	if (ipAddress().isEmpty()) {
		_wantToConnect = true;
		return;
	}
	_connecting = true;
	sendWakeOnLanPacket();
	getRestApiInfo();

}

void QFrameClient::disconnectFromFrame()
{
	_connecting = false;
	_websocket->close();
}

bool QFrameClient::isConnected() const
{
	return _websocket->isValid();
}

void QFrameClient::setConnected(bool connected)
{
	if (connected && !isConnected()) {
		connectToFrame();
	} else if (!connected && isConnected()) {
		disconnectFromFrame();
	}
}

void QFrameClient::sendWakeOnLanPacket()
{
	QString macAddr = macAddress();
	macAddr.remove(QRegExp("[^A-Fa-f0-9]"));
	if (macAddr.length() != 12) {
		return;
	}
	QByteArray macBytes = QByteArray::fromHex(macAddr.toUtf8());
	QByteArray magicPacket(6, 0xff);
	for (int i = 0; i < 16; ++i) {
		magicPacket.append(macBytes);
	}
	QUdpSocket().writeDatagram(magicPacket, QHostAddress::Broadcast, 9);
}

void QFrameClient::getRestApiInfo()
{
	QNetworkReply* reply = _manager->get(QNetworkRequest(QUrl(QStringLiteral(FRAME_API_URL).arg(ipAddress()))));
	connect(reply, &QNetworkReply::finished,[this, reply]() {
		if (reply->error() != QNetworkReply::NoError) {
			qDebug("ERROR: '%s' '%s'", qPrintable(reply->errorString()), qPrintable(reply->request().url().toString()));
			_connecting = false;
			return;
		}
		QByteArray ba = reply->readAll();
		reply->close();
		reply->deleteLater();
		QVariantMap map = QJsonDocument::fromJson(ba).toVariant().toMap();
		QVariantMap supportMap = QJsonDocument::fromJson(map.value("isSupport").toString().toUtf8()).toVariant().toMap();
		_deviceInfo = map.value("device").toMap();
		_deviceInfo.insert("support", supportMap);
		_deviceInfo.insert("version", map.value("version").toString());
		qDebug("RestApiInfo: %s", QJsonDocument::fromVariant(_deviceInfo).toJson(QJsonDocument::Indented).constData());
		emit deviceInfoChanged();
		_websocket->open(QUrl(QString("ws://%1:8001/api/v2/channels/com.samsung.art-app?name=%2").arg(ipAddress(), clientName())));
	});
}

void QFrameClient::sendArtRequest(const QVariantMap& requestMap)
{
	if (!_websocket->isValid()) {
		return;
	}
	QVariantMap dataMap = requestMap;
	QVariantMap map;
	dataMap["id"] = _uuid;
	map["method"] = "ms.channel.emit";
	map["params"] = QVariantMap{ {"event", "art_app_request"}, {"to", "host"}, {"data", QJsonDocument::fromVariant(dataMap).toJson(QJsonDocument::Compact)}};
	QByteArray packet = QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact);
	// qDebug("Sending: %s", packet.constData());
	_websocket->sendTextMessage(packet);
}

// matte = "none", "shadowbox_black"
void QFrameClient::uploadImage(const QString& fileName, const QString& matte)
{
	QFile f(fileName);
	if (!f.open(QIODevice::ReadOnly)) {
		return;
	}
	_uploadData = f.readAll();
	f.close();
	int fileSize = _uploadData.size();

	QString date = QDateTime::currentDateTime().toString("yyyy:MM:dd hh:mm:ss");
	quint32 connId = QRandomGenerator::global()->bounded(std::numeric_limits<quint32>::min(), std::numeric_limits<quint32>::max());
	QVariantMap connInfo{{"d2d_mode", "socket"}, {"connection_id", connId}, {"id", _uuid}};
	sendArtRequest(QVariantMap{{"request", "send_image"},{"file_type", "jpg"},{"conn_info", connInfo}, {"image_date", date}, {"matte_id", matte}, {"file_size", fileSize}});
}

void QFrameClient::uploadImage(const QString& ip, quint16 port, const QString& key, const QByteArray& payload)
{
	QVariantMap hdMap;
	hdMap["num"]        = 0;
	hdMap["total"]      = 1;
	hdMap["fileLength"] = payload.size();
	hdMap["fileName"]   = "aleprint";
	hdMap["fileType"]   = "jpg";
	hdMap["secKey"]     = key;
	hdMap["versiom"]    = "0.0.1";
	QByteArray hdData = QJsonDocument::fromVariant(hdMap).toJson(QJsonDocument::Compact);
	quint32 headerLen = qToBigEndian(hdData.size());
	QByteArray data;
	data.append(QByteArray((const char*) &headerLen, 4));
	data.append(hdData);
	data.append(payload);
	QTcpSocket* sock = new QTcpSocket(this);
	connect(sock, &QTcpSocket::connected, sock, [sock, data]() { sock->write(data); sock->close(); });
	connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
	sock->connectToHost(ip, port);
}

QString QFrameClient::tempPath() const
{
	return QDir::tempPath() + "/qframeclient";
}

void QFrameClient::socketReadyRead()
{
	QTcpSocket* sock = qobject_cast<QTcpSocket*>(sender());
	QByteArray thumbData = sock->property("thumbData").toByteArray();
	thumbData.append(sock->readAll());
	sock->setProperty("thumbData", thumbData);
	quint32 headerLen = qFromBigEndian(*((quint32*)thumbData.left(4).constBegin()));
	int payloadStart = 4 + headerLen;
	if (thumbData.size() < payloadStart) {
		return;
	}
	QVariantMap headerMap = QJsonDocument::fromJson(thumbData.mid(4, headerLen)).toVariant().toMap();
	QString fileName = headerMap.value("fileName").toString();
	QString fileType = headerMap.value("fileType").toString();
	QString fileID = headerMap.value("fileID").toString();
	int fileLength = headerMap.value("fileLength").toInt();
	if (fileType=="jpeg") {
		fileType = "jpg";
	}

	int completeSize = payloadStart + fileLength;
	if (thumbData.size() < completeSize) {
		return;
	}
	// qDebug("headerMap: %s", QJsonDocument::fromVariant(headerMap).toJson().constData());
	QByteArray imgData = thumbData.mid(4 + headerLen, fileLength);

	QString imagePath = QString("%1/%2.%3").arg(tempPath(), fileID, fileType);
	QFile f(imagePath);
	if (f.open(QIODevice::WriteOnly)) {
		f.write(imgData);
		f.close();
	}
	sock->close();
	emit gotThumbnail(fileID, imagePath);
}

void QFrameClient::readThumbnail(const QString& ip, quint16 port)
{
	QTcpSocket* sock = new QTcpSocket(this);
	sock->setProperty("thumbData", QByteArray());
	connect(sock, &QTcpSocket::readyRead,    this, &QFrameClient::socketReadyRead);
	connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
	sock->connectToHost(ip, port);
}

void QFrameClient::deleteImage(const QString& contentId)
{
	sendArtRequest(QVariantMap{{"request", "delete_image_list"}, {"content_id_list", QVariantList{QVariantMap{{"content_id", contentId}}}}});
}

void QFrameClient::getThumbnail(const QString& contentId)
{
	quint32 connId = QRandomGenerator::global()->bounded(std::numeric_limits<quint32>::min(), std::numeric_limits<quint32>::max());
	QVariantMap connInfo{{"d2d_mode", "socket"}, {"connection_id", connId}, {"id", _uuid}};
	sendArtRequest(QVariantMap{{"request", "get_thumbnail"},{"content_id", contentId},{"conn_info", connInfo}});
}

void QFrameClient::getDeviceInfo()
{
	sendArtRequest({{"request", "get_device_info"}});
}

void QFrameClient::getApiVersion()
{
	sendArtRequest({{"request", "get_api_version"}});
}

void QFrameClient::getArtModeStatus()
{
	sendArtRequest({{"request", "get_artmode_status"}});
}

void QFrameClient::getContentList()
{
	sendArtRequest({{"request", "get_content_list"},{"category", "None"}});
}

void QFrameClient::getCurrentArtwork()
{
	sendArtRequest({{"request", "get_current_artwork"}});
}

void QFrameClient::selectImage(const QString& contentId, const QString& categoryId)
{
	sendArtRequest({{"request", "select_image"}, {"category_id", categoryId}, {"content_id", contentId}, {"show", true}});
}

void QFrameClient::changeMatte(const QString& contentId, const QString& matteId)
{
	sendArtRequest({{"request", "change_matte"}, {"content_id", contentId}, {"matte_id", matteId}});
}

void QFrameClient::getMatteList()
{
	sendArtRequest({{"request", "get_matte_list"}});
}

void QFrameClient::getPhotoFilterList()
{
	sendArtRequest({{"request", "get_photo_filter_list"}});
}

void QFrameClient::setArtModeStatus(bool artModeStatus)
{
	sendArtRequest({{"request", "set_artmode_status"},{"value", artModeStatus ? "on" : "off"}}); // "on", "off", "test"
}

void QFrameClient::messageReceived(const QString &message)
{
	QVariantMap map = QJsonDocument::fromJson(message.toUtf8()).toVariant().toMap();
	QString evt = map.value("event").toString();

	if (evt == MS_CHANNEL_CONNECT_EVENT) {
		qDebug("Frame Event: '%s'", qPrintable(evt));
		// qDebug("%s", QJsonDocument::fromVariant(map).toJson().constData());
	} else if (evt == MS_CHANNEL_READY_EVENT) {
		qDebug("Frame Event: '%s'", qPrintable(evt));
		getApiVersion();
		getDeviceInfo();
		getArtModeStatus();

		_connecting = false;
		emit connectedChanged(true);

	} else if (evt == D2D_SERVICE_MESSAGE_EVENT) {
		QVariantMap dataMap = QJsonDocument::fromJson(map.value("data").toString().toUtf8()).toVariant().toMap();
		QString evt = dataMap.value("event").toString();
		if (evt == FRAME_EVENT_ARTMODE_STATUS) {
			QString val = dataMap.value("value").toString();
			qDebug("Frame Event: gotArtModeStatus: %s", qPrintable(val));
			_artModeStatus = val=="on";
			emit artModeStatusChanged(_artModeStatus);
		} else if (evt == FRAME_EVENT_ART_MODE_CHANGED) {
			QString val = dataMap.value("status").toString();
			qDebug("Frame Event: art_mode_changed: %s", qPrintable(val));
			_artModeStatus = val=="on";
			emit artModeStatusChanged(_artModeStatus);
		} else if (evt == FRAME_EVENT_FAVORITE_CHANGED) {
			QString contentId = dataMap.value("content_id").toString();
			bool status = dataMap.value("status").toString()=="on";
			qDebug("Frame Event: favorite_changed: '%s' -> %d", qPrintable(contentId), status);
			emit favoriteChanged(contentId, status);
		} else if (evt == FRAME_EVENT_GET_DEVICE_INFO) {
			QVariantMap deviceInfo = dataMap;
			deviceInfo.remove("event");
			deviceInfo.remove("id");
			deviceInfo.remove("target_client_id");
			qDebug("Frame Event: gotDeviceInfo: %s", QJsonDocument::fromVariant(deviceInfo).toJson(QJsonDocument::Compact).constData());
			emit gotDeviceInfo(deviceInfo);
		} else if (evt == FRAME_EVENT_AUTO_ROTATION_IMAGE_CHANGED) {
			QString contentId = dataMap.value("current_content_id").toString();
			QString type = dataMap.value("type").toString();
			qDebug("Frame Event: auto_rotation_image_changed: '%s' '%s'", qPrintable(type), qPrintable(contentId));
		} else if (evt == FRAME_EVENT_API_VERSION) {
			QString apiVersion = dataMap.value("version").toString();
			qDebug("Frame Event: gotApiVersion: %s", qPrintable(apiVersion));
			emit gotApiVersion(apiVersion);
		} else if (evt == FRAME_EVENT_CURRENT_ARTWORK) {
			QString contentId = dataMap.value("content_id").toString();
			QString matteId = dataMap.value("matte_id").toString();
			QString portraitMatteId = dataMap.value("matte_id").toString();
			qDebug("Frame Event: current_artwork: '%s','%s','%s'", qPrintable(contentId), qPrintable(matteId), qPrintable(portraitMatteId));
		} else if (evt == FRAME_EVENT_CONTENT_LIST) {
			QVariantList contentList = QJsonDocument::fromJson(dataMap.value("content_list").toString().toUtf8()).toVariant().toList();
			// qDebug("Frame Event: content_list: %s", QJsonDocument::fromVariant(contentList).toJson().constData());
			emit gotContentList(contentList);
		} else if (evt == FRAME_EVENT_MATTE_LIST) {
			QVariantList matteList = QJsonDocument::fromJson(dataMap.value("matte_color_list").toString().toUtf8()).toVariant().toList();
			qDebug("Frame Event: matte_list: %s", QJsonDocument::fromVariant(matteList).toJson().constData());
		} else if (evt == FRAME_EVENT_GET_PHOTO_FILTER_LIST) {
			QVariantList filterList = QJsonDocument::fromJson(dataMap.value("filter_list").toString().toUtf8()).toVariant().toList();
			qDebug("Frame Event: filterList: %s", QJsonDocument::fromVariant(filterList).toJson().constData());
		} else if (evt == FRAME_EVENT_IMAGE_SELECTED) {
			QString contentId = dataMap.value("content_id").toString();
			QString matteId = dataMap.value("matte_id").toString();
			QString portraitMatteId = dataMap.value("matte_id").toString();
			QString isShown = dataMap.value("is_shown").toString();
			qDebug("Frame Event: image_selected: ContentId: '%s', matteId: '%s', portraitMatteId: '%s', isShown: '%s'", qPrintable(contentId), qPrintable(matteId), qPrintable(portraitMatteId), qPrintable(isShown));
		} else if (evt == FRAME_EVENT_THUMBNAIL) {
			QVariantMap connInfo = QJsonDocument::fromJson(dataMap.value("conn_info").toString().toUtf8()).toVariant().toMap();
			QString ip = connInfo.value("ip").toString();
			quint16 port = connInfo.value("port").toInt();
			readThumbnail(ip, port);
		} else if (evt == FRAME_EVENT_READY_TO_USE) {
			QVariantMap connInfo = QJsonDocument::fromJson(dataMap.value("conn_info").toString().toUtf8()).toVariant().toMap();
			QString ip   = connInfo.value("ip").toString();
			QString key  = connInfo.value("key").toString();
			quint16 port = connInfo.value("port").toInt();
			qDebug("Frame Event: ready_to_use: '%s:%d' SecKey: '%s'", qPrintable(ip), port, qPrintable(key));
			uploadImage(ip, port, key, _uploadData);
		} else if (evt == FRAME_EVENT_IMAGE_ADDED) {
			// qDebug("DATA: %s", QJsonDocument::fromVariant(dataMap).toJson().constData());
			QString categoryId = dataMap.value("category_id").toString();
			QString contentId = dataMap.value("content_id").toString();

			qDebug("Frame Event: image_added: '%s', '%s'", qPrintable(categoryId), qPrintable(contentId));
			if (categoryId.isEmpty()) {
				selectImage(contentId, categoryId);
				emit imageUploadFinished(contentId);
			}
		} else if (evt == FRAME_EVENT_IMAGE_LIST_DELETED) {
			qDebug("Frame Event:  image_list_deleted: %s", QJsonDocument::fromVariant(dataMap).toJson().constData());
			QVariantList list = QJsonDocument::fromVariant(dataMap.value("content_id_list").toString()).toVariant().toList();
			QStringList contentIds;
			for (const QVariant& v : list) {
				contentIds.append(v.toMap().value("content_id").toString());
			}
			emit imagesDeleted(contentIds);
		} else if (evt == FRAME_EVENT_ERROR) {
			QString errCode = dataMap.value("error_code").toString();
			QVariantMap reqData = QJsonDocument::fromJson(dataMap.value("request_data").toString().toUtf8()).toVariant().toMap();
			qDebug("Frame Event: error: '%s' %s", qPrintable(errCode), QJsonDocument::fromVariant(reqData).toJson().constData());
		} else if (evt == FRAME_EVENT_GO_TO_STANDBY) {
			qDebug("Frame Event: '%s'", qPrintable(evt));
			sendWakeOnLanPacket();
		} else {
			qDebug("Frame Event: '%s'", qPrintable(evt));
			qDebug("DATA: %s", QJsonDocument::fromVariant(dataMap).toJson().constData());
		}
	} else {
		qDebug("%s", QJsonDocument::fromVariant(map).toJson().constData());
	}

}

QString QFrameClient::macAddress() const
{
	return _macAddress;
}

void QFrameClient::setMacAddress(const QString& macAddress)
{
	if (_macAddress != macAddress) {
		_macAddress = macAddress;
		emit macAddressChanged();
	}
}

QString QFrameClient::ipAddress() const
{
	return _ipAddress;
}

void QFrameClient::setIpAddress(const QString& ipAddress)
{
	if (_ipAddress != ipAddress) {
		_ipAddress = ipAddress;
		emit ipAddressChanged();
		if (_wantToConnect) {
			_wantToConnect = false;
			connectToFrame();
		}
	}
}

QString QFrameClient::clientName() const
{
	return _clientName;
}

void QFrameClient::setClientName(const QString& clientName)
{
	if (_clientName != clientName) {
		_clientName = clientName;
		emit clientNameChanged();
	}
}

bool QFrameClient::artModeStatus() const
{
	return _artModeStatus;
}

QVariantMap QFrameClient::deviceInfo() const
{
	return _deviceInfo;
}

bool QFrameClient::hasFrameTVSupport() const
{
	return _deviceInfo.value("FrameTVSupport").toString() == "true";
}

QString QFrameClient::frameName() const
{
	return _deviceInfo.value("name").toString();
}
