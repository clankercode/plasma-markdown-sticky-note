// SPDX-License-Identifier: MIT
#pragma once

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QQuickTextDocument>
#include <QtQml/qqmlregistration.h>

class QTextDocument;

// Gives the rendered Markdown face of a note its typography. Qt's Markdown importer produces
// browser-like defaults that are wrong for a small sheet of paper: 40 px list/quote indents, a 2x H1,
// a bullet *and* a box on task items, no code background, no quote rule. apply() rewrites the block
// and character formats of the parsed document once per render (never while typing: the rendered
// face is empty during editing) and publishes the text rectangles of code blocks and block quotes so
// QML can draw rounded backgrounds and rules behind them.
//
// Usage (NoteBody.qml):
//     MarkdownStyler { id: styler; document: rendered.textDocument; muted: swatch.muted }
//     TextEdit { id: rendered; textFormat: TextEdit.MarkdownText; onTextChanged: styler.apply() }
//
// Task items retain their source index in their task:N anchor; code/literal glyphs are not tasks.
class MarkdownStyler : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(QColor muted READ muted WRITE setMuted NOTIFY mutedChanged)
    // Rects in document (TextEdit-local) coordinates, one per run of consecutive code / quote blocks.
    Q_PROPERTY(QVariantList codeRects READ codeRects NOTIFY decorationsChanged)
    Q_PROPERTY(QVariantList quoteRects READ quoteRects NOTIFY decorationsChanged)

public:
    explicit MarkdownStyler(QObject *parent = nullptr);

    QQuickTextDocument *document() const { return m_document; }
    void setDocument(QQuickTextDocument *document);
    QColor muted() const { return m_muted; }
    void setMuted(const QColor &color);
    QVariantList codeRects() const { return m_codeRects; }
    QVariantList quoteRects() const { return m_quoteRects; }

    // Restyles the current document. Idempotent; cheap (one pass over the blocks).
    Q_INVOKABLE void apply();
    Q_INVOKABLE int taskIndexAt(int position) const;

Q_SIGNALS:
    void documentChanged();
    void mutedChanged();
    void decorationsChanged();

private:
    struct Run { int first; int last; };
    QTextDocument *textDocument() const;
    void updateDecorations();

    QPointer<QQuickTextDocument> m_document;
    QPointer<QObject> m_layout;   // the document layout we listen to
    QColor m_muted;
    QList<Run> m_codeRuns;
    QList<Run> m_quoteRuns;
    QVariantList m_codeRects;
    QVariantList m_quoteRects;
    bool m_applying = false;
};
