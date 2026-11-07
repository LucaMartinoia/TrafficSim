import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtPositioning
import QtQuick.Window

Item {
    id: root

    required property var backend
    required property var mapCanvas
    required property Theme theme

    implicitWidth: settingsButton.width
    implicitHeight: settingsButton.height

    function isValidNumber(text, minimum, maximum) {
        const trimmedText = text.trim();
        if (trimmedText.length === 0)
            return false;

        const value = Number(trimmedText);
        return Number.isFinite(value) && value >= minimum && value <= maximum;
    }

    StyledButton {
        id: settingsButton

        text: "Settings"
        Accessible.name: "Settings"
        icon.source: "qrc:/icons/gear-solid-full.svg"
        icon.width: 24
        icon.height: 24
        icon.color: palette.buttonText
        width: 144
        height: 48

        onClicked: settingsWindow.open()
    }

    Popup {
        id: settingsWindow

        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2.5)
        width: 430
        height: 460
        modal: true
        dim: true
        focus: true

        // Keep the settings window on the same Fusion palette
        palette.window: root.theme.window
        palette.windowText: root.theme.windowText
        palette.base: root.theme.base
        palette.alternateBase: root.theme.alternateBase
        palette.text: root.theme.text
        palette.button: root.theme.button
        palette.buttonText: root.theme.buttonText
        palette.highlight: root.theme.highlight
        palette.highlightedText: root.theme.highlightedText
        palette.placeholderText: root.theme.placeholderText

        onOpened: {
            // Refresh the form whenever the settings popup is opened
            if (root.backend) {
                const mapView = root.backend.getSettings();
                if (mapView) {
                    latitudeField.text = Number(mapView.location.latitude).toFixed(6);
                    longitudeField.text = Number(mapView.location.longitude).toFixed(6);
                    zoomField.value = mapView.zoom;
                }
                statusLabel.text = "";
            }
        }

        Pane {
            anchors.fill: parent
            padding: 0
            background: null
            font.pixelSize: root.theme.fontSize

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                Label {
                    text: "Settings"
                    font.bold: true
                }

                Label {
                    Layout.topMargin: 16
                    text: "Default map view:"
                }

                Label {
                    Layout.topMargin: 8
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
                    Layout.topMargin: 8

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

                StyledButton {
                    Layout.topMargin: 8
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
                    Layout.fillWidth: true

                    StyledButton {
                        text: "Clear cache"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        enabled: !!root.backend
                        onClicked: {
                            // Ask the backend to clear OSM data and report the result
                            statusLabel.text = root.backend.clearCache() ? "Cached OSM data cleared." : "Could not clear cached OSM data.";
                        }
                    }

                    StyledButton {
                        text: "Cancel"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        onClicked: settingsWindow.close()
                    }

                    StyledButton {
                        text: "Apply"
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        icon.source: "qrc:/icons/floppy-disk-solid-full.svg"
                        icon.color: palette.buttonText
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
}
