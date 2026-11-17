// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// Round, quiet icon button: the glyph is drawn in the paper's ink and a soft ink-tinted disc
// appears on hover / press / checked. Does not take focus on click, so typing is never interrupted.
T.AbstractButton {
    id: control

    property string glyph
    property bool filledGlyph: false
    property color ink: Theme.chromeInk
    property string tip

    implicitWidth: 28
    implicitHeight: 28
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: control.tip

    background: Rectangle {
        radius: width / 2
        color: control.ink
        opacity: control.down ? 0.16 : control.checked ? 0.12 : control.hovered ? 0.08 : 0
        border.width: control.visualFocus ? 1.5 : 0
        border.color: control.ink
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            path: control.glyph
            filled: control.filledGlyph
            color: control.ink
            opacity: control.enabled ? 1 : 0.4
        }
    }

    Loader {
        active: control.hovered && control.tip.length > 0
        sourceComponent: PaperToolTip {
            parent: control
            text: control.tip
            visible: true
        }
    }
}
