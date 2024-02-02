/*
 * qframeclient.h
 *
 * Description: Header for the QFrameClient class
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

#ifndef FRAMECLIENT_H
#define FRAMECLIENT_H

#include <QObject>
#include <QVariantMap>

class QNetworkAccessManager;
class QWebSocket;

class QFrameClient : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString macAddress  READ macAddress    WRITE setMacAddress    NOTIFY macAddressChanged)
	Q_PROPERTY(QString ipAddress   READ ipAddress     WRITE setIpAddress     NOTIFY ipAddressChanged)
	Q_PROPERTY(QString clientName  READ clientName    WRITE setClientName    NOTIFY clientNameChanged)
	Q_PROPERTY(QString frameName   READ frameName                            NOTIFY deviceInfoChanged)
	Q_PROPERTY(bool connected      READ isConnected   WRITE setConnected     NOTIFY connectedChanged)
	Q_PROPERTY(bool artModeStatus  READ artModeStatus WRITE setArtModeStatus NOTIFY artModeStatusChanged)
	Q_PROPERTY(bool frameTVSupport READ hasFrameTVSupport                    NOTIFY deviceInfoChanged)
public:
	explicit QFrameClient(QObject *parent = nullptr);
	virtual ~QFrameClient();

	bool isConnected() const;
	void setConnected(bool connected);
	void getRestApiInfo();
	QString macAddress() const;
	void setMacAddress(const QString& macAddress);
	QString ipAddress() const;
	void setIpAddress(const QString& ipAddress);
	QString clientName() const;
	void setClientName(const QString& clientName);
	bool artModeStatus() const;
	QVariantMap deviceInfo() const;
	bool hasFrameTVSupport() const;
	QString frameName() const;
public slots:
	void connectToFrame();
	void disconnectFromFrame();
	void sendWakeOnLanPacket();
	void getApiVersion();
	void getDeviceInfo();
	void getArtModeStatus();
	void setArtModeStatus(bool artModeStatus);
	void getContentList();
	void getCurrentArtwork();
	void getMatteList();
	void getPhotoFilterList();
	void selectImage(const QString& contentId, const QString& categoryId=QString());
	void uploadImage(const QString& fileName, const QString& matte="none");
	void deleteImage(const QString& contentId);
	void getThumbnail(const QString& contentId);
	void changeMatte(const QString &contentId, const QString &matteId);
private slots:
	void messageReceived(const QString& message);
	void socketReadyRead();
	void sendArtRequest(const QVariantMap& dataMap);
signals:
	void macAddressChanged();
	void ipAddressChanged();
	void clientNameChanged();
	void connectedChanged(bool connected);
	void gotDeviceInfo(const QVariantMap& deviceInfo);
	void gotApiVersion(const QString& apiVersion);
	void artModeStatusChanged(bool artModeStatus);
	void deviceInfoChanged();
	void favoriteChanged(const QString& contentId, bool status);
	void imageUploadFinished(const QString& contentId);
	void gotContentList(const QVariantList& contentList);
	void gotThumbnail(const QString& contentId, const QString& fileName);
	void imagesDeleted(const QStringList& contentIdList);

private:
	void readThumbnail(const QString& ip, quint16 port);
	void uploadImage(const QString& ip, quint16 port, const QString& key, const QByteArray& payload);
	QString tempPath() const;

	QNetworkAccessManager* _manager = nullptr;
	QWebSocket* _websocket = nullptr;
	QByteArray _uploadData;
	QString _uuid;
	QString _macAddress;
	QString _ipAddress;
	QString _clientName;
	QVariantMap _deviceInfo;
	bool _artModeStatus = true;
	bool _connecting    = false;
	bool _wantToConnect = false;
};

#endif // FRAMECLIENT_H
