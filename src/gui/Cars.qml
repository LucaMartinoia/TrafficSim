import QtQuick
import QtLocation

MapItemView {
    id: cars

    property var positions: []

    model: positions

    delegate: MapQuickItem {
        required property var modelData

        coordinate: modelData
        anchorPoint.x: sourceItem.width / 2
        anchorPoint.y: sourceItem.height / 2
        sourceItem: Rectangle {
            width: 10
            height: 10
            radius: width / 2
            color: "red"
            border.color: "white"
            border.width: 1
        }
    }
}
