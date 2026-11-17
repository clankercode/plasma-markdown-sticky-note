// SPDX-License-Identifier: MIT
#pragma once

#include "frontmatter.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class NoteStore;

// One sticky note, backed by `<notesDir>/<id>.md`. Owned by NoteStore.
//
// All setters are cheap: they update the in-memory value, emit the NOTIFY signal, bump
// `modified` (only for content changes: title/body/color), and mark the note dirty so the
// store schedules a debounced atomic save. Setting an equal value is a no-op (no signal, no save).
class Note : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Notes are created by the NoteStore")

    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    // title if non-empty, else MarkdownUtil::deriveTitle(body), else "Untitled"
    Q_PROPERTY(QString displayTitle READ displayTitle NOTIFY displayTitleChanged)
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY bodyChanged)
    // MarkdownUtil::renderable(body), computed lazily and cached until body changes.
    Q_PROPERTY(QString renderedBody READ renderedBody NOTIFY bodyChanged)
    Q_PROPERTY(QString color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QDateTime created READ created CONSTANT)
    Q_PROPERTY(QDateTime modified READ modified NOTIFY modifiedChanged)
    Q_PROPERTY(bool archived READ archived WRITE setArchived NOTIFY archivedChanged)
    Q_PROPERTY(int order READ order WRITE setOrder NOTIFY orderChanged)
    Q_PROPERTY(bool poppedOut READ poppedOut WRITE setPoppedOut NOTIFY poppedOutChanged)
    Q_PROPERTY(bool pinned READ pinned WRITE setPinned NOTIFY pinnedChanged)
    Q_PROPERTY(QString screen READ screen WRITE setScreen NOTIFY geometryChanged)
    Q_PROPERTY(int x READ x WRITE setX NOTIFY geometryChanged)
    Q_PROPERTY(int y READ y WRITE setY NOTIFY geometryChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY geometryChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY geometryChanged)
    Q_PROPERTY(QString filePath READ filePath CONSTANT)

public:
    // Valid colour names, in picker order.
    static QStringList colorNames(); // yellow, pink, green, blue, purple, orange, gray
    static constexpr int DefaultWidth = 300;
    static constexpr int DefaultHeight = 280;
    static constexpr int NoPosition = -1; // x/y value meaning "never placed"

    explicit Note(const QString &id, const QString &filePath, QObject *parent = nullptr);

    // Load from full file text (front matter + body). Does not mark dirty. Unknown keys preserved.
    // Missing fields get defaults: color yellow, created/modified = now (or file mtime if given),
    // archived/poppedOut/pinned false, order 0, x/y NoPosition, width/height defaults.
    // Emits change signals for properties that changed (so reloads update bound UIs).
    void loadFromText(const QString &text, const QDateTime &fileModified = {});
    // Serialise to file text, preserving unknown keys / comments / ordering from the loaded file.
    QString toText() const;

    QString id() const { return m_id; }
    QString filePath() const { return m_filePath; }
    QString title() const { return m_title; }
    QString displayTitle() const;
    QString body() const { return m_body; }
    QString renderedBody() const;
    QString color() const { return m_color; }
    QDateTime created() const { return m_created; }
    QDateTime modified() const { return m_modified; }
    bool archived() const { return m_archived; }
    int order() const { return m_order; }
    bool poppedOut() const { return m_poppedOut; }
    bool pinned() const { return m_pinned; }
    QString screen() const { return m_screen; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int width() const { return m_width; }
    int height() const { return m_height; }

    void setTitle(const QString &v);
    void setBody(const QString &v);
    void setColor(const QString &v); // unknown names are ignored
    void setArchived(bool v);
    void setOrder(int v);
    void setPoppedOut(bool v);
    void setPinned(bool v);
    void setScreen(const QString &v);
    void setX(int v);
    void setY(int v);
    void setWidth(int v);
    void setHeight(int v);

    // Set several geometry fields at once (one geometryChanged emission, one save).
    Q_INVOKABLE void setGeometry(const QString &screen, int x, int y, int width, int height);
    // Toggle the index-th Markdown task checkbox in the body.
    Q_INVOKABLE void toggleTask(int index);

    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

Q_SIGNALS:
    void titleChanged();
    void displayTitleChanged();
    void bodyChanged();
    void colorChanged();
    void modifiedChanged();
    void archivedChanged();
    void orderChanged();
    void poppedOutChanged();
    void pinnedChanged();
    void geometryChanged();
    // Emitted whenever the note becomes dirty (store listens to schedule a save).
    void dirtied();

private:
    void markDirty(bool contentChange);

    QString m_id;
    QString m_filePath;
    QString m_title;
    QString m_body;
    QString m_color = QStringLiteral("yellow");
    QDateTime m_created;
    QDateTime m_modified;
    bool m_archived = false;
    int m_order = 0;
    bool m_poppedOut = false;
    bool m_pinned = false;
    QString m_screen;
    int m_x = NoPosition;
    int m_y = NoPosition;
    int m_width = DefaultWidth;
    int m_height = DefaultHeight;
    bool m_dirty = false;
    mutable QString m_displayCache;
    mutable bool m_displayValid = false;
    mutable QString m_renderedCache;
    mutable bool m_renderedValid = false;
    FrontMatter::Document m_doc; // retains unknown keys/comments for round-trip
};
