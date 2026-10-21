
import QtQuick
import QtQuick.Layouts
import QtLocation
import QtPositioning

Item {
    id: root

    property bool interactionEnabled: true
    property int borderWidth: 0

    Layout.fillWidth: true
    Layout.fillHeight: true

    Rectangle {
        id: border

        anchors.fill: parent
        color: "transparent"
        border.color: "red"
        border.width: root.borderWidth
        z: 1
    }

    Plugin {
        id: mapPlugin
        name: "osm"

        PluginParameter {
            name: "osm.mapping.custom.host"
            value: "https://tile.openstreetmap.org/"
        }
    }

    MapView {
        id: mapView

        anchors.fill: parent
        enabled: root.interactionEnabled

        map.plugin: mapPlugin
        map.center: QtPositioning.coordinate(44.4056, 8.9463)
        map.zoomLevel: 20
        map.activeMapType: map.supportedMapTypes[map.supportedMapTypes.length - 1]

        Cars {
            parent: mapView.map
        }

    }
}
