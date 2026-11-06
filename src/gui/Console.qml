import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: consolePanel

    // Receive the backend that publishes log messages
    required property var backend

    Layout.preferredHeight: 150

    color: "#202020"

    ScrollView {
        anchors.fill: parent
        anchors.margins: 8

        TextArea {
            id: logConsole

            readOnly: true
            selectByMouse: true
            wrapMode: TextArea.Wrap

            font.family: "Consolas"
            font.pixelSize: 12
            color: "#dddddd"

            background: null

            Connections {
                target: consolePanel.backend

                function onLogMessage(severity, message) {
                    logConsole.append("[" + severity + "] " + message);
                }
            }
        }
    }
}
