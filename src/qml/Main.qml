// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// Root object: owns the notebook window and one desktop window per popped-out note, and answers
// requests from the tray and from second instances.
QtObject {
    id: root

    property NotebookWindow notebook: NotebookWindow {
        onRaiseDesktopNoteRequested: (note) => root.raiseDesktopNote(note)
    }

    property Instantiator desktopNotes: Instantiator {
        active: App.desktopNotesVisible
        model: App.store.desktopNotes
        delegate: DesktopNoteWindow {
            onDocked: (note) => {
                root.notebook.select(note)
                root.notebook.showAndRaise()
            }
            onShowArchivedRequested: {
                root.notebook.showArchive()
                root.notebook.showAndRaise()
            }
            onAboutRequested: {
                root.notebook.showAbout()
                root.notebook.showAndRaise()
            }
        }
    }

    function raiseDesktopNote(note: Note): void {
        if (!App.desktopNotesVisible) {
            App.desktopNotesVisible = true
            // Windows are created synchronously when the Instantiator activates; raise on the next turn
            // so the freshly mapped surface can take focus.
            Qt.callLater(root.raiseDesktopNote, note)
            return
        }
        const window = root.desktopNotes.objectAt(App.store.desktopNotes.indexOf(note)) as DesktopNoteWindow
        if (window)
            window.raiseNote()
    }

    property Connections appConnections: Connections {
        target: App
        function onShowNotebookRequested(): void { root.notebook.showAndRaise() }
        function onToggleNotebookRequested(): void { root.notebook.toggle() }
        function onNewNoteRequested(): void {
            root.notebook.showAndRaise()
            root.notebook.newNote()
        }
    }
}
