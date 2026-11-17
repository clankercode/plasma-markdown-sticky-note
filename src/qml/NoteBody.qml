// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// The body of one note. Two faces sharing one scroll area:
//  * rendered: read-only Markdown (note.renderedBody). Links open in the browser; clicking a task box
//    toggles it; clicking elsewhere, Enter or typing switches to editing at (roughly) that spot.
//  * editing: plain text (note.body). Each keystroke writes back to the note (the store debounces
//    disk writes). The rendered face is emptied while hidden so typing never re-parses Markdown.
// Escape or losing focus returns to the rendered face.
FocusScope {
    id: root
    objectName: "noteBody"

    property Note note
    property Swatch swatch: Theme.yellow
    readonly property bool editing: editor.activeFocus || root.editRequested
    property bool editRequested: false
    property bool syncingEditor: false
    property bool windowVisible: true

    // Enter edit mode. `cursor` < 0 puts the cursor at the end of the text.
    function edit(cursor: int): void {
        if (!root.note)
            return
        root.syncEditor()
        root.editRequested = true
        editor.cursorPosition = cursor < 0 || cursor > editor.length ? editor.length : cursor
        editor.forceActiveFocus()
        root.editRequested = false
    }

    function leaveEdit(): void {
        if (!editor.activeFocus)
            return
        editor.deselect()
        editor.focus = false   // otherwise the scope would hand active focus straight back to it
        root.forceActiveFocus()
    }

    // Copies note.body into the editor only when it actually differs, keeping the cursor and scroll
    // position stable (an external reload must not throw the user to the top of the note).
    function syncEditor(): void {
        const body = root.note ? root.note.body : ""
        if (editor.text === body)
            return
        const pos = editor.cursorPosition
        const contentY = flick.contentY
        root.syncingEditor = true
        editor.text = body
        root.syncingEditor = false
        editor.cursorPosition = Math.min(pos, editor.length)
        flick.contentY = Math.max(0, Math.min(contentY, flick.contentHeight - flick.height))
    }

    // Look up the actual task link's source index, never count glyphs in literal/code text.
    function taskIndexAt(pos: int): int {
        return styler.taskIndexAt(pos)
    }

    // Best effort mapping of a click in the rendered text to an offset in the Markdown source: take the
    // word fragment just before the click (rendered text is the source minus markup) and find it in
    // the source. Returns -1 (end of note) when it cannot be located.
    function isBreak(c: string): bool {
        return c === " " || c === "\n" || c === "\u2029" || c === "\u2028" || c === "\t"
            || c === "\u2610" || c === "\u2611" || c === "\u2060"
    }

    function sourceOffsetAt(x: real, y: real): int {
        const body = root.note ? root.note.body : ""
        const pos = rendered.positionAt(x, y)
        // The word under / before the click.
        let start = pos
        while (start > 0 && pos - start < 32 && !root.isBreak(rendered.getText(start - 1, start)))
            --start
        let end = pos
        while (end < rendered.length && end - pos < 32 && !root.isBreak(rendered.getText(end, end + 1)))
            ++end
        const word = rendered.getText(start, end)
        if (word.length === 0)
            return -1
        // Prefer the occurrence nearest the click's relative position in the document.
        const hint = Math.round(body.length * (rendered.length > 0 ? start / rendered.length : 0))
        let best = -1
        for (let at = body.indexOf(word); at >= 0; at = body.indexOf(word, at + 1)) {
            if (best < 0 || Math.abs(at - hint) < Math.abs(best - hint))
                best = at
        }
        return best < 0 ? -1 : best + (pos - start)
    }

    // The URL under source position `pos` in the raw text: a Markdown link target or a bare URL.
    function urlAt(pos: int): string {
        const text = editor.text
        const start = text.lastIndexOf("\n", pos - 1) + 1
        const newline = text.indexOf("\n", pos)
        const line = text.substring(start, newline < 0 ? text.length : newline)
        const offset = pos - start
        const patterns = [new RegExp("\\[[^\\]]*\\]\\(([^)\\s]+)\\)", "g"),
                          new RegExp("(https?://[^\\s<>()]+|mailto:[^\\s<>()]+)", "g")]
        for (const re of patterns) {
            let m
            while ((m = re.exec(line)) !== null) {
                if (offset >= m.index && offset <= m.index + m[0].length)
                    return m[1]
            }
        }
        return ""
    }

    onNoteChanged: {
        root.leaveEdit()
        root.syncEditor()
        editor.cursorPosition = 0
        flick.contentY = 0
    }

    Connections {
        target: root.note
        function onBodyChanged(): void {
            // Echo of our own edit: nothing to do. External reload / task toggle: adopt it.
            if (editor.text !== root.note.body)
                root.syncEditor()
        }
    }

    Keys.onPressed: (event) => {
        if (root.editing || !root.note)
            return
        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            root.edit(-1)
            event.accepted = true
        } else if (event.text.length > 0 && event.text.charCodeAt(0) >= 32
                   && !(event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))) {
            root.edit(-1)
            editor.insert(editor.cursorPosition, event.text)
            event.accepted = true
        } else if (event.key === Qt.Key_Down) {
            flick.contentY = Math.min(flick.contentY + 40, Math.max(0, flick.contentHeight - flick.height))
            event.accepted = true
        } else if (event.key === Qt.Key_Up) {
            flick.contentY = Math.max(0, flick.contentY - 40)
            event.accepted = true
        } else if (event.key === Qt.Key_PageDown) {
            flick.contentY = Math.min(flick.contentY + flick.height * 0.9, Math.max(0, flick.contentHeight - flick.height))
            event.accepted = true
        } else if (event.key === Qt.Key_PageUp) {
            flick.contentY = Math.max(0, flick.contentY - flick.height * 0.9)
            event.accepted = true
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        clip: true
        contentWidth: width
        // Text insets live in x/y rather than TextEdit padding: Qt's TextEdit.linkAt() ignores padding,
        // which would put task-box hit areas in the wrong place.
        readonly property int insetTop: 6
        readonly property int insetBottom: Theme.fold + 6
        contentHeight: Math.max(height, insetTop + insetBottom
                                + (root.editing ? editor.implicitHeight : rendered.implicitHeight))
        boundsBehavior: Flickable.StopAtBounds
        // Mouse drags select text instead of flicking; wheel and touch still scroll.
        acceptedButtons: Qt.NoButton
        pixelAligned: true

        T.ScrollBar.vertical: PaperScrollBar { ink: root.swatch.ink }

        // Keep the caret visible while typing / moving through long notes.
        function ensureVisible(r: rect): void {
            const top = r.y + flick.insetTop
            if (top < flick.contentY)
                flick.contentY = Math.max(0, top - 4)
            else if (top + r.height > flick.contentY + flick.height)
                flick.contentY = top + r.height - flick.height + 4
        }

        // Clicks on the paper around the text (margins, below the last line) start editing at the end.
        MouseArea {
            width: flick.contentWidth
            height: flick.contentHeight
            cursorShape: Qt.IBeamCursor
            onPressed: (mouse) => {
                if (!root.note)
                    return
                if (root.editing) {
                    editor.cursorPosition = mouse.y < editor.y ? 0 : editor.length
                    editor.forceActiveFocus()
                } else {
                    root.edit(-1)
                }
            }
        }

        // Paper-scale Markdown typography plus code-block backgrounds and quote rules, applied once
        // per render (the rendered face is empty while editing, so typing never pays for this).
        MarkdownStyler {
            id: styler
            document: rendered.textDocument
            muted: root.swatch.muted
        }
        Repeater {
            model: rendered.visible ? styler.codeRects : []
            delegate: Rectangle {
                required property rect modelData
                x: rendered.x + modelData.x
                y: rendered.y + modelData.y
                width: modelData.width
                height: modelData.height
                radius: 5
                color: Qt.alpha(root.swatch.ink, 0.06)
                border.width: 1
                border.color: Qt.alpha(root.swatch.ink, 0.08)
            }
        }
        Repeater {
            model: rendered.visible ? styler.quoteRects : []
            delegate: Rectangle {
                required property rect modelData
                x: rendered.x + modelData.x
                y: rendered.y + modelData.y + 2
                width: 3
                height: modelData.height - 4
                radius: 1.5
                color: root.swatch.tab
            }
        }

        TextEdit {
            id: rendered
            objectName: "renderedBody"
            x: Theme.pad
            y: flick.insetTop
            width: flick.width - 2 * Theme.pad
            visible: !root.editing
            readOnly: true
            activeFocusOnPress: false
            selectByMouse: true
            textFormat: TextEdit.MarkdownText
            // Emptied while editing: the Markdown parse only happens when this face is shown.
            text: root.editing || !root.visible || !root.windowVisible || !root.note ? "" : root.note.renderedBody
            wrapMode: TextEdit.Wrap
            font: Theme.bodyFont
            color: root.swatch.ink
            selectionColor: root.swatch.selection
            selectedTextColor: root.swatch.ink

            onLinkActivated: (link) => {
                if (link.startsWith("task:") && root.note)
                    root.note.toggleTask(Number(link.substring(5)))
                else
                    App.openUrl(link)
            }
            onTextChanged: styler.apply()

            HoverHandler {
                id: renderedHover
                // Hand over links and task boxes (box lookup only runs as the pointer moves).
                cursorShape: rendered.hoveredLink.length > 0
                             || (renderedHover.hovered
                                 && root.taskIndexAt(rendered.positionAt(renderedHover.point.position.x,
                                                                         renderedHover.point.position.y)) >= 0)
                             ? Qt.PointingHandCursor : Qt.IBeamCursor
            }

            TapHandler {
                // A task box toggles; any other plain click starts editing near the click.
                // Links are left to onLinkActivated; drags still select text.
                onTapped: (eventPoint) => {
                    const x = eventPoint.position.x
                    const y = eventPoint.position.y
                    if (rendered.linkAt(x, y).length > 0 || rendered.selectedText.length > 0 || !root.note)
                        return
                    const task = root.taskIndexAt(rendered.positionAt(x, y))
                    if (task >= 0)
                        root.note.toggleTask(task)
                    else
                        root.edit(root.sourceOffsetAt(x, y))
                }
            }
        }

        Text {
            // Empty-note invitation (rendered face only; the editor shows its own).
            x: Theme.pad
            y: flick.insetTop
            width: flick.width - 2 * Theme.pad
            visible: !root.editing && root.note !== null && root.note.body.length === 0
            text: qsTr("Write something… (Markdown supported)")
            font: Theme.bodyFont
            color: root.swatch.muted
            wrapMode: Text.Wrap
        }

        TextEdit {
            id: editor
            x: Theme.pad
            y: flick.insetTop
            width: flick.width - 2 * Theme.pad
            visible: root.editing
            textFormat: TextEdit.PlainText
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            persistentSelection: false
            font: Theme.bodyFont
            color: root.swatch.ink
            selectionColor: root.swatch.selection
            selectedTextColor: root.swatch.ink
            tabStopDistance: 28
            Accessible.name: qsTr("Note text")

            onTextChanged: {
                if (!root.syncingEditor && root.note && editor.text !== root.note.body)
                    root.note.body = editor.text
            }
            onCursorRectangleChanged: if (editor.activeFocus) flick.ensureVisible(editor.cursorRectangle)
            Keys.onEscapePressed: root.leaveEdit()

            cursorDelegate: Rectangle {
                width: 2
                color: root.swatch.ink
                visible: editor.cursorVisible
            }

            HoverHandler { cursorShape: Qt.IBeamCursor }

            // Ctrl+click opens a link while editing.
            TapHandler {
                acceptedModifiers: Qt.ControlModifier
                onTapped: (eventPoint) => {
                    const url = root.urlAt(editor.positionAt(eventPoint.position.x, eventPoint.position.y))
                    if (url.length > 0)
                        App.openUrl(url)
                }
            }

            Text {
                width: editor.width
                visible: editor.length === 0 && !editor.inputMethodComposing
                text: qsTr("Write something… (Markdown supported)")
                font: Theme.bodyFont
                color: root.swatch.muted
                wrapMode: Text.Wrap
            }
        }
    }
}
