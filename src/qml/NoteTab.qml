// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// One index tab on the notebook's right edge. The tab's left part is tucked under the page; only the
// stub sticks out. The current note's tab is page-coloured and sticks out furthest, so it reads as
// part of the open sheet. Popped-out notes get a faded tab with a small arrow.
Item {
    id: tab

    required property Note note
    required property int index
    property bool current: false
    property bool dragging: false
    property real shift: 0              // make room for a tab being dragged past this one
    property bool animateShift: false
    property real dragOffset: 0

    readonly property Swatch swatch: Theme.swatch(tab.note.color)
    readonly property bool popped: tab.note.poppedOut

    signal activated()
    signal dragStarted()
    signal dragMoved(real centerY)    // dragged tab centre, in list content coordinates
    signal dragFinished()

    width: Theme.tabTuck + Theme.tabStub + 4
    height: Theme.tabHeight
    z: tab.dragging ? 2 : 0
    transform: Translate {
        y: tab.dragging ? tab.dragOffset : tab.shift
        Behavior on y {
            enabled: tab.animateShift && !tab.dragging
            NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
        }
    }

    Rectangle {
        id: face
        x: 0
        y: 1
        height: parent.height - 2
        width: Theme.tabTuck + (tab.current ? Theme.tabStub + 2 : (mouse.containsMouse || tab.dragging ? Theme.tabStub - 1 : Theme.tabStub - 5))
        topRightRadius: 6
        bottomRightRadius: 6
        color: tab.current ? tab.swatch.paper : tab.popped ? tab.swatch.faded : tab.swatch.strip
        Behavior on width { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }

        // A crisp outer edge so neighbouring tabs of the same colour stay distinct.
        Rectangle {
            anchors.fill: parent
            topRightRadius: 6
            bottomRightRadius: 6
            color: "transparent"
            border.width: 1
            border.color: tab.popped && !tab.current ? tab.swatch.strip : Qt.darker(tab.swatch.strip, tab.current ? 1.08 : 1.14)
        }
        // The current tab is cut from the open sheet: it carries the sheet's ink-coloured spine mark so
        // it stands apart even beside a tab of the same colour.
        Rectangle {
            visible: tab.current
            x: face.width - 5
            y: 8
            width: 2
            height: face.height - 16
            radius: 1
            color: tab.swatch.tab
        }

        // A rotated label reading top to bottom, like a binder index.
        Text {
            id: label
            // Only when a useful amount of title fits; a few letters and an ellipsis is just noise
            // (the colour and the tooltip identify short tabs).
            visible: face.height - y - 6 >= 58
            width: face.height - y - 6
            x: Theme.tabTuck + (face.width - Theme.tabTuck - height) / 2 + height
            y: tab.popped ? 22 : 6
            rotation: 90
            transformOrigin: Item.TopLeft
            text: tab.note.displayTitle
            font: Theme.smallFont
            color: tab.popped ? tab.swatch.muted : tab.swatch.ink
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignLeft
        }

        Icon {
            visible: tab.popped
            x: Theme.tabTuck + Math.round((face.width - Theme.tabTuck - width) / 2)
            y: label.visible ? 6 : Math.round((face.height - height) / 2)
            width: 16
            height: 16
            scale: 0.75
            path: Glyphs.popped
            color: tab.swatch.ink
            strokeWidth: 2
        }
    }

    MouseArea {
        id: mouse

        property real pressY: 0       // in the list's content coordinates (immune to our own transform)
        property bool moved: false

        x: Theme.tabTuck
        width: parent.width - Theme.tabTuck
        height: parent.height
        hoverEnabled: true
        cursorShape: tab.dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        preventStealing: true

        onPressed: (m) => {
            pressY = mouse.mapToItem(tab.parent, m.x, m.y).y
            moved = false
            tab.dragOffset = 0
        }
        onPositionChanged: (m) => {
            if (!pressed)
                return
            const y = mouse.mapToItem(tab.parent, m.x, m.y).y
            if (!moved && Math.abs(y - pressY) > 6) {
                moved = true
                tab.dragStarted()
            }
            if (moved) {
                tab.dragOffset = y - pressY
                tab.dragMoved(tab.y + tab.height / 2 + tab.dragOffset)
            }
        }
        onReleased: {
            if (moved)
                tab.dragFinished()
            else
                tab.activated()
            moved = false
        }
        onCanceled: {
            if (moved)
                tab.dragFinished()
            moved = false
        }
    }

    Loader {
        active: mouse.containsMouse && !mouse.pressed
        sourceComponent: PaperToolTip {
            parent: tab
            x: tab.width + 2
            y: Math.round((tab.height - implicitHeight) / 2)
            text: tab.popped ? qsTr("%1 (on the desktop)").arg(tab.note.displayTitle) : tab.note.displayTitle
            visible: true
        }
    }
}
