// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// An in-note confirmation: the paper dims and a short question with two buttons appears in place.
// Lives inside the note (not a separate window) so it works the same on tiny desktop notes.
FocusScope {
    id: root

    property Swatch swatch: Theme.yellow
    property string message
    property string detail
    property string confirmText: qsTr("Delete")
    property bool shown: false

    signal confirmed()

    function ask(): void {
        root.shown = true
        cancelButton.forceActiveFocus()
    }
    function dismiss(): void {
        root.shown = false
    }

    visible: opacity > 0
    opacity: shown ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: Theme.animFast } }

    Keys.onEscapePressed: root.dismiss()

    // Swallow pointer input to the note underneath; a click on the backdrop cancels.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onClicked: root.dismiss()
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
        width: Math.min(parent.width - 24, 280)
        spacing: 6

        Text {
            width: parent.width
            text: root.message
            font: Theme.titleFont
            color: root.swatch.ink
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Text {
            width: parent.width
            visible: text.length > 0
            text: root.detail
            font: Theme.smallFont
            color: root.swatch.muted
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
        }
        Item { width: 1; height: 6 }
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            PaperButton {
                id: cancelButton
                text: qsTr("Keep")
                ink: root.swatch.ink
                onClicked: root.dismiss()
                Keys.onReturnPressed: root.dismiss()
            }
            PaperButton {
                text: root.confirmText
                ink: Theme.danger
                filled: true
                onClicked: {
                    root.dismiss()
                    root.confirmed()
                }
                Keys.onReturnPressed: clicked()
            }
        }
    }
}
