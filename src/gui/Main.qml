import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow

    required property var backend

    width: 1000
    height: 700
    visible: true
    visibility: Window.Maximized

    title: "Traffic Simulator"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        MapCanvas {
            id: mapMain

            backend: mainWindow.backend
        }

        ToolBar {
            id: toolBar

            Layout.fillWidth: true
            Layout.preferredHeight: 65

            Item {
                anchors.fill: parent

                Button {
                    id: lockButton

                    checkable: true
                    text: checked ? "Unlock view" : "Lock view"
                    enabled: mapMain.mapReady
                    anchors.centerIn: parent
                    width: 100
                    height: 40

                    onToggled: {
                        mapMain.interactionEnabled = !checked;
                        mapMain.borderWidth = checked ? 2 : 0;
                        if (checked)
                            mapMain.requestBBoxData();
                        else
                            mainWindow.backend.clearSimulation();
                    }
                }

                Settings {
                    backend: mainWindow.backend
                    mapCanvas: mapMain
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
