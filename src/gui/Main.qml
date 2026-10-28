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

            background: Rectangle {
                color: '#700ea1ca'
            }

            RowLayout {
                anchors.fill: parent

                Button {
                    id: lockButton

                    checkable: true
                    text: checked ? "Unlock view" : "Lock view"
                    enabled: mapMain.mapReady
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 100
                    Layout.preferredHeight: 40

                    background: Rectangle {
                        color: lockButton.checked ? "red" : "gray"
                        radius: 5
                    }

                    contentItem: Text {
                        text: lockButton.text
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onToggled: {
                        mapMain.interactionEnabled = !checked;
                        mapMain.borderWidth = checked ? 2 : 0;
                        if (checked)
                            mapMain.requestBBoxData();
                    }
                }
            }
        }
    }
}
