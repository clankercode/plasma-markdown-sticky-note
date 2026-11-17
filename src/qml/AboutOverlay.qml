// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// A small card laid over the page with the version and shortcuts. Click anywhere or Escape to close.
FocusScope {
    id: root

    property Swatch swatch: Theme.yellow
    property bool shown: false

    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    onShownChanged: if (shown) root.forceActiveFocus()
    Keys.onEscapePressed: root.shown = false

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onClicked: root.shown = false
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: root.swatch.paper
        opacity: 0.97
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 300)
        spacing: 4

        Text {
            width: parent.width
            text: qsTr("Sticky Notes")
            font: Theme.titleFont
            color: root.swatch.ink
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            width: parent.width
            text: qsTr("Version %1. Markdown notes for your desktop, saved as plain files.").arg(App.version)
            font: Theme.smallFont
            color: root.swatch.muted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Item { width: 1; height: 10 }
        Grid {
            anchors.horizontalCenter: parent.horizontalCenter
            columns: 2
            columnSpacing: 14
            rowSpacing: 3
            Repeater {
                model: [
                    "Ctrl+N", qsTr("New note"),
                    "Ctrl+W", qsTr("Archive note"),
                    "Ctrl+Shift+P", qsTr("Pop out onto the desktop"),
                    "Ctrl+Tab", qsTr("Next note"),
                    "Ctrl+Shift+Tab", qsTr("Previous note"),
                    "Esc", qsTr("Stop editing"),
                    "Ctrl+Q", qsTr("Quit")
                ]
                delegate: Text {
                    required property string modelData
                    required property int index
                    text: modelData
                    font: index % 2 === 0 ? Theme.uiStrongFont : Theme.uiFont
                    color: index % 2 === 0 ? root.swatch.ink : root.swatch.muted
                }
            }
        }
        Item { width: 1; height: 10 }
        Text {
            width: parent.width
            text: qsTr("Notes live in %1").arg(App.store.notesDir)
            font: Theme.smallFont
            color: root.swatch.muted
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WrapAnywhere
        }
    }
}
