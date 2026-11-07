import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: consolePanel

    // Receive the backend that publishes log messages
    required property var backend

    opacity: 0.8

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Label {
            text: "Console"
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            TextArea {
                id: logConsole

                readOnly: true
                selectByMouse: true
                wrapMode: TextArea.Wrap
                placeholderText: "Console output will appear here"

                Connections {
                    target: consolePanel.backend

                    function onLogMessage(severity, message) {
                        logConsole.append("[" + severity + "] " + message);
                    }
                }
            }
        }
    }
}
