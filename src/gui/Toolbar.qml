import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolBar {
    id: toolbar

    // Receive the shared app services from Main.qml
    required property var backend
    required property var mapCanvas

    // Map the normalized slider position evenly across the logarithmic range
    function targetScaleFromSlider(position) {
        return Math.pow(10, (position - 0.5) * 2);
    }

    Layout.preferredHeight: 65

    Item {
        anchors.fill: parent

        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            Label {
                text: "Route by:"
            }

            ComboBox {
                model: ["Distance", "Travel time", "Adaptive time"]
                currentIndex: 0
                Layout.preferredWidth: 150

                onActivated: {
                    if (!toolbar.backend.setRoutingMetric(currentText))
                        currentIndex = 0;
                }
            }

            Label {
                text: "Target scale: " + toolbar.targetScaleFromSlider(trafficScale.value).toFixed(1) + "x"
            }

            Slider {
                id: trafficScale

                from: 0.0
                to: 1.0
                value: 0.5
                stepSize: 0.01
                snapMode: Slider.SnapAlways
                Layout.preferredWidth: 160

                onMoved: {
                    if (!toolbar.backend.setTargetScale(toolbar.targetScaleFromSlider(value))) {
                        value = 0.5;
                    }
                }
            }
        }

        Button {
            checkable: true
            text: checked ? "Stop simulation" : "Start simulation"
            enabled: toolbar.mapCanvas.mapReady
            anchors.centerIn: parent
            width: 100
            height: 40

            onToggled: {
                toolbar.mapCanvas.interactionEnabled = !checked;
                toolbar.mapCanvas.borderWidth = checked ? 2 : 0;
                if (checked)
                    toolbar.mapCanvas.requestBBoxData();
                else
                    toolbar.backend.clearSimulation();
            }
        }

        Settings {
            backend: toolbar.backend
            mapCanvas: toolbar.mapCanvas
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
