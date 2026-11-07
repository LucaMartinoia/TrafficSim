import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: toolbar

    // Receive the shared app services from Main.qml
    required property var backend
    required property var mapCanvas
    required property Theme theme

    // Map the normalized slider position evenly across the logarithmic range
    function targetScaleFromSlider(position) {
        return Math.pow(10, (position - 0.5) * 2);
    }

    Layout.preferredHeight: 200

    // Keep toolbar controls in three balanced sections
    RowLayout {
        id: bottomRow

        anchors.fill: parent
        anchors.margins: 12
        anchors.rightMargin: settingsControl.width + 24
        spacing: 0

        Item {
            Layout.preferredWidth: (bottomRow.width - toolbar.centerSectionWidth) / 2
            Layout.fillHeight: true

            Console {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                anchors.rightMargin: 16
                height: 180
                backend: toolbar.backend
            }
        }

        Item {
            Layout.preferredWidth: toolbar.centerSectionWidth
            Layout.fillHeight: true

            StyledButton {
                checkable: true
                text: checked ? "Stop simulation" : "Start simulation"
                enabled: toolbar.mapCanvas.mapReady
                anchors.centerIn: parent
                width: 160
                height: 48
                z: 1

                onToggled: {
                    toolbar.mapCanvas.interactionEnabled = !checked;
                    toolbar.mapCanvas.borderWidth = checked ? 2 : 0;
                    if (checked)
                        toolbar.mapCanvas.requestBBoxData();
                    else
                        toolbar.backend.clearSimulation();
                }
            }
        }

        Item {
            Layout.preferredWidth: (bottomRow.width - toolbar.centerSectionWidth) / 2
            Layout.fillHeight: true

            ColumnLayout {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 40
                spacing: 4

                Label {
                    text: "Route by:"
                    color: toolbar.theme.windowText
                    font.bold: true
                }

                ComboBox {
                    model: ["Distance", "Travel time", "Adaptive time"]
                    currentIndex: 0
                    Layout.preferredWidth: 200

                    onActivated: {
                        toolbar.backend.setRoutingMetric(currentText);
                    }
                }

                Label {
                    text: "Target scale: " + toolbar.targetScaleFromSlider(trafficScale.value).toFixed(1) + "x"
                    color: toolbar.theme.windowText
                    font.bold: true
                    Layout.topMargin: 16
                }

                Slider {
                    id: trafficScale

                    from: 0.0
                    to: 1.0
                    value: 0.5
                    stepSize: 0.01
                    snapMode: Slider.SnapAlways
                    Layout.preferredWidth: 200

                    onMoved: {
                        if (!toolbar.backend.setTargetScale(toolbar.targetScaleFromSlider(value))) {
                            value = 0.5;
                        }
                    }
                }
            }
        }
    }

    Settings {
        id: settingsControl

        backend: toolbar.backend
        mapCanvas: toolbar.mapCanvas
        theme: toolbar.theme
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.rightMargin: 16
    }

    readonly property real centerSectionWidth: 200
}
