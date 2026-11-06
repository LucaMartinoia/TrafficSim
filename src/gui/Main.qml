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

        Toolbar {
            Layout.fillWidth: true
            backend: mainWindow.backend
            mapCanvas: mapMain
        }

        Console {
            Layout.fillWidth: true
            backend: mainWindow.backend
        }
    }
}
