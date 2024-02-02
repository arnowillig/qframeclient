import QtQuick 2.15
import QtQuick.Window 2.15
import QtGraphicalEffects 1.12
// import Qt5Compat.GraphicalEffects
import qframeclient 1.0

Window {
	id: window
	visible: true
	width: 1280
	height: 800
	title: qsTr("QtFrameClient Demo")
	color: "#222222"

	FrameClient {
		id: frameClient
		clientName: "FrameClient"
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
			// Filter duplicates out of contentList:
			thumbModel.clear();
			const contentIdSet = new Set();
			contentList.forEach((item) => { contentIdSet.add(item.content_id); });
			downloadList = Array.from(contentIdSet);
			loadNextItem();
		}
		onGotThumbnail: { // (contentId, fileName)
			// console.log("onGotThumbnail",contentId, fileName);
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
		cellWidth:  Math.floor(width / itemPerRow)
		cellHeight: Math.round((cellWidth + 2 * itemMargins) * 9 / 16)
		snapMode: GridView.SnapToRow

		property int itemPerRow: Math.max(1, Math.round(width / 320))
		property int itemMargins: 8
		property int pullToRefreshThreshold: -100

		displaced: Transition {
			NumberAnimation { properties: "x,y"; duration: 1000 }
		}
		remove: Transition {
			ParallelAnimation {
				NumberAnimation { property: "opacity"; to: 0; duration: 1000 }
				NumberAnimation { properties: "x"; to: -thumbGrid.cellWidth; duration: 1000 }
			}
		}

		onContentYChanged: { refreshItem.opacity = dragging && (contentY < -refreshItem.height) ? 1 : 0; }
		onDragEnded: { if (contentY < pullToRefreshThreshold) { frameClient.getContentList(); } }

		Rectangle {
			id: refreshItem
			anchors.horizontalCenter: parent.horizontalCenter
			width: 200
			height: 40
			radius: height/2
			visible: opactity>0
			opacity: 0
			gradient: Gradient {
				GradientStop {
					position: 0
					color: thumbGrid.contentY < thumbGrid.pullToRefreshThreshold ? "#0077b6" : "#90e0ef" // Darker blue to lighter blue
				}
				GradientStop {
					position: 1
					color: thumbGrid.contentY < thumbGrid.pullToRefreshThreshold ? "#023e8a" : "#caf0f8" // Even darker blue to very light blue
				}
			}
			Behavior on opacity { NumberAnimation {} }
			Text {
				anchors.centerIn: parent
				text: thumbGrid.contentY < thumbGrid.pullToRefreshThreshold ? "Release to refresh..." : "Pull to refresh..."
				color: thumbGrid.contentY < thumbGrid.pullToRefreshThreshold ? "#ffffff" : "#222222"
			}
		}


		delegate: Item {
			width: thumbGrid.cellWidth
			height: thumbGrid.cellHeight
			clip: true
			Item {
				id: thumbItem
				anchors.fill: parent
				anchors.margins: thumbGrid.itemMargins

				RectangularGlow {
					id: effect
					anchors.centerIn: parent
					width: thumbnailImage.paintedWidth
					height: thumbnailImage.paintedHeight
					glowRadius: Math.min(thumbGrid.itemMargins, 8)
					spread: 0
					color: thumbItemMouseArea.pressed ? "#80ffffff" : "#80000000"
					cornerRadius: 4
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

				Rectangle {
					anchors.right: effect.right
					anchors.top: effect.top
					anchors.margins: 4
					color: deleteButtonMouseArea.pressed ? "#aa0000" :  "#ff0000"
					width: 24
					height: width
					radius: width/2
					Rectangle {
						anchors.centerIn: parent
						color: "#440000"
						width: parent.width - 8
						height: 4
					}
					MouseArea {
						id: deleteButtonMouseArea
						anchors.fill: parent
						onClicked: {
							frameClient.deleteImage(contentId);
							thumbModel.remove(index);
						}
					}
				}
			}
		}
	}
}

