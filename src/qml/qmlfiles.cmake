# SPDX-License-Identifier: MIT
# Owned by the QML/UI implementation. Paths relative to src/.
set(STICKY_QML_FILES
    qml/AboutOverlay.qml
    qml/ArchiveView.qml
    qml/ColorPopup.qml
    qml/ConfirmOverlay.qml
    qml/DesktopNoteWindow.qml
    qml/EmptyState.qml
    qml/Glyphs.qml
    qml/IconButton.qml
    qml/Icon.qml
    qml/Main.qml
    qml/NoteBody.qml
    qml/NotebookWindow.qml
    qml/NoteHeader.qml
    qml/NoteTab.qml
    qml/PaperButton.qml
    qml/PaperMenuItem.qml
    qml/PaperMenu.qml
    qml/PaperMenuSeparator.qml
    qml/Paper.qml
    qml/PaperScrollBar.qml
    qml/PaperToolTip.qml
    qml/ResizeHandle.qml
    qml/Swatch.qml
    qml/Theme.qml
)

set_source_files_properties(qml/Theme.qml qml/Glyphs.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
