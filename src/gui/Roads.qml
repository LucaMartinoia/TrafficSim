import QtQuick
import QtLocation

MapItemView {
    id: roads

    property var paths: []

    // Render every returned coordinate path as a separate road.
    model: paths

    delegate: MapPolyline {
        required property var modelData

        path: modelData
        line.width: 3
        line.color: "#1976d2"
    }
}
