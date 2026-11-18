// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// The notebook: one sheet of paper showing the current note, with index tabs down its right edge.
// Only the current note's body exists; switching tabs rebinds it.
Window {
    id: root

    property Note currentNote: null
    property bool showingArchive: false
    property Note pendingDelete: null
    readonly property bool modalShown: confirmDelete.shown || about.shown || saveFailure.shown
    property int lastIndex: 0            // where the current note sat, to pick a neighbour when it leaves

    // Tab drag state (index in activeNotes; -1 when idle).
    property int dragIndex: -1
    property int dropIndex: -1

    readonly property NoteListModel notes: App.store.activeNotes
    readonly property Swatch swatch: root.showingArchive ? Theme.gray
                                     : root.currentNote ? Theme.swatch(root.currentNote.color) : Theme.yellow
    readonly property int margin: root.visibility === Window.Maximized || root.visibility === Window.FullScreen ? 0 : Theme.shadowMargin
    readonly property int tabColumnWidth: Theme.tabStub + 6

    signal raiseDesktopNoteRequested(Note note)

    title: root.currentNote && !root.showingArchive
           ? qsTr("%1 — Sticky Notes").arg(root.currentNote.displayTitle) : qsTr("Sticky Notes")
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    minimumWidth: 300
    minimumHeight: 240
    width: App.notebookGeometry.width >= minimumWidth ? App.notebookGeometry.width : 440
    height: App.notebookGeometry.height >= minimumHeight ? App.notebookGeometry.height : 500
    visible: !App.startMinimized || !App.trayAvailable

    // ---- note selection -------------------------------------------------------------------------

    function select(note: Note): void {
        if (root.modalShown) return
        root.showingArchive = false
        if (root.currentNote === note)
            return
        root.leaveEditing()
        root.currentNote = note
        if (note) {
            root.lastIndex = root.notes.indexOf(note)
            App.currentNoteId = note.id
        }
    }

    // The nearest docked note to `index`, looking forward first (the note that slides into place).
    function neighbourOf(index: int): Note {
        const n = root.notes.count
        for (let i = Math.max(0, index); i < n; ++i) {
            const candidate = root.notes.get(i)
            if (candidate && !candidate.poppedOut && candidate !== root.currentNote)
                return candidate
        }
        for (let j = Math.min(index, n) - 1; j >= 0; --j) {
            const candidate = root.notes.get(j)
            if (candidate && !candidate.poppedOut && candidate !== root.currentNote)
                return candidate
        }
        return null
    }

    // Re-validates the current note after the list changed (archive, delete, external removal).
    function revalidate(): void {
        if (root.currentNote) {
            const index = root.notes.indexOf(root.currentNote)
            if (index >= 0 && !root.currentNote.poppedOut) {
                root.lastIndex = index
                return
            }
        }
        root.currentNote = null
        const next = root.neighbourOf(root.lastIndex)
        if (next)
            root.select(next)
    }

    function step(delta: int): void {
        if (root.modalShown) return
        const n = root.notes.count
        if (n === 0)
            return
        let index = root.currentNote ? root.notes.indexOf(root.currentNote) : -1
        for (let k = 0; k < n; ++k) {
            index = (index + delta + n) % n
            const candidate = root.notes.get(index)
            if (candidate && !candidate.poppedOut) {
                root.select(candidate)
                return
            }
        }
    }

    function newNote(): void {
        if (root.modalShown) return
        const note = App.store.createNote("")
        if (!note)
            return
        root.select(note)
        Qt.callLater(root.editCurrentBody)
    }

    function editCurrentBody(): void {
        if (root.currentNote && !root.showingArchive)
            body.edit(-1)
    }

    function leaveEditing(): void {
        body.leaveEdit()
    }

    function archiveCurrent(): void {
        if (!root.currentNote)
            return
        const note = root.currentNote
        root.lastIndex = root.notes.indexOf(note)
        root.select(root.neighbourOf(root.lastIndex + 1) ?? null)
        App.store.archive(note)
        root.revalidate()
    }

    function popOutCurrent(): void {
        if (!root.currentNote)
            return
        const note = root.currentNote
        root.lastIndex = root.notes.indexOf(note)
        root.leaveEditing()
        if (!App.desktopNotesVisible)
            App.desktopNotesVisible = true
        // A note that has never been on the desktop appears just beside the notebook. Only where window
        // positions are known (not Wayland); otherwise DesktopPlacement cascades it.
        if (note.x < 0 && Qt.platform.pluginName !== "wayland" && root.Screen.name.length > 0) {
            note.setGeometry(root.Screen.name,
                             root.x - root.Screen.virtualX + page.x + page.width + root.tabColumnWidth + 12,
                             root.y - root.Screen.virtualY + page.y + 24,
                             note.width, note.height)
        }
        note.poppedOut = true
        root.revalidate()
    }

    function deleteCurrent(): void {
        if (!root.currentNote)
            return
        const note = root.currentNote
        root.lastIndex = root.notes.indexOf(note)
        root.leaveEditing()
        App.store.deleteNote(note)
        root.revalidate()
    }

    function showAndRaise(): void {
        root.show()
        root.raise()
        root.requestActivate()
    }

    function toggle(): void {
        if (root.visible && root.active)
            root.hide()
        else
            root.showAndRaise()
    }

    function showArchive(): void {
        root.leaveEditing()
        root.showingArchive = true
    }

    function showAbout(): void {
        about.shown = true
    }

    function closeOrHide(): void {
        if (App.trayAvailable)
            root.hide()
        else
            App.quit()
    }

    Component.onCompleted: {
        const remembered = App.store.noteById(App.currentNoteId)
        if (remembered && !remembered.archived && !remembered.poppedOut)
            root.select(remembered)
        else
            root.revalidate()
        const g = App.notebookGeometry
        if (g.width > 0 && Qt.platform.pluginName !== "wayland") {
            root.x = g.x
            root.y = g.y
        }
    }

    onClosing: (close) => {
        if (App.trayAvailable) {
            close.accepted = false
            root.hide()
        } else {
            close.accepted = false
            App.quit()
        }
    }

    // Geometry is saved a moment after resizing / moving settles (the timer only runs after a change).
    onWidthChanged: geometrySaver.restart()
    onHeightChanged: geometrySaver.restart()
    onXChanged: geometrySaver.restart()
    onYChanged: geometrySaver.restart()
    function commitGeometry(): void {
        if (root.visibility !== Window.Maximized && root.visibility !== Window.FullScreen)
            App.notebookGeometry = Qt.rect(root.x, root.y, root.width, root.height)
        geometrySaver.stop()
    }
    Connections {
        target: App
        function onPreparingToQuit(): void { root.commitGeometry() }
        function onSaveErrorChanged(): void {
            if (App.saveError.length > 0) saveFailure.shown = true
            else saveFailure.shown = false
        }
    }
    Timer {
        id: geometrySaver
        interval: 500
        onTriggered: root.commitGeometry()
    }

    Connections {
        target: root.notes
        function onCountChanged(): void { Qt.callLater(root.revalidate) }
        function onRowsInserted(): void { Qt.callLater(root.revalidate) }
        function onRowsRemoved(): void { Qt.callLater(root.revalidate) }
        function onModelReset(): void { Qt.callLater(root.revalidate) }
    }
    Connections {
        target: root.currentNote
        function onPoppedOutChanged(): void { Qt.callLater(root.revalidate) }
        function onArchivedChanged(): void { Qt.callLater(root.revalidate) }
    }

    // ---- shortcuts ------------------------------------------------------------------------------

    Shortcut { sequence: "Ctrl+W"; enabled: !root.modalShown && root.currentNote !== null && !root.showingArchive; onActivated: root.archiveCurrent() }
    Shortcut { sequence: "Ctrl+Shift+P"; enabled: !root.modalShown && root.currentNote !== null && !root.showingArchive; onActivated: root.popOutCurrent() }
    Shortcut { enabled: !root.modalShown; sequences: ["Ctrl+Tab", "Ctrl+PgDown"]; onActivated: root.step(1) }
    Shortcut { enabled: !root.modalShown; sequences: ["Ctrl+Shift+Tab", "Ctrl+Backtab", "Ctrl+PgUp"]; onActivated: root.step(-1) }

    // ---- tabs (drawn first: they tuck under the page) ---------------------------------------------

    ListView {
        id: tabs
        enabled: !root.modalShown

        // Tabs shrink (down to tabMinHeight) so that every tab and the + tab fit beside the page;
        // past that the column scrolls with the wheel.
        // The + tab is pinned below the list (always reachable), so the list gets the rest.
        readonly property int available: page.height - Theme.headerHeight - Theme.fold - 6 - plusTab.height
        readonly property int tabHeight: Math.max(Theme.tabMinHeight, Math.min(Theme.tabHeight,
                Math.floor(tabs.available / Math.max(1, root.notes.count)) - Theme.tabSpacing))
        readonly property int stepSize: tabs.tabHeight + Theme.tabSpacing

        x: page.x + page.width - Theme.tabTuck
        y: page.y + Theme.headerHeight + 6
        width: Theme.tabTuck + root.tabColumnWidth
        height: Math.min(contentHeight, tabs.available)
        model: root.notes
        spacing: Theme.tabSpacing
        interactive: false
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 0
        reuseItems: true

        WheelHandler {
            onWheel: (event) => {
                const max = Math.max(0, tabs.contentHeight - tabs.height)
                tabs.contentY = Math.max(0, Math.min(max, tabs.contentY - event.angleDelta.y / 2))
            }
        }

        delegate: NoteTab {
            id: tabDelegate
            height: tabs.tabHeight
            current: tabDelegate.note === root.currentNote && !root.showingArchive
            dragging: root.dragIndex === tabDelegate.index
            animateShift: root.dragIndex >= 0
            shift: {
                if (root.dragIndex < 0 || tabDelegate.index === root.dragIndex)
                    return 0
                if (tabDelegate.index > root.dragIndex && tabDelegate.index <= root.dropIndex)
                    return -tabs.stepSize
                if (tabDelegate.index < root.dragIndex && tabDelegate.index >= root.dropIndex)
                    return tabs.stepSize
                return 0
            }
            onActivated: {
                if (tabDelegate.note.poppedOut)
                    root.raiseDesktopNoteRequested(tabDelegate.note)
                else
                    root.select(tabDelegate.note)
            }
            // Desktop notes live on the desktop layer (under windows) unless pinned; pinning is the
            // only way to bring one above other windows, so a double-click on its tab pins it.
            // Deliberately not a toggle: a double-click should never send a note behind everything.
            onDoubleClicked: {
                if (!tabDelegate.note.poppedOut)
                    return
                tabDelegate.note.pinned = true
                root.raiseDesktopNoteRequested(tabDelegate.note)
            }
            onDragStarted: {
                root.dragIndex = tabDelegate.index
                root.dropIndex = tabDelegate.index
            }
            onDragMoved: (centerY) => {
                root.dropIndex = Math.max(0, Math.min(root.notes.count - 1, Math.floor(centerY / tabs.stepSize)))
            }
            onDragFinished: {
                const from = root.dragIndex
                const to = root.dropIndex
                root.dragIndex = -1
                root.dropIndex = -1
                if (from >= 0 && to >= 0 && from !== to)
                    App.store.moveNote(tabDelegate.note, to)
            }
        }
    }

    // The + tab, pinned under the list.
    Item {
        id: plusTab
        enabled: !root.modalShown
        x: tabs.x
        y: tabs.y + tabs.height
        width: tabs.width
        height: Theme.tabSpacing + 34

        MouseArea {
            id: plusMouse
            x: Theme.tabTuck
            y: Theme.tabSpacing
            width: parent.width - Theme.tabTuck
            height: 34
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.newNote()
        }
        Rectangle {
            y: Theme.tabSpacing + 1
            height: 32
            width: Theme.tabTuck + (plusMouse.containsMouse ? Theme.tabStub - 1 : Theme.tabStub - 5)
            topRightRadius: 6
            bottomRightRadius: 6
            color: Theme.surface
            border.width: 1
            border.color: Theme.hairline
            Behavior on width { NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic } }
            Icon {
                x: Theme.tabTuck + Math.round((parent.width - Theme.tabTuck - width) / 2)
                anchors.verticalCenter: parent.verticalCenter
                width: 12
                height: 12
                scale: 0.8
                path: Glyphs.plus
                strokeWidth: 2
                color: Theme.chromeMuted
            }
        }
        Loader {
            active: plusMouse.containsMouse
            sourceComponent: PaperToolTip {
                parent: plusMouse
                x: plusMouse.width + 2
                y: Math.round((plusMouse.height - implicitHeight) / 2)
                text: qsTr("New note (Ctrl+N)")
                visible: true
            }
        }
    }

    // ---- the page -------------------------------------------------------------------------------

    Paper {
        id: page
        x: root.margin
        y: root.margin
        width: root.width - 2 * root.margin - root.tabColumnWidth
        height: root.height - 2 * root.margin
        swatch: root.swatch
    }

    Item {
        id: content
        anchors.fill: page

        NoteHeader {
            id: header
            visible: !root.showingArchive && root.currentNote !== null
            anchors.left: parent.left
            anchors.right: parent.right
            height: Theme.headerHeight
            note: root.currentNote
            swatch: root.swatch
            onDragPressed: root.startSystemMove()
            onDoubleClicked: root.visibility === Window.Maximized ? root.showNormal() : root.showMaximized()
            onTitleAccepted: root.editCurrentBody()
            onPopOutRequested: root.popOutCurrent()
            onArchiveRequested: root.archiveCurrent()
            onDeleteRequested: confirmDelete.ask()
            onCloseRequested: root.closeOrHide()
            onShowArchivedRequested: root.showArchive()
            onAboutRequested: root.showAbout()
        }

        NoteBody {
            id: body
            windowVisible: root.visible
            visible: !root.showingArchive && root.currentNote !== null
            anchors.fill: parent
            anchors.topMargin: Theme.headerHeight
            note: root.currentNote
            swatch: root.swatch
            focus: true
        }

        EmptyState {
            visible: !root.showingArchive && root.currentNote === null
            anchors.fill: parent
            swatch: root.swatch
            allOnDesktop: root.notes.count > 0
            onNewNoteRequested: root.newNote()
            onDragPressed: root.startSystemMove()
            onCloseRequested: root.closeOrHide()
            onShowArchivedRequested: root.showArchive()
        }

        Loader {
            anchors.fill: parent
            active: root.showingArchive
            sourceComponent: ArchiveView {
                swatch: root.swatch
                onBackRequested: root.showingArchive = false
                onDragPressed: root.startSystemMove()
                onOpenRequested: (note) => {
                    App.store.unarchive(note)
                    root.select(note)
                }
            }
        }

        ConfirmOverlay {
            id: confirmDelete
            objectName: "deleteConfirmation"
            anchors.fill: parent
            swatch: root.swatch
            onShownChanged: if (shown) root.pendingDelete = root.currentNote
            message: qsTr("Delete “%1”?").arg(root.pendingDelete ? root.pendingDelete.displayTitle : "")
            detail: qsTr("The file moves to the .trash folder inside your notes folder.")
            onConfirmed: {
                const target = root.pendingDelete
                root.pendingDelete = null
                if (target) App.store.deleteNote(target)
                root.revalidate()
            }
        }

        Rectangle {
            id: saveFailure
            property bool shown: false
            visible: shown
            anchors.fill: parent
            radius: Theme.radius
            color: root.swatch.paper
            z: 20
            MouseArea { anchors.fill: parent; acceptedButtons: Qt.AllButtons; onWheel: (event) => event.accepted = true }
            Column {
                anchors.centerIn: parent
                width: parent.width - 32
                spacing: 10
                Text {
                    width: parent.width
                    text: qsTr("Your changes have not been saved")
                    font: Theme.titleFont
                    color: root.swatch.ink
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: App.saveError
                    font: Theme.smallFont
                    color: root.swatch.ink
                    wrapMode: Text.Wrap
                }
                PaperButton { text: qsTr("Retry save"); ink: root.swatch.ink; onClicked: App.retrySave() }
                PaperButton { text: qsTr("Save recovery copy and quit"); ink: root.swatch.ink; onClicked: App.saveRecoveryAndQuit() }
                PaperButton { text: qsTr("Keep editing"); ink: root.swatch.ink; onClicked: saveFailure.shown = false }
                PaperButton { text: qsTr("Discard unsaved edits and quit…"); ink: Theme.danger; onClicked: discardEdits.ask() }
            }
            ConfirmOverlay {
                id: discardEdits
                anchors.fill: parent
                swatch: root.swatch
                message: qsTr("Discard all unsaved changes?")
                detail: qsTr("Edits that could not be saved will be lost.")
                confirmText: qsTr("Discard and quit")
                onConfirmed: App.discardAndQuit()
            }
        }

        AboutOverlay {
            id: about
            anchors.fill: parent
            swatch: root.swatch
        }
    }

    // ---- resize edges (in the shadow margin, overlapping the page edge slightly) ------------------

    ResizeHandle { edges: Qt.LeftEdge; x: 0; y: 16; width: page.x + 4; height: root.height - 32 }
    ResizeHandle { edges: Qt.RightEdge; x: root.width - 6; y: 16; width: 6; height: root.height - 32 }
    ResizeHandle { edges: Qt.TopEdge; x: 16; y: 0; width: root.width - 32; height: page.y + 3 }
    ResizeHandle { edges: Qt.BottomEdge; x: 16; y: root.height - page.y - 4; width: root.width - 32; height: page.y + 4 }
    ResizeHandle { edges: Qt.TopEdge | Qt.LeftEdge; x: 0; y: 0; width: 16; height: 16 }
    ResizeHandle { edges: Qt.TopEdge | Qt.RightEdge; x: root.width - 16; y: 0; width: 16; height: 16 }
    ResizeHandle { edges: Qt.BottomEdge | Qt.LeftEdge; x: 0; y: root.height - 16; width: 16; height: 16 }
    // The dog-eared corner of the page is the obvious grip.
    ResizeHandle {
        edges: Qt.BottomEdge | Qt.RightEdge
        x: page.x + page.width - Theme.fold
        y: page.y + page.height - Theme.fold
        width: root.width - x
        height: root.height - y
    }
}
