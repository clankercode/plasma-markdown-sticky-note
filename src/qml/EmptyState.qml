// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// Shown on the notebook page when there is nothing docked to show: a blank sheet that invites writing.
Item {
    id: root

    property Swatch swatch: Theme.yellow
    property bool allOnDesktop: false    // there are notes, but every one is popped out

    signal newNoteRequested()
    signal dragPressed()
    signal closeRequested()
    signal showArchivedRequested()

    MouseArea {
        anchors.fill: parent
        onPressed: (mouse) => {
            if (mouse.y < Theme.headerHeight)
                root.dragPressed()
        }
        onDoubleClicked: (mouse) => {
            if (mouse.y >= Theme.headerHeight)
                root.newNoteRequested()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 6
        y: Math.round((Theme.headerHeight - height) / 2)
        IconButton {
            glyph: Glyphs.archive
            ink: root.swatch.ink
            tip: qsTr("Show archived")
            onClicked: root.showArchivedRequested()
        }
        IconButton {
            glyph: Glyphs.close
            ink: root.swatch.ink
            tip: App.trayAvailable ? qsTr("Hide notebook") : qsTr("Close")
            onClicked: root.closeRequested()
        }
    }

    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: Theme.headerHeight / 4
        width: Math.min(parent.width - 48, 264)
        spacing: 8

        Text {
            width: parent.width
            text: root.allOnDesktop ? qsTr("All your notes are on the desktop.") : qsTr("A clean sheet.")
            font: Theme.titleFont
            color: root.swatch.ink
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            text: root.allOnDesktop
                  ? qsTr("Click a faded tab to find one, or start another here.")
                  : qsTr("Jot down a list, a reminder, a thought. Notes save as you type.")
            font: Theme.uiFont
            color: root.swatch.muted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            lineHeight: 1.15
        }
        Item { width: 1; height: 8 }
        PaperButton {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("New note")
            ink: root.swatch.ink
            fillInk: root.swatch.paper
            filled: true
            onClicked: root.newNoteRequested()
        }
        Text {
            width: parent.width
            text: qsTr("or press Ctrl+N")
            font: Theme.smallFont
            color: root.swatch.muted
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
