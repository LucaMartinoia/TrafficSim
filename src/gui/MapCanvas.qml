import QtQuick
import QtQuick.Layouts
import QtLocation
import QtPositioning

Item {
    id: root

    required property var backend
    property bool interactionEnabled: true
    property int borderWidth: 0
    readonly property bool mapReady: mapView.map.mapReady

    Layout.fillWidth: true
    Layout.fillHeight: true

    function requestBBoxData() {
        // Convert the visible map corners into a geographic bounding box.
        const topLeft = mapView.map.toCoordinate(Qt.point(0, 0), false);
        const bottomRight = mapView.map.toCoordinate(Qt.point(mapView.map.width, mapView.map.height), false);

        // Pad the visible bounds by five percent of each geographic span.
        const latitudePadding = (topLeft.latitude - bottomRight.latitude) * 0.05;
        const longitudePadding = (bottomRight.longitude - topLeft.longitude) * 0.05;
        const south = Math.max(-90, bottomRight.latitude - latitudePadding);
        const west = Math.max(-180, topLeft.longitude - longitudePadding);
        const north = Math.min(90, topLeft.latitude + latitudePadding);
        const east = Math.min(180, bottomRight.longitude + longitudePadding);

        // Let the backend forward the padded bounding box to the OSM client.
        root.backend.requestBBoxData(south, west, north, east);
    }

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
        map.zoomLevel: 18
        map.activeMapType: map.supportedMapTypes[map.supportedMapTypes.length - 1]

        Cars {
            id: cars

            parent: mapView.map
        }

        Roads {
            id: roads

            parent: mapView.map
        }
    }

    // Forward completed OSM geometry to the map overlay.
    Connections {
        target: root.backend

        function onRoadsReceived(paths) {
            roads.paths = paths;
        }
    }
}
