import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning
import QtQuick.Window

Item {
    id: root

    required property var backend
    required property var mapCanvas

    implicitWidth: settingsButton.implicitWidth
    implicitHeight: settingsButton.implicitHeight

    function isValidNumber(text, minimum, maximum) {
        const trimmedText = text.trim();
        if (trimmedText.length === 0)
            return false;

        const value = Number(trimmedText);
        return Number.isFinite(value) && value >= minimum && value <= maximum;
    }

    Button {
        id: settingsButton

        text: "Settings"
        width: 100
        height: 40

        onClicked: settingsWindow.show()
    }

    ApplicationWindow {
        id: settingsWindow

        title: "Settings"
        width: 430
        height: 500
        minimumWidth: 430
        maximumWidth: 430
        minimumHeight: 500
        maximumHeight: 500
        modality: Qt.ApplicationModal
        flags: Qt.Dialog

        onVisibleChanged: {
            // Refresh the form whenever the settings window is opened
            if (visible && root.backend) {
                const mapView = root.backend.getSettings();
                if (mapView) {
                    latitudeField.text = Number(mapView.location.latitude).toFixed(6);
                    longitudeField.text = Number(mapView.location.longitude).toFixed(6);
                    zoomField.value = mapView.zoom;
                }
                statusLabel.text = "";
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            Label {
                text: "Default map view"
            }

            Label {
                text: "Latitude"
            }

            TextField {
                id: latitudeField

                Layout.fillWidth: true
                selectByMouse: true
                validator: DoubleValidator {
                    bottom: -90
                    top: 90
                    decimals: 8
                    notation: DoubleValidator.StandardNotation
                }
            }

            Label {
                text: "Longitude"
            }

            TextField {
                id: longitudeField

                Layout.fillWidth: true
                selectByMouse: true
                validator: DoubleValidator {
                    bottom: -180
                    top: 180
                    decimals: 8
                    notation: DoubleValidator.StandardNotation
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Label {
                    text: "Zoom level"
                }

                SpinBox {
                    id: zoomField

                    from: 0
                    to: 22
                    editable: true
                }
            }

            Button {
                text: "Use current map view"
                enabled: root.backend && root.mapCanvas.mapReady
                onClicked: {
                    // Copy the live map view into the editable defaults
                    latitudeField.text = Number(root.mapCanvas.centerLatitude).toFixed(6);
                    longitudeField.text = Number(root.mapCanvas.centerLongitude).toFixed(6);
                    zoomField.value = Math.round(root.mapCanvas.currentZoomLevel);
                    statusLabel.text = "Current view copied; apply to save it as the default.";
                }
            }

            Label {
                id: statusLabel

                Layout.fillWidth: true
                wrapMode: Text.Wrap
            }

            Item {
                Layout.fillHeight: true
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight

                Button {
                    text: "Clear cached OSM data"
                    enabled: !!root.backend
                    onClicked: {
                        // Ask the backend to clear OSM data and report the result
                        statusLabel.text = root.backend.clearCache() ? "Cached OSM data cleared." : "Could not clear cached OSM data.";
                    }
                }

                Button {
                    text: "Cancel"
                    onClicked: settingsWindow.close()
                }

                Button {
                    text: "Apply"
                    enabled: !!root.backend && root.isValidNumber(latitudeField.text, -90, 90) && root.isValidNumber(longitudeField.text, -180, 180)
                    onClicked: {
                        const center = QtPositioning.coordinate(Number(latitudeField.text), Number(longitudeField.text));
                        if (root.backend.saveSettings(center, zoomField.value)) {
                            statusLabel.text = "Default map view saved.";
                        } else {
                            statusLabel.text = "Could not save the default map view.";
                        }
                    }
                }
            }
        }
    }
}
