// SPDX-License-Identifier: MIT
#include "notestore.h"
#include "note.h"
#include "notelistmodel.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRandomGenerator>
#include <QSaveFile>
#include <algorithm>
#include <limits>
#include <utility>

NoteStore::NoteStore(const QString &notesDir, QObject *parent)
    : QObject(parent), m_dir(QDir(notesDir).absolutePath()), m_watcher(new QFileSystemWatcher(this)),
      m_active(new NoteListModel(this)), m_archived(new NoteListModel(this)), m_desktop(new NoteListModel(this))
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(400);
    m_rescanTimer.setSingleShot(true);
    m_rescanTimer.setInterval(150);
    connect(&m_saveTimer, &QTimer::timeout, this, &NoteStore::flush);
    connect(&m_rescanTimer, &QTimer::timeout, this, &NoteStore::rescan);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &NoteStore::scheduleRescan);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        m_changedPaths.insert(path);
        scheduleRescan();
    });
    if (auto *app = QCoreApplication::instance())
        connect(app, &QCoreApplication::aboutToQuit, this, &NoteStore::flush);
}
NoteStore::~NoteStore() { flush(); }
void NoteStore::setSaveDelay(int ms) { m_saveTimer.setInterval(qMax(0, ms)); }
int NoteStore::saveDelay() const { return m_saveTimer.interval(); }
Note *NoteStore::noteById(const QString &id) const { return m_byId.value(id, nullptr); }

bool NoteStore::load()
{
    m_error.clear();
    if (!QDir().mkpath(m_dir)) {
        m_error = QStringLiteral("Could not create notes directory: %1").arg(m_dir);
        return false;
    }
    const QFileInfo directory(m_dir);
    if (!directory.isDir() || !directory.isReadable()) {
        m_error = QStringLiteral("Cannot read notes directory: %1").arg(m_dir);
        return false;
    }
    watchDirectory();
    rescan();
    return true;
}
void NoteStore::observeNote(Note *note)
{
    connect(note, &Note::dirtied, this, &NoteStore::onNoteDirtied);
    auto refresh = [this] { if (!m_deferModelRefresh) refreshModels(); };
    connect(note, &Note::archivedChanged, this, refresh);
    connect(note, &Note::poppedOutChanged, this, refresh);
    connect(note, &Note::orderChanged, this, refresh);
    connect(note, &Note::modifiedChanged, this, [this, note] {
        if (note->archived() && !m_deferModelRefresh) refreshModels();
    });
}
Note *NoteStore::addNoteFromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return nullptr;
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return nullptr;
    const QFileInfo info(file);
    const auto id = info.completeBaseName();
    if (m_byId.contains(id)) return m_byId.value(id);
    auto *note = new Note(id, path, this);
    note->loadFromText(QString::fromUtf8(bytes), info.lastModified());
    m_notes.append(note);
    m_byId.insert(id, note);
    m_ownWrites.insert(path, bytes);
    m_fileStamps.insert(path, {info.lastModified(), info.metadataChangeTime(), info.size()});
    observeNote(note);
    Q_EMIT countChanged();
    Q_EMIT noteAdded(note);
    return note;
}
void NoteStore::watchFile(const QString &path)
{
    if (QFile::exists(path) && !m_watcher->files().contains(path)) m_watcher->addPath(path);
}
void NoteStore::watchDirectory()
{
    // Watch the closest surviving ancestor as well, so deletion/recreation needs no polling.
    QString ancestor = QFileInfo(m_dir).absolutePath();
    while (!QFileInfo(ancestor).isDir()) {
        const auto parent = QFileInfo(ancestor).absolutePath();
        if (parent == ancestor) break;
        ancestor = parent;
    }
    QStringList desired{ancestor};
    if (QFileInfo(m_dir).isDir() && ancestor != m_dir) desired.append(m_dir);
    const auto watched = m_watcher->directories();
    for (const auto &path : watched)
        if (!desired.contains(path)) m_watcher->removePath(path);
    for (const auto &path : desired)
        if (!watched.contains(path)) m_watcher->addPath(path);
}
void NoteStore::scheduleRescan() { m_rescanTimer.start(); }
void NoteStore::rescan()
{
    m_rescanTimer.stop();
    watchDirectory();
    // A temporarily unavailable directory must not look like an external deletion of every note.
    if (!QFileInfo(m_dir).isReadable() || !QFileInfo(m_dir).isDir()) return;
    const auto files = QDir(m_dir).entryInfoList({QStringLiteral("*.md")}, QDir::Files | QDir::NoDotAndDotDot, QDir::NoSort);
    const auto changedPaths = std::exchange(m_changedPaths, {});
    QSet<QString> present;
    m_deferModelRefresh = true;
    for (const auto &info : files) {
        if (info.fileName().startsWith('.') || !info.fileName().endsWith(".md")) continue;
        const auto path = info.absoluteFilePath();
        present.insert(path);
        auto *note = noteById(info.completeBaseName());
        if (!note) { addNoteFromFile(path); continue; }
        if (note->isDirty()) continue;
        const FileStamp stamp{info.lastModified(), info.metadataChangeTime(), info.size()};
        // Directory events describe membership/atomic replacements, not changes to every body.
        if (!changedPaths.contains(path) && m_fileStamps.value(path) == stamp) continue;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto bytes = file.readAll();
        if (file.error() != QFileDevice::NoError) continue;
        m_fileStamps.insert(path, stamp);
        if (m_ownWrites.value(path) == bytes) continue;
        note->loadFromText(QString::fromUtf8(bytes), info.lastModified());
        m_ownWrites.insert(path, bytes);
        Q_EMIT noteReloaded(note);
    }
    const auto previous = m_notes;
    QList<Note *> removed;
    for (auto *note : previous) {
        if (present.contains(note->filePath()) || note->isDirty()) continue;
        m_notes.removeOne(note);
        m_byId.remove(note->id());
        m_ownWrites.remove(note->filePath());
        m_fileStamps.remove(note->filePath());
        m_dirty.remove(note);
        removed.append(note);
    }
    m_deferModelRefresh = false;
    refreshModels();
    for (auto *note : removed) {
        Q_EMIT countChanged();
        Q_EMIT noteRemoved(note->id());
        note->deleteLater();
    }
    // Atomic replacements drop individual file watches. Restore them in a single batch.
    const auto watched = m_watcher->files();
    QSet<QString> current(watched.begin(), watched.end());
    QStringList add;
    for (const auto &path : present) if (!current.contains(path)) add.append(path);
    if (!add.isEmpty()) m_watcher->addPaths(add);
}
void NoteStore::refreshModels()
{
    QList<Note *> active, archived, desktop;
    active.reserve(m_notes.size());
    for (auto *note : m_notes) {
        if (note->archived()) archived.append(note);
        else {
            active.append(note);
            if (note->poppedOut()) desktop.append(note);
        }
    }
    std::sort(active.begin(), active.end(), [](const Note *a, const Note *b) {
        if (a->order() != b->order()) return a->order() < b->order();
        if (a->created() != b->created()) return a->created() < b->created();
        return a->id() < b->id();
    });
    std::sort(archived.begin(), archived.end(), [](const Note *a, const Note *b) {
        if (a->modified() != b->modified()) return a->modified() > b->modified();
        return a->id() < b->id();
    });
    m_active->setNotes(active);
    m_archived->setNotes(archived);
    m_desktop->setNotes(desktop);
}
void NoteStore::onNoteDirtied()
{
    auto *note = qobject_cast<Note *>(sender());
    if (!note || !m_notes.contains(note)) return;
    m_dirty.insert(note);
    m_saveTimer.start();
}
void NoteStore::writeNote(Note *note)
{
    const auto path = note->filePath();
    const auto bytes = note->toText().toUtf8();
    // No disk access occurs in the setters. Comparing cached bytes also avoids redundant saves.
    if (m_ownWrites.contains(path) && m_ownWrites.value(path) == bytes) {
        // A dirty note wins over external edits, even if the user reverted back to the cached
        // content before this save. Do not mistake cached equality for on-disk equality.
        QFile existing(path);
        if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes
            && existing.error() == QFileDevice::NoError) {
            note->clearDirty();
            m_dirty.remove(note);
            return;
        }
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = file.errorString();
        Q_EMIT saveFailed(path, m_error);
        return;
    }
    m_ownWrites.insert(path, bytes);
    const QFileInfo info(path);
    m_fileStamps.insert(path, {info.lastModified(), info.metadataChangeTime(), info.size()});
    note->clearDirty();
    m_dirty.remove(note);
    watchFile(path);
}
void NoteStore::flush()
{
    m_saveTimer.stop();
    const auto dirty = m_dirty;
    for (auto *note : dirty) if (note->isDirty()) writeNote(note); else m_dirty.remove(note);
}
bool NoteStore::hasUnsavedNotes() const
{
    return std::any_of(m_notes.cbegin(), m_notes.cend(), [](const Note *note) { return note->isDirty(); });
}
bool NoteStore::saveRecoveryCopies(const QString &directory)
{
    if (!QDir().mkpath(directory)
        || !QFile::setPermissions(directory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) return false;
    for (const auto *note : std::as_const(m_notes)) {
        if (!note->isDirty()) continue;
        const auto path = QDir(directory).filePath(note->id() + ".md");
        if (QFileInfo::exists(path)) return false;
        const auto bytes = note->toText().toUtf8();
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return false;
    }
    return true;
}
QString NoteStore::generateId() const
{
    QString id;
    do {
        id = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-"))
            + QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(65536), 4, 16, QLatin1Char('0'));
    } while (m_byId.contains(id) || QFile::exists(QDir(m_dir).filePath(id + ".md")));
    return id;
}
Note *NoteStore::createNote(const QString &color)
{
    if (!QDir().mkpath(m_dir)) {
        m_error = QStringLiteral("Could not create notes directory");
        Q_EMIT saveFailed(m_dir, m_error);
        return nullptr;
    }
    const auto id = generateId();
    auto *note = new Note(id, QDir(m_dir).filePath(id + ".md"), this);
    int order = -1;
    for (auto *existing : m_notes) order = qMax(order, existing->order());
    if (order < std::numeric_limits<int>::max()) ++order;
    note->setOrder(order);
    QString selected = color;
    if (selected.isEmpty()) {
        const auto colors = Note::colorNames();
        const auto last = m_active->count() ? m_active->get(m_active->count() - 1) : (m_notes.isEmpty() ? nullptr : m_notes.last());
        selected = last ? colors.at((colors.indexOf(last->color()) + 1) % colors.size()) : colors.first();
    }
    note->setColor(selected);
    // writeNote also writes clean new notes; a failure must not expose a note with no backing file.
    writeNote(note);
    if (!QFile::exists(note->filePath())) { delete note; return nullptr; }
    observeNote(note);
    m_notes.append(note);
    m_byId.insert(id, note);
    watchFile(note->filePath());
    refreshModels();
    Q_EMIT countChanged();
    Q_EMIT noteAdded(note);
    return note;
}
void NoteStore::archive(Note *note)
{
    if (!m_notes.contains(note)) return;
    m_deferModelRefresh = true;
    note->setPoppedOut(false);
    note->setArchived(true);
    m_deferModelRefresh = false;
    refreshModels();
}
void NoteStore::unarchive(Note *note)
{
    if (m_notes.contains(note)) note->setArchived(false);
}
bool NoteStore::deleteNote(Note *note)
{
    if (!m_notes.contains(note)) return false;
    if (note->isDirty()) {
        writeNote(note);
        if (note->isDirty()) return false;
    }
    const auto trash = QDir(m_dir).filePath(QStringLiteral(".trash"));
    auto destination = QDir(trash).filePath(note->id() + ".md");
    for (int suffix = 1; QFileInfo::exists(destination); ++suffix)
        destination = QDir(trash).filePath(note->id() + '-' + QString::number(suffix) + ".md");
    if (!QDir().mkpath(trash) || !QFile::rename(note->filePath(), destination)) {
        m_error = QStringLiteral("Could not move note to trash: %1").arg(destination);
        Q_EMIT saveFailed(note->filePath(), m_error);
        return false;
    }
    m_watcher->removePath(note->filePath());
    m_ownWrites.remove(note->filePath());
    m_fileStamps.remove(note->filePath());
    m_dirty.remove(note);
    m_notes.removeOne(note);
    m_byId.remove(note->id());
    refreshModels();
    Q_EMIT countChanged();
    Q_EMIT noteRemoved(note->id());
    note->deleteLater();
    return true;
}
void NoteStore::moveNote(Note *note, int toIndex)
{
    auto notes = m_active->notes();
    const auto from = notes.indexOf(note);
    if (from < 0 || toIndex < 0 || toIndex >= notes.size()) return;
    notes.move(from, toIndex);
    m_deferModelRefresh = true;
    for (int i = 0; i < notes.size(); ++i) notes.at(i)->setOrder(i);
    m_deferModelRefresh = false;
    refreshModels();
}
