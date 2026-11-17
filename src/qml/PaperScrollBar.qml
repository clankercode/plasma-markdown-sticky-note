// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// Thin pencil-line scrollbar; widens slightly under the pointer. Hidden when there is nothing to scroll.
T.ScrollBar {
    id: control

    property color ink: Theme.chromeInk

    implicitWidth: 10
    implicitHeight: 10
    padding: 2
    minimumSize: 0.1
    visible: control.policy !== T.ScrollBar.AlwaysOff && control.size < 1.0
    hoverEnabled: true

    contentItem: Rectangle {
        implicitWidth: control.hovered || control.pressed ? 6 : 3
        implicitHeight: 3
        radius: width / 2
        color: control.ink
        opacity: control.pressed ? 0.45 : control.hovered ? 0.35 : control.active ? 0.22 : 0.12
        x: control.width - width - 2
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        Behavior on implicitWidth { NumberAnimation { duration: Theme.animFast } }
    }
}
