import QtQuick
import QtLocation

MapItemView {
    id: cars

    required property var backend

    model: backend.carsModel

    delegate: MapQuickItem {
        id: carItem

        required property var position
        required property real direction

        coordinate: position
        anchorPoint: Qt.point(6, 8)

        sourceItem: Item {
            width: 12
            height: 16

            Item {
                id: rotatingItem

                anchors.fill: parent
                transformOrigin: Item.Center
                rotation: carItem.direction

                Canvas {
                    anchors.fill: parent

                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        ctx.beginPath();
                        ctx.moveTo(width / 2, 0);
                        ctx.lineTo(width, height * 3 / 4);
                        ctx.lineTo(0, height * 3 / 4);
                        ctx.closePath();

                        ctx.fillStyle = "red";
                        ctx.fill();

                        ctx.strokeStyle = "white";
                        ctx.lineWidth = 1;
                        ctx.stroke();
                    }
                }
            }
        }
    }
}
