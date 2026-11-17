// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

T.MenuItem {
    id: control

    property bool destructive: false
    readonly property color ink: !enabled ? Theme.chromeMuted : destructive ? Theme.danger : Theme.chromeInk

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    font: Theme.uiFont
    leftPadding: 30
    rightPadding: 26
    topPadding: 7
    bottomPadding: 7

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.ink
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Icon {
        x: 9
        y: Math.round((control.height - height) / 2)
        visible: control.checkable && control.checked
        path: Glyphs.check
        color: control.ink
    }

    arrow: Icon {
        x: control.width - width - 8
        y: Math.round((control.height - height) / 2)
        visible: control.subMenu !== null
        path: Glyphs.chevron
        color: Theme.chromeMuted
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 30
        radius: 5
        color: control.destructive ? Theme.danger : Theme.chromeInk
        opacity: control.down ? 0.13 : control.highlighted ? 0.07 : 0
    }
}
