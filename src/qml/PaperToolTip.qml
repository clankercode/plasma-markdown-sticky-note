// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// Small index-card tooltip. Shown in its own popup window so it is never clipped by tiny notes.
T.ToolTip {
    id: control

    x: parent ? Math.round((parent.width - implicitWidth) / 2) : 0
    y: -implicitHeight - 4
    delay: 600
    timeout: 6000
    margins: 4
    padding: 0
    topPadding: 4
    bottomPadding: 4
    leftPadding: 8
    rightPadding: 8
    popupType: T.Popup.Window
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    implicitWidth: Math.min(320, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding

    contentItem: Text {
        text: control.text
        font: Theme.smallFont
        color: Theme.chromeInk
        wrapMode: Text.Wrap
        elide: Text.ElideRight
        maximumLineCount: 3
    }

    background: Rectangle {
        radius: 5
        color: Theme.surface
        border.color: Theme.hairline
    }
}
