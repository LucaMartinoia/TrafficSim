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

    Theme {
        id: appTheme
    }

    // Apply the shared palette directly to Fusion controls
    palette.window: appTheme.window
    palette.windowText: appTheme.windowText
    palette.base: appTheme.base
    palette.alternateBase: appTheme.alternateBase
    palette.text: appTheme.text
    palette.button: appTheme.button
    palette.buttonText: appTheme.buttonText
    palette.highlight: appTheme.highlight
    palette.highlightedText: appTheme.highlightedText
    palette.placeholderText: appTheme.placeholderText

    Pane {
        anchors.fill: parent
        padding: 0
        background: null
        font.pixelSize: appTheme.fontSize

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Let the map use all space not needed by the bottom toolbar
            MapCanvas {
                id: mapMain

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 0
                backend: mainWindow.backend
                theme: appTheme
            }

            Toolbar {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                backend: mainWindow.backend
                mapCanvas: mapMain
                theme: appTheme
            }
        }
    }
}
