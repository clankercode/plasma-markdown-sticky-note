// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// Text button for dialogs and the empty state. `ink` sets the tone; `filled` makes it the primary action.
T.Button {
    id: control

    property color ink: Theme.chromeInk
    property color fillInk: "#ffffff"
    property bool filled: false

    implicitWidth: Math.max(72, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    leftPadding: 14
    rightPadding: 14
    topPadding: 7
    bottomPadding: 7
    hoverEnabled: true
    font: Theme.uiStrongFont

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.filled ? control.fillInk : control.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 6
        color: control.filled ? control.ink : "transparent"
        border.width: control.filled ? 0 : 1
        border.color: Qt.alpha(control.ink, 0.3)
        opacity: control.down ? 0.8 : 1

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: control.filled ? "#ffffff" : control.ink
            opacity: control.hovered && !control.down ? (control.filled ? 0.1 : 0.07) : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 1.5
            border.color: control.ink
            visible: control.visualFocus
        }
    }
}
