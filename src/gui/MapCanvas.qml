pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtLocation
import QtPositioning

Item {
    id: root

    required property var backend
    required property Theme theme
    property bool interactionEnabled: true
    property int borderWidth: 0
    property string addressSearchStatus: ""
    property string queuedGeocodeQuery: ""
    property string activeGeocodeQuery: ""
    property int activeCarCount: 0
    property real elapsedSeconds: 0
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

        // Apply saved defaults through the shared map navigation function
        root.gotoMapView(defaults.location, defaults.zoom);
    }

    function gotoMapView(center, zoomLevel) {
        // Ignore missing or invalid geocoding results
        if (!center || !center.isValid)
            return;

        mapView.map.center = center;
        mapView.map.zoomLevel = zoomLevel;
    }

    function formatElapsedTime(seconds) {
        const totalSeconds = Math.floor(seconds);
        const hours = Math.floor(totalSeconds / 3600);
        const minutes = Math.floor((totalSeconds % 3600) / 60);
        const remainingSeconds = totalSeconds % 60;
        const formattedMinutes = String(minutes).padStart(2, "0");
        const formattedSeconds = String(remainingSeconds).padStart(2, "0");

        if (hours > 0)
            return String(hours).padStart(2, "0") + ":" + formattedMinutes + ":" + formattedSeconds;

        return formattedMinutes + ":" + formattedSeconds;
    }

    function searchAddress() {
        // Submit the current address immediately when Enter or the search button is used
        const query = addressSearch.text.trim();
        if (query.length < 3)
            return;

        geocodeTimer.stop();
        root.addressSearchStatus = "Searching...";
        root.queueGeocode(query);
    }

    function queueGeocode(query) {
        // Keep only the newest request while a previous geocoding request is active
        if (geocodeModel.status === GeocodeModel.Loading && query === root.activeGeocodeQuery) {
            root.queuedGeocodeQuery = "";
        } else {
            root.queuedGeocodeQuery = query;
        }

        searchItems.clear();
        if (geocodeModel.status !== GeocodeModel.Loading)
            root.startQueuedGeocode();
    }

    function startQueuedGeocode() {
        // Start one request at a time to avoid resetting the model during an update
        if (geocodeModel.status === GeocodeModel.Loading || root.queuedGeocodeQuery.length === 0)
            return;

        root.activeGeocodeQuery = root.queuedGeocodeQuery;
        root.queuedGeocodeQuery = "";
        geocodeModel.query = root.activeGeocodeQuery;
        geocodeModel.update();
    }

    function scheduleAddressSuggestions() {
        // Geocode after three characters and wait briefly for the user to pause typing
        const query = addressSearch.text.trim();
        searchItems.clear();
        root.addressSearchStatus = "";
        if (query.length < 4) {
            root.queuedGeocodeQuery = "";
            geocodeTimer.stop();
            return;
        }

        geocodeTimer.restart();
    }

    function selectSearchItem(index) {
        // Center the map on the selected geocoding result
        const item = searchItems.get(index);
        const location = geocodeModel.get(item.resultIndex);
        root.gotoMapView(location.coordinate, mapView.map.zoomLevel);
        addressSearch.text = location.address.text || item.displayText;
        searchItems.clear();
        root.queuedGeocodeQuery = "";
        root.addressSearchStatus = "";
    }

    function showGeocodingResults() {
        // Display results only if they still match the text currently being edited
        if (root.activeGeocodeQuery !== addressSearch.text.trim())
            return;

        searchItems.clear();
        const resultCount = Math.min(geocodeModel.count, 4);
        for (let index = 0; index < resultCount; ++index) {
            const location = geocodeModel.get(index);
            const label = location.address.text || location.coordinate.latitude.toFixed(5) + ", " + location.coordinate.longitude.toFixed(5);
            searchItems.append({
                displayText: label,
                resultIndex: index
            });
        }

        root.addressSearchStatus = resultCount === 0 ? "No matching address found." : "";
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

        PluginParameter {
            name: "osm.useragent"
            value: "TrafficSim/0.1"
        }

        PluginParameter {
            // Use the plugin's built-in providers instead of querying its remote repository
            name: "osm.mapping.providersrepository.disabled"
            value: true
        }
    }

    GeocodeModel {
        id: geocodeModel

        plugin: mapPlugin
        autoUpdate: false
        limit: 4

        onStatusChanged: {
            if (status === GeocodeModel.Ready) {
                if (root.queuedGeocodeQuery.length > 0) {
                    root.startQueuedGeocode();
                    return;
                }

                root.showGeocodingResults();
            } else if (status === GeocodeModel.Error) {
                if (root.queuedGeocodeQuery.length > 0) {
                    root.startQueuedGeocode();
                    return;
                }

                if (root.activeGeocodeQuery === addressSearch.text.trim())
                    root.addressSearchStatus = errorString || "Address search failed.";
            }
        }
    }

    ListModel {
        id: searchItems
    }

    Timer {
        id: geocodeTimer

        interval: 300
        repeat: false
        onTriggered: root.queueGeocode(addressSearch.text.trim())
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

    // Observe road and car state forwarded through the backend
    Connections {
        target: root.backend

        function onRoadsReceived(paths) {
            roads.paths = paths;
        }

        function onCarsUpdated(positions) {
            cars.positions = positions;
        }

        function onSimulationStatus(activeCarCount, elapsedSeconds) {
            root.activeCarCount = activeCarCount;
            root.elapsedSeconds = elapsedSeconds;
        }
    }

    Rectangle {
        id: statusPanel

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.bottomMargin: 16
        width: statusRow.implicitWidth
        height: statusRow.implicitHeight
        z: 2

        color: "#CCFFFFFF"
        radius: 4

        RowLayout {
            id: statusRow

            spacing: 24

            Label {
                text: "Cars: " + root.activeCarCount
                color: root.theme.mapText
                font.bold: true
                padding: 8
            }

            Label {
                text: "Time: " + root.formatElapsedTime(root.elapsedSeconds)
                color: root.theme.mapText
                font.bold: true
                padding: 8
            }
        }
    }

    Item {
        id: addressSearchPanel

        // Keep address search controls locked with map interaction
        enabled: root.interactionEnabled
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 16
        anchors.topMargin: 16
        width: 360
        implicitHeight: searchColumn.implicitHeight + 16
        height: implicitHeight
        z: 2

        ColumnLayout {
            id: searchColumn

            anchors.fill: parent
            anchors.margins: 8
            spacing: 0

            RowLayout {
                id: searchRow

                Layout.fillWidth: true
                spacing: 0

                Item {
                    id: searchEntry

                    Layout.fillWidth: true
                    implicitHeight: addressSearch.implicitHeight + 10

                    TextField {
                        id: addressSearch

                        anchors.fill: parent
                        rightPadding: searchButton.width + 8
                        leftPadding: 8
                        placeholderText: "Search for an address"
                        selectByMouse: true

                        ToolTip.visible: root.addressSearchStatus.length > 0 && hovered
                        ToolTip.text: root.addressSearchStatus
                        ToolTip.delay: 300

                        onTextEdited: {
                            root.scheduleAddressSuggestions();
                        }
                        onAccepted: root.searchAddress()
                    }

                    ToolButton {
                        id: searchButton

                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        enabled: addressSearch.text.trim().length >= 3
                        Accessible.name: "Search address"
                        ToolTip.text: "Search address"
                        ToolTip.visible: hovered
                        icon.source: "qrc:/icons/circle-chevron-right-solid-full.svg"
                        icon.width: 26
                        icon.height: 26
                        icon.color: root.theme.controlIcon
                        background: null
                        onClicked: root.searchAddress()
                    }
                }
            }

            ListView {
                id: searchResults

                Layout.preferredWidth: searchEntry.width
                Layout.preferredHeight: Math.min(count, 4) * 40
                visible: count > 0
                clip: true
                model: searchItems
                spacing: 0

                delegate: ItemDelegate {
                    id: resultDelegate

                    required property string displayText
                    required property int resultIndex
                    required property int index

                    width: ListView.view.width
                    height: 40
                    text: displayText
                    highlighted: hovered
                    onClicked: root.selectSearchItem(resultDelegate.index)
                }
            }
        }
    }

    ToolButton {
        id: defaultViewButton

        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 16
        anchors.bottomMargin: 16
        width: 60
        height: 60
        z: 2
        Accessible.name: "Default view"
        ToolTip.text: "Default view"
        ToolTip.visible: hovered
        icon.source: "qrc:/icons/location-crosshairs-solid-full.svg"
        icon.width: 60
        icon.height: 60
        icon.color: root.theme.mapIcon
        background: null
        // Keep default navigation locked with map interaction
        enabled: !!root.backend && root.interactionEnabled

        onClicked: root.gotoDefaultMapView()
    }
}
