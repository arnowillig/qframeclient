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
		onConnectedChanged: { if (connected) { getContentList(); } }
		property var downloadList: ([])

		function loadNextItem() {
			if (downloadList.length>0) {
				getThumbnail(downloadList.shift());
			}
		}

		onGotContentList: {
			// Filter duplicates from contentList
			const contentIdSet = new Set();
			contentList.forEach((item) => { contentIdSet.add(item.content_id); });
			downloadList = Array.from(contentIdSet);
			loadNextItem();
		}
		onGotThumbnail: { // (contentId, fileName)
			console.log("onGotThumbnail",contentId, fileName);
			thumbModel.append({"contentId": contentId, "fileName": fileName});
			loadNextItem();
		}
	}

	GridView {
		id: thumbGrid
		anchors.fill: parent
		anchors.margins: 10
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
					color: thumbItemMouseArea.pressed ? "#80ffffff" : "#80000000"
					cornerRadius: 8
				}
				Image {
					id: thumbnailImage
					anchors.fill: parent
					source: "file://" + fileName
					fillMode: Image.PreserveAspectFit
				}
				MouseArea {
					id: thumbItemMouseArea
					anchors.fill: parent
					onClicked: {
						frameClient.selectImage(contentId); // "MY_F0007"
					}
				}
			}
		}
	}
}

