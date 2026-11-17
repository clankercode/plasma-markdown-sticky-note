// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// A row of paper chips. Picking one recolours the note immediately.
T.Popup {
    id: popup

    property Note note

    popupType: T.Popup.Window
    padding: 8
    margins: 6
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside

    implicitWidth: row.implicitWidth + leftPadding + rightPadding
    implicitHeight: row.implicitHeight + topPadding + bottomPadding

    background: Rectangle {
        radius: 8
        color: Theme.surface
        border.color: Theme.hairline
    }

    contentItem: Row {
        id: row
        spacing: 6

        Repeater {
            model: Theme.colorNames

            delegate: T.AbstractButton {
                id: chip

                required property string modelData
                readonly property Swatch swatch: Theme.swatch(chip.modelData)
                readonly property bool current: popup.note !== null && popup.note.color === chip.modelData

                implicitWidth: 28
                implicitHeight: 28
                hoverEnabled: true
                focusPolicy: Qt.TabFocus
                Accessible.name: Theme.colorLabel(chip.modelData)
                onClicked: {
                    if (popup.note)
                        popup.note.color = chip.modelData
                    popup.close()
                }

                background: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: chip.hovered || chip.current ? 26 : 22
                        height: width
                        radius: width / 2
                        color: chip.swatch.paper
                        border.width: 1
                        border.color: chip.swatch.tab
                        Behavior on width { NumberAnimation { duration: Theme.animFast } }
                    }
                    Rectangle {
                        anchors.centerIn: parent
                        width: 10
                        height: 10
                        radius: 5
                        color: chip.swatch.tab
                        visible: !chip.current
                    }
                    Icon {
                        anchors.centerIn: parent
                        visible: chip.current
                        path: Glyphs.check
                        color: chip.swatch.ink
                        strokeWidth: 1.8
                    }
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: "transparent"
                        border.width: 1.5
                        border.color: Theme.chromeInk
                        visible: chip.visualFocus
                    }
                }
            }
        }
    }
}
