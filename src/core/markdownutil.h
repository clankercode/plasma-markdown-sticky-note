// SPDX-License-Identifier: MIT
#pragma once

#include <QString>

namespace MarkdownUtil {

// Derives a short display title from a Markdown body: the first non-empty line, with leading
// Markdown syntax stripped (#, >, -, *, +, "1.", "- [ ]", "- [x]"), emphasis markers (*, _, `)
// removed, trimmed and elided to at most maxLength characters (with "…"). Empty => "".
QString deriveTitle(const QString &body, int maxLength = 40);

// Number of GFM task-list items ("- [ ] ", "* [x] ", "+ [X] ", "1. [ ] ", indented allowed),
// ignoring lines inside fenced code blocks (``` or ~~~).
int taskCount(const QString &body);

// Toggles the index-th (0-based, document order, same rules as taskCount) task checkbox between
// "[ ]" and "[x]". Returns body unchanged if index is out of range.
QString toggleTask(const QString &body, int index);

// Sets the index-th task to checked/unchecked. Returns body unchanged if out of range.
QString setTaskChecked(const QString &body, int index, bool checked);

// Prepares a body for display with Qt's Markdown renderer (Text.MarkdownText): every task checkbox
// "[ ]" / "[x]" (same matching rules as taskCount) is replaced by a link "[☐](task:N)" / "[☑](task:N)"
// so the UI can catch clicks via onLinkActivated("task:N") and call Note::toggleTask(N).
// The list marker itself is kept, so "- [ ] milk" becomes "- [☐](task:0) milk".
// Everything else (including fenced code) is untouched.
QString renderable(const QString &body);

} // namespace MarkdownUtil
