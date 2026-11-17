// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// The adhesive strip at the top of a note: colour dot, inline-editable title, and the note's actions.
// Empty strip space is a drag handle; the window decides what dragging means (dragPressed etc. carry
// header-local coordinates).
Item {
    id: root

    property Note note
    property Swatch swatch: Theme.yellow
    property bool desktop: false          // desktop note: dock + pin instead of pop-out + close
    property bool revealOnHover: false    // fade secondary buttons in only when the note is hovered
    property bool hovered: true
    property list<string> screenNames: []
    readonly property bool colorOpen: colorLoader.item !== null && (colorLoader.item as ColorPopup).opened
    readonly property bool menuOpen: menuLoader.item !== null && (menuLoader.item as PaperMenu).opened
    readonly property bool busy: root.colorOpen || root.menuOpen || title.activeFocus
    readonly property bool chromeShown: !root.revealOnHover || root.hovered || root.busy

    signal dragPressed(point pos)
    signal dragMoved(point pos)
    signal dragReleased()
    signal doubleClicked()
    signal titleAccepted()
    signal popOutRequested()
    signal dockRequested()
    signal archiveRequested()
    signal deleteRequested()
    signal closeRequested()
    signal showArchivedRequested()
    signal aboutRequested()
    signal moveToScreenRequested(string name)

    implicitHeight: Theme.headerHeight

    function editTitle(): void {
        title.forceActiveFocus()
        title.selectAll()
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: (mouse) => root.dragPressed(Qt.point(mouse.x, mouse.y))
        onPositionChanged: (mouse) => root.dragMoved(Qt.point(mouse.x, mouse.y))
        onReleased: root.dragReleased()
        onCanceled: root.dragReleased()
        onDoubleClicked: root.doubleClicked()
    }

    T.AbstractButton {
        id: dot
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        hoverEnabled: true
        focusPolicy: Qt.TabFocus
        Accessible.name: qsTr("Change colour")
        onClicked: {
            colorLoader.active = true
            const popup = colorLoader.item as ColorPopup
            if (popup)
                popup.opened ? popup.close() : popup.open()
        }

        background: Item {
            Rectangle {
                anchors.centerIn: parent
                width: dot.hovered || root.colorOpen ? 16 : 13
                height: width
                radius: width / 2
                color: root.swatch.tab
                border.width: 1
                border.color: Qt.darker(root.swatch.tab, 1.25)
                Behavior on width { NumberAnimation { duration: Theme.animFast } }
            }
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: "transparent"
                border.width: 1.5
                border.color: root.swatch.ink
                visible: dot.visualFocus
            }
        }

        Loader {
            id: colorLoader
            active: false
            sourceComponent: ColorPopup {
                parent: dot
                note: root.note
                y: dot.height + 2
            }
        }
    }

    TextInput {
        id: title
        anchors.left: dot.right
        anchors.leftMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        // Only as wide as its text, so the rest of the strip stays a drag handle.
        // implicitWidth is the natural text width and does not depend on width (no binding loop).
        width: Math.max(0, Math.min(Math.max(implicitWidth, title.length > 0 ? 0 : placeholder.implicitWidth) + 4,
                                    (root.chromeShown ? buttons.x : root.width) - x - 8))
        // Scroll only while editing; at rest a long title shows its beginning, not its end.
        autoScroll: activeFocus
        text: root.note ? root.note.title : ""
        font: Theme.titleFont
        // At rest the elided copy below is shown instead, so long titles end in "…" rather than a cut.
        color: title.activeFocus ? root.swatch.ink : "transparent"
        selectionColor: root.swatch.selection
        selectedTextColor: root.swatch.ink
        selectByMouse: true
        clip: true
        maximumLength: 200
        Accessible.name: qsTr("Note title")

        onTextEdited: if (root.note && root.note.title !== text) root.note.title = text
        onAccepted: root.titleAccepted()
        Keys.onEscapePressed: root.titleAccepted()

        HoverHandler {
            id: titleHover
            cursorShape: Qt.IBeamCursor
        }

        // A pencil line under the title: faint on hover (it is editable), firm while editing.
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.bottom
            anchors.topMargin: 1
            height: 1
            color: root.swatch.ink
            opacity: title.activeFocus ? 0.45 : titleHover.hovered ? 0.18 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
        }

        Text {
            id: placeholder
            anchors.verticalCenter: parent.verticalCenter
            width: title.width
            visible: !title.activeFocus || (title.length === 0 && !title.inputMethodComposing)
            // With no explicit title, the derived one is shown muted: it is what tabs and tooltips use.
            text: title.length > 0 ? title.text : root.note ? root.note.displayTitle : ""
            font: Theme.titleFont
            // On the desk a derived title is just the note's name; the muted hint is for the notebook.
            color: title.length > 0 || (root.desktop && !title.activeFocus) ? root.swatch.ink : root.swatch.muted
            elide: Text.ElideRight
        }
    }

    Row {
        id: buttons
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0
        opacity: root.chromeShown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }

        IconButton {
            visible: root.desktop
            glyph: Glyphs.pin
            ink: root.swatch.ink
            checkable: true
            checked: root.note !== null && root.note.pinned
            tip: checked ? qsTr("Unpin: let windows cover this note") : qsTr("Pin above windows")
            onToggled: if (root.note) root.note.pinned = checked
        }
        IconButton {
            glyph: root.desktop ? Glyphs.dock : Glyphs.popOut
            ink: root.swatch.ink
            tip: root.desktop ? qsTr("Put back in notebook") : qsTr("Pop out onto the desktop (Ctrl+Shift+P)")
            onClicked: root.desktop ? root.dockRequested() : root.popOutRequested()
        }
        IconButton {
            glyph: Glyphs.archive
            ink: root.swatch.ink
            tip: root.desktop ? qsTr("Archive") : qsTr("Archive (Ctrl+W)")
            onClicked: root.archiveRequested()
        }
        IconButton {
            id: moreButton
            glyph: Glyphs.more
            filledGlyph: true
            ink: root.swatch.ink
            tip: root.menuOpen ? "" : qsTr("More")
            checked: root.menuOpen
            onClicked: {
                menuLoader.active = true
                const menu = menuLoader.item as PaperMenu
                if (menu)
                    menu.opened ? menu.close() : menu.popup(moreButton, moreButton.width - menu.width, moreButton.height + 2)
            }
        }
        IconButton {
            visible: !root.desktop
            glyph: Glyphs.close
            ink: root.swatch.ink
            tip: App.trayAvailable ? qsTr("Hide notebook") : qsTr("Close")
            onClicked: root.closeRequested()
        }
    }

    Loader {
        id: menuLoader
        active: false
        sourceComponent: Component {
            PaperMenu {
                id: menu

                PaperMenu {
                    id: screenMenu
                    title: qsTr("Move to screen")
                    enabled: root.screenNames.length > 1
                }
                Instantiator {
                    active: root.desktop
                    model: root.desktop ? root.screenNames : []
                    delegate: PaperMenuItem {
                        required property string modelData
                        text: modelData
                        checkable: true
                        checked: root.note !== null && root.note.screen === modelData
                        onTriggered: root.moveToScreenRequested(modelData)
                    }
                    onObjectAdded: (index, object) => screenMenu.insertItem(index, object as PaperMenuItem)
                    onObjectRemoved: (index, object) => screenMenu.removeItem(object as PaperMenuItem)
                }
                PaperMenuSeparator { id: screenSeparator }
                PaperMenuItem {
                    text: qsTr("Show archived")
                    onTriggered: root.showArchivedRequested()
                }
                PaperMenuItem {
                    text: qsTr("Open notes folder")
                    onTriggered: App.openNotesFolder()
                }
                PaperMenuItem {
                    text: qsTr("Start on login")
                    checkable: true
                    checked: App.startOnLogin
                    onTriggered: App.startOnLogin = checked
                }
                PaperMenuItem {
                    text: qsTr("About Sticky Notes")
                    onTriggered: root.aboutRequested()
                }
                PaperMenuSeparator {}
                PaperMenuItem {
                    text: qsTr("Delete note…")
                    destructive: true
                    enabled: root.note !== null
                    onTriggered: root.deleteRequested()
                }
                PaperMenuItem {
                    text: qsTr("Quit")
                    onTriggered: App.quit()
                }

                // The screen submenu only makes sense on desktop notes.
                Component.onCompleted: {
                    if (!root.desktop) {
                        menu.removeMenu(screenMenu)
                        menu.removeItem(screenSeparator)
                    }
                }
            }
        }
    }
}
