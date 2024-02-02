import QtQuick 2.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.12
// import Qt5Compat.GraphicalEffects
import qframeclient 1.0

Window {
	id: window
	visible: true
	width: 1024
	height: 768
	title: qsTr("QtFrameClient Demo")
	color: "#222222"

	FrameClient {
		id: frameClient
		clientName: "AleFrameClient"
		macAddress: "54:3A:D6:B9:2C:35"
		ipAddress:  "192.168.178.108"
		connected: true
		onConnectedChanged: {
			console.log("FRAMECLIENT CONNECTED: ", connected)
			if (connected) {
				getContentList();
				// deleteImage("MY_F0055");
			}
		}
		property var contentIdList: ([])

		function loadNextItem() {
			if (contentIdList.length>0) {
				getThumbnail(contentIdList.shift());
			}
		}

		onGotContentList: {
			// console.log("CONTENT: ", JSON.stringify(contentList));
			const contentIdSet = new Set();
			contentList.forEach((item) => { contentIdSet.add(item.content_id); });
			contentIdList = Array.from(contentIdSet);
			loadNextItem();
		}
		onGotThumbnail: { // (contentId, fileName)
			console.log("onGotThumbnail",contentId,fileName);
			//bditDialog.client.loadImage(fileName, "", frameClient.frameName, false);

			thumbModel.append({"contentId": contentId, "fileName": fileName});
			loadNextItem();
		}
	}

	GridView {
		id: thumbGrid
		anchors.fill: parent
		anchors.margins: 40
		clip: true
		model: ListModel { id: thumbModel; }
		cellWidth: width/5
		cellHeight: (cellWidth+20) * 9 / 16
		snapMode: GridView.SnapToRow
		delegate: Item {
			width: thumbGrid.cellWidth
			height: thumbGrid.cellHeight
			clip: true
			Item {
				id: thumbItem
				anchors.fill: parent
				anchors.margins: 10

				RectangularGlow {
					id: effect
					anchors.fill: parent
					glowRadius: 8
					spread: 0
					color: "#80000000"
					cornerRadius: 8
				}
				Image {
					id: thumbnailImage
					anchors.fill: parent
					source: "file://" + fileName
					fillMode: Image.PreserveAspectFit
				}
				MouseArea {
					anchors.fill: parent
					onClicked: {
						frameClient.selectImage(contentId); // "MY_F0007"
					}
				}
			}
		}
	}
}

