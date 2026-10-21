import QtQuick
import QtLocation
import QtPositioning

MapQuickItem {
    coordinate: QtPositioning.coordinate(44.4056, 8.9463)

    sourceItem: Rectangle {
        width: 12
        height: 12
        radius: 6
        color: "red"
    }

    anchorPoint.x: 6
    anchorPoint.y: 6
}