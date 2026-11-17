// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// The archive drawer: archived notes, newest first, each with Restore and Delete.
Item {
    id: root

    property Swatch swatch: Theme.gray
    readonly property NoteListModel archived: App.store.archivedNotes
    property Note pendingDelete: null

    signal backRequested()
    signal dragPressed()
    signal openRequested(Note note)

    Keys.onEscapePressed: root.backRequested()
    Component.onCompleted: root.forceActiveFocus()

    // Header strip.
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.headerHeight
        onPressed: root.dragPressed()
    }
    IconButton {
        id: backButton
        x: 6
        y: Math.round((Theme.headerHeight - height) / 2)
        glyph: Glyphs.back
        ink: root.swatch.ink
        tip: qsTr("Back to notes")
        onClicked: root.backRequested()
    }
    Text {
        anchors.left: backButton.right
        anchors.leftMargin: 4
        anchors.verticalCenter: backButton.verticalCenter
        text: qsTr("Archive")
        font: Theme.titleFont
        color: root.swatch.ink
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: Theme.pad
        anchors.verticalCenter: backButton.verticalCenter
        text: root.archived.count === 1 ? qsTr("1 note") : qsTr("%1 notes").arg(root.archived.count)
        visible: root.archived.count > 0
        font: Theme.smallFont
        color: root.swatch.muted
    }

    ListView {
        id: list
        anchors.fill: parent
        anchors.topMargin: Theme.headerHeight + 4
        anchors.bottomMargin: Theme.fold
        clip: true
        model: root.archived
        boundsBehavior: Flickable.StopAtBounds
        reuseItems: true
        T.ScrollBar.vertical: PaperScrollBar { ink: root.swatch.ink }

        delegate: Item {
            id: row

            required property Note note
            readonly property Swatch noteSwatch: Theme.swatch(row.note.color)

            width: list.width
            height: 54

            HoverHandler { id: rowHover }

            Rectangle {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                radius: 6
                color: root.swatch.ink
                opacity: rowHover.hovered ? 0.06 : 0
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onDoubleClicked: root.openRequested(row.note)
            }

            // A small swatch of the note's paper, like a corner of the sheet.
            Rectangle {
                id: chip
                x: Theme.pad
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 26
                radius: 3
                color: row.noteSwatch.paper
                border.width: 1
                border.color: row.noteSwatch.tab
                Rectangle {
                    width: parent.width
                    height: 7
                    topLeftRadius: 3
                    topRightRadius: 3
                    color: row.noteSwatch.strip
                    border.width: 1
                    border.color: row.noteSwatch.tab
                }
            }

            Column {
                anchors.left: chip.right
                anchors.leftMargin: 10
                anchors.right: actions.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Text {
                    width: parent.width
                    text: row.note.displayTitle
                    font: Theme.uiStrongFont
                    color: root.swatch.ink
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: qsTr("Edited %1").arg(row.note.modified.toLocaleDateString(Qt.locale(), "d MMM yyyy"))
                    font: Theme.smallFont
                    color: root.swatch.muted
                    elide: Text.ElideRight
                }
            }

            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: Theme.pad - 4
                anchors.verticalCenter: parent.verticalCenter
                IconButton {
                    glyph: Glyphs.unarchive
                    ink: root.swatch.ink
                    tip: qsTr("Restore to notebook")
                    onClicked: App.store.unarchive(row.note)
                }
                IconButton {
                    glyph: Glyphs.trash
                    ink: Theme.danger
                    tip: qsTr("Delete…")
                    onClicked: {
                        root.pendingDelete = row.note
                        confirm.ask()
                    }
                }
            }
        }
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 48, 240)
        visible: root.archived.count === 0
        spacing: 4
        Text {
            width: parent.width
            text: qsTr("Nothing archived.")
            font: Theme.titleFont
            color: root.swatch.ink
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            width: parent.width
            text: qsTr("Archived notes wait here until you restore or delete them.")
            font: Theme.uiFont
            color: root.swatch.muted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }

    ConfirmOverlay {
        id: confirm
        anchors.fill: parent
        swatch: root.swatch
        message: qsTr("Delete “%1”?").arg(root.pendingDelete ? root.pendingDelete.displayTitle : "")
        detail: qsTr("The file moves to the .trash folder inside your notes folder.")
        onConfirmed: {
            if (root.pendingDelete)
                App.store.deleteNote(root.pendingDelete)
            root.pendingDelete = null
        }
    }
}
