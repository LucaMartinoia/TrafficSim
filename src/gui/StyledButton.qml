import QtQuick
import QtQuick.Controls

Button {
    id: control

    background: Rectangle {
        implicitWidth: 100
        implicitHeight: 40
        radius: 6
        color: !control.enabled ? Qt.darker(control.palette.window, 1.1)
              : control.down ? Qt.darker(control.palette.button, 1.25)
              : control.hovered ? Qt.lighter(control.palette.button, 1.18)
              : control.palette.button
        border.color: control.activeFocus ? control.palette.highlight : Qt.lighter(control.palette.button, 1.3)
        border.width: control.activeFocus ? 2 : 1
    }
}
