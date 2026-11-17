// SPDX-License-Identifier: MIT
#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class Note;
class NoteListModel;
class QFileSystemWatcher;

// Owns all notes in a directory and persists them.
//
// Persistence rules:
//  * load() reads every `*.md` directly in notesDir (not subdirectories, not dotfiles) synchronously.
//    Must be fast: one read per file, no rendering.
//  * When a Note emits dirtied(), it is queued and a single-shot debounce timer
//    (saveDelayMs, default 400) is (re)started; on timeout all dirty notes are written with QSaveFile.
//  * flush() writes all dirty notes immediately (called on aboutToQuit and in destructor).
//  * Own writes are remembered (path -> written content hash / mtime) so the QFileSystemWatcher
//    notification they trigger does not cause a reload.
//  * External change to a file: if the note is not dirty, reload it in place (same Note object,
//    signals fire). If dirty, the in-app version wins (it will be written on next save).
//  * New external file => note added. External deletion => note removed (unless dirty, then re-written).
//  * Watcher events are coalesced with a short timer (~150 ms) before rescanning.
//  * Note ids: "yyyyMMdd-HHmmss-xxxx" (local time + 4 random lowercase hex chars), unique in dir.
class NoteStore : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by the application")

    Q_PROPERTY(QString notesDir READ notesDir CONSTANT)
    // Non-archived notes sorted by (order, created). Includes popped-out notes.
    Q_PROPERTY(NoteListModel *activeNotes READ activeNotes CONSTANT)
    // Archived notes sorted by modified, newest first.
    Q_PROPERTY(NoteListModel *archivedNotes READ archivedNotes CONSTANT)
    // Non-archived notes with poppedOut == true (any order).
    Q_PROPERTY(NoteListModel *desktopNotes READ desktopNotes CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit NoteStore(const QString &notesDir, QObject *parent = nullptr);
    ~NoteStore() override; // calls flush()

    // Creates notesDir if needed and loads all notes. Returns false (and sets errorString) on failure.
    bool load();
    QString errorString() const { return m_error; }

    QString notesDir() const { return m_dir; }
    NoteListModel *activeNotes() const { return m_active; }
    NoteListModel *archivedNotes() const { return m_archived; }
    NoteListModel *desktopNotes() const { return m_desktop; }
    int count() const { return m_notes.size(); }
    QList<Note *> notes() const { return m_notes; }

    Q_INVOKABLE Note *noteById(const QString &id) const;

    // Creates a new (non-archived) note with order = max(order)+1, writes it immediately, returns it.
    // If color is empty, picks the colour after the last note's colour in Note::colorNames() cycle.
    Q_INVOKABLE Note *createNote(const QString &color = {});
    // Archive / unarchive (sets flag, pops back in when archiving).
    Q_INVOKABLE void archive(Note *note);
    Q_INVOKABLE void unarchive(Note *note);
    // Moves the file to <notesDir>/.trash/<id>.md (unique suffix on collision) and removes the note.
    // Existing recovery copies are never overwritten or unlinked.
    Q_INVOKABLE bool deleteNote(Note *note);
    // Reorders active notes: moves `note` to position `toIndex` within activeNotes and renumbers
    // `order` of all active notes 0..n-1 (only notes whose order changed are dirtied).
    Q_INVOKABLE void moveNote(Note *note, int toIndex);

    Q_INVOKABLE void flush();
    // Flush remains source-compatible; callers can inspect outstanding edits before exiting.
    Q_INVOKABLE bool hasUnsavedNotes() const;
    // Atomic recovery copies of dirty notes; never clears dirty state or overwrites existing copies.
    bool saveRecoveryCopies(const QString &directory);

    void setSaveDelay(int ms); // for tests
    int saveDelay() const;

    // Generates an unused id (exposed for tests).
    QString generateId() const;

Q_SIGNALS:
    void countChanged();
    void noteAdded(Note *note);
    void noteRemoved(const QString &id);
    // Emitted after a note was reloaded because its file changed on disk.
    void noteReloaded(Note *note);
    void saveFailed(const QString &path, const QString &error);

private:
    Note *addNoteFromFile(const QString &path);
    void observeNote(Note *note);
    bool m_deferModelRefresh = false; // batch loading / renumbering without intermediate model churn
    void writeNote(Note *note);
    void onNoteDirtied();
    void scheduleRescan();
    void rescan();
    void refreshModels();
    void watchFile(const QString &path);
    void watchDirectory();
    struct FileStamp {
        QDateTime modified;
        QDateTime changed;
        qint64 size = -1;
        bool operator==(const FileStamp &) const = default;
    };
    QHash<QString, FileStamp> m_fileStamps;
    QSet<QString> m_changedPaths;

    QString m_dir;
    QString m_error;
    QList<Note *> m_notes;
    QHash<QString, Note *> m_byId;
    QSet<Note *> m_dirty;
    QHash<QString, QByteArray> m_ownWrites; // path -> bytes we last wrote/read
    QTimer m_saveTimer;
    QTimer m_rescanTimer;
    QFileSystemWatcher *m_watcher = nullptr;
    NoteListModel *m_active = nullptr;
    NoteListModel *m_archived = nullptr;
    NoteListModel *m_desktop = nullptr;
};
