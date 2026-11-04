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
    readonly property real centerLatitude: mapView.map.center.latitude
    readonly property real centerLongitude: mapView.map.center.longitude
    readonly property real currentZoomLevel: mapView.map.zoomLevel

    Layout.fillWidth: true
    Layout.fillHeight: true

    function requestBBoxData() {
        if (!root.backend)
            return;

        // Convert the visible map corners into a geographic bounding box.
        const topLeft = mapView.map.toCoordinate(Qt.point(0, 0), false);
        const bottomRight = mapView.map.toCoordinate(Qt.point(mapView.map.width, mapView.map.height), false);

        // Pad the visible bounds by five percent of each geographic span.
        if (!topLeft.isValid || !bottomRight.isValid) {
            console.warn("Cannot query roads for an invalid map view");
            return;
        }

        const latitudePadding = (topLeft.latitude - bottomRight.latitude) * 0.05;
        const longitudePadding = (bottomRight.longitude - topLeft.longitude) * 0.05;
        const south = Math.max(-90, bottomRight.latitude - latitudePadding);
        const west = Math.max(-180, topLeft.longitude - longitudePadding);
        const north = Math.min(90, topLeft.latitude + latitudePadding);
        const east = Math.min(180, bottomRight.longitude + longitudePadding);

        // Represent the padded bounds as the top-left and bottom-right coordinates
        const queryTopLeft = QtPositioning.coordinate(north, west);
        const queryBottomRight = QtPositioning.coordinate(south, east);
        root.backend.requestBBoxData(queryTopLeft, queryBottomRight);
    }

    function gotoDefaultMapView() {
        if (!root.backend || !mapView || !mapView.map)
            return;

        const defaults = root.backend.getSettings();
        if (!defaults)
            return;

        // Apply saved defaults only during initialization, not when settings change
        mapView.map.center = defaults.location;
        mapView.map.zoomLevel = defaults.zoom;
    }

    Component.onCompleted: root.gotoDefaultMapView()
    onBackendChanged: root.gotoDefaultMapView()

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

    // Forward completed OSM geometry to the map overlay
    Connections {
        target: root.backend

        function onRoadsReceived(paths) {
            roads.paths = paths;
        }

        function onCarsUpdated(positions) {
            cars.positions = positions;
        }
    }
}
