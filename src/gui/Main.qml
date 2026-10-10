import QtQuick
import QtQuick.Controls

ApplicationWindow {
    width: 800
    height: 600
    visible: true
    title: "Traffic Simulator"

    Button {
        text: "Exit"

        anchors.centerIn: parent

        width: 200
        height: 100

        font.pixelSize: 32

        onClicked: {
            Qt.quit()
        }
    }
}