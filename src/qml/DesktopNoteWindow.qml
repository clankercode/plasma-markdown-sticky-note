// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// A note popped out onto the desktop: one sheet of paper, its own small frameless window.
// DesktopPlacement owns flags, size, position and layer; this file only draws and forwards input.
// The header's buttons stay hidden until the pointer is over the note, so a desk of notes stays calm.
Window {
    id: root

    required property Note note
    readonly property Swatch swatch: Theme.swatch(root.note.color)

    signal docked(Note note)
    signal showArchivedRequested()
    signal aboutRequested()

    function raiseNote(): void {
        placement.raise()
    }

    title: root.note.displayTitle
    color: "transparent"
    visible: placement.ready && App.desktopNotesVisible

    Shortcut { sequence: "Ctrl+W"; enabled: !confirmDelete.shown; onActivated: App.store.archive(root.note) }
    Shortcut {
        sequence: "Ctrl+Shift+P"
        enabled: !confirmDelete.shown
        onActivated: {
            const note = root.note
            note.poppedOut = false
            root.docked(note)
        }
    }

    DesktopPlacement {
        id: placement
        window: root
        note: root.note
    }

    // The dragged position is in window-local coordinates (the header maps its own into the window).
    function windowPoint(item: Item, p: point): point {
        return item.mapToItem(null, p.x, p.y)
    }

    HoverHandler { id: hover }

    Paper {
        id: page
        anchors.fill: parent
        anchors.topMargin: Theme.deskInsetTop
        anchors.leftMargin: Theme.deskInsetSide
        anchors.rightMargin: Theme.deskInsetSide
        anchors.bottomMargin: Theme.deskInsetBottom
        swatch: root.swatch
        lifted: placement.dragging
    }

    Item {
        anchors.fill: page

        NoteHeader {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.headerHeight
            note: root.note
            swatch: root.swatch
            desktop: true
            revealOnHover: true
            hovered: hover.hovered || placement.dragging
            screenNames: placement.screenNames
            onDragPressed: (p) => placement.beginDrag(root.windowPoint(header, p))
            onDragMoved: (p) => placement.dragTo(root.windowPoint(header, p))
            onDragReleased: placement.endDrag()
            onTitleAccepted: body.edit(-1)
            onDockRequested: {
                const note = root.note
                body.leaveEdit()
                note.poppedOut = false
                root.docked(note)
            }
            onArchiveRequested: App.store.archive(root.note)
            onDeleteRequested: confirmDelete.ask()
            onShowArchivedRequested: root.showArchivedRequested()
            onAboutRequested: root.aboutRequested()
            onMoveToScreenRequested: (name) => placement.moveToScreen(name)
        }

        NoteBody {
            id: body
            windowVisible: root.visible
            anchors.fill: parent
            anchors.topMargin: Theme.headerHeight
            note: root.note
            swatch: root.swatch
            focus: true
        }

        // Bottom-right dog-ear: the resize grip.
        MouseArea {
            id: grip
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Theme.fold + 6
            height: Theme.fold + 6
            cursorShape: Qt.SizeFDiagCursor
            onPressed: (m) => placement.beginResize(root.windowPoint(grip, Qt.point(m.x, m.y)))
            onPositionChanged: (m) => {
                if (pressed)
                    placement.resizeTo(root.windowPoint(grip, Qt.point(m.x, m.y)))
            }
            onReleased: placement.endResize()
            onCanceled: placement.endResize()
        }

        ConfirmOverlay {
            id: confirmDelete
            anchors.fill: parent
            swatch: root.swatch
            message: qsTr("Delete “%1”?").arg(root.note.displayTitle)
            detail: qsTr("It moves to the .trash folder.")
            onConfirmed: App.store.deleteNote(root.note)
        }
    }
}
