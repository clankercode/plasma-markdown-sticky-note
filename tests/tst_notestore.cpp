// SPDX-License-Identifier: MIT
#include "notestore.h"
#include "note.h"
#include "notelistmodel.h"
#include <QElapsedTimer>
#include <QFile>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class StoreTest : public QObject {
    Q_OBJECT
    static bool write(const QString &path, const QByteArray &bytes) {
        QSaveFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
    }
    static QByteArray read(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll();
    }
private slots:
    void loading() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(write(dir.filePath("a.md"), "---\nid: wrong\norder: 2\ncreated: 2026-01-01T12:00:00Z\n---\nA"));
        QVERIFY(write(dir.filePath("b.md"), "---\norder: 1\npopped_out: true\n---\nB"));
        QVERIFY(write(dir.filePath("c.md"), "---\narchived: true\nmodified: 2026-11-17T12:00:00Z\n---\nC"));
        QVERIFY(write(dir.filePath("d.md"), "---\narchived: true\nmodified: 2026-11-18T12:00:00Z\n---\nD"));
        QVERIFY(write(dir.filePath(".hidden.md"), "hidden"));
        QVERIFY(write(dir.filePath("other.txt"), "ignore"));
        QVERIFY(QDir(dir.path()).mkdir("sub"));
        QVERIFY(write(dir.filePath("sub/sub.md"), "ignore"));
        NoteStore store(dir.path());
        QSignalSpy added(&store, &NoteStore::noteAdded);
        QVERIFY2(store.load(), qPrintable(store.errorString()));
        QCOMPARE(store.count(), 4);
        QCOMPARE(added.count(), 4);
        QCOMPARE(store.activeNotes()->get(0)->id(), "b");
        QCOMPARE(store.archivedNotes()->get(0)->id(), "d");
        QCOMPARE(store.desktopNotes()->count(), 1);
        QCOMPARE(store.noteById("a")->id(), "a");
        QVERIFY(!store.noteById("wrong"));
        QVERIFY(store.load());
        QCOMPARE(added.count(), 4);
        QVERIFY(store.errorString().isEmpty());
    }
    void lifecycle() {
        QTemporaryDir dir;
        NoteStore store(dir.filePath("nested"));
        QVERIFY(store.load());
        auto *a = store.createNote();
        auto *b = store.createNote();
        auto *c = store.createNote("blue");
        QVERIFY(a && b && c);
        QVERIFY(QFile::exists(a->filePath()));
        QCOMPARE(a->color(), "yellow");
        QCOMPARE(b->color(), "pink");
        QCOMPARE(c->color(), "blue");
        QCOMPARE(c->order(), 2);
        QVERIFY(QRegularExpression("^[0-9]{8}-[0-9]{6}-[0-9a-f]{4}$").match(a->id()).hasMatch());
        QSignalSpy reset(store.activeNotes(), &QAbstractItemModel::modelReset);
        store.moveNote(c, 0);
        QCOMPARE(store.activeNotes()->notes(), QList<Note *>({c, a, b}));
        QCOMPARE(c->order(), 0);
        QCOMPARE(a->order(), 1);
        QCOMPARE(b->order(), 2);
        QCOMPARE(reset.count(), 0);
        b->setOrder(-1);
        QCOMPARE(store.activeNotes()->get(0), b);
        a->setPoppedOut(true);
        QCOMPARE(store.desktopNotes()->get(0), a);
        store.archive(a);
        QVERIFY(a->archived());
        QVERIFY(!a->poppedOut());
        QCOMPARE(store.desktopNotes()->count(), 0);
        QCOMPARE(store.archivedNotes()->get(0), a);
        store.unarchive(a);
        QCOMPARE(store.archivedNotes()->count(), 0);
        a->setArchived(true);
        QCOMPARE(store.archivedNotes()->count(), 1);
        a->setArchived(false);
        store.flush();
        QVERIFY(!a->isDirty());
        const QString id = a->id(), path = a->filePath();
        const auto trash = QDir(store.notesDir()).filePath(".trash/" + id + ".md");
        QVERIFY(QDir().mkpath(QFileInfo(trash).absolutePath()));
        QVERIFY(write(trash, "old trash"));
        a->setBody("Latest content");
        QVERIFY(store.deleteNote(a));
        QVERIFY(!QFile::exists(path));
        QCOMPARE(read(trash), "old trash");
        QVERIFY(read(QDir(store.notesDir()).filePath(".trash/" + id + "-1.md")).endsWith("Latest content"));
        QCOMPARE(store.count(), 2);
        QVERIFY(!store.noteById(id));
        QVERIFY(!store.deleteNote(nullptr));
        NoteStore reload(store.notesDir());
        QVERIFY(reload.load());
        QCOMPARE(reload.count(), 2);
    }
    // Archiving must never delete or rewrite the note's content: the file stays where it is,
    // `archived: true` is added to the front matter, and the body + unknown keys are preserved.
    void archiveKeepsFileOnDisk() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("keep.md");
        QVERIFY(write(path, "---\ntitle: Keep me\nmy_key: custom value # note\n---\n- [ ] milk\n- [x] eggs\n"));
        NoteStore store(dir.path());
        QVERIFY(store.load());
        Note *note = store.noteById("keep");
        QVERIFY(note);
        store.archive(note);
        store.flush();
        QVERIFY(QFile::exists(path));
        const QByteArray archived = read(path);
        QVERIFY2(archived.contains("archived: true\n"), archived.constData());
        QVERIFY(archived.contains("my_key: custom value # note\n"));
        QVERIFY(archived.endsWith("\n---\n- [ ] milk\n- [x] eggs\n"));

        // A fresh store sees it as archived; unarchiving flips the flag back without losing anything.
        NoteStore reloaded(dir.path());
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.archivedNotes()->count(), 1);
        QCOMPARE(reloaded.activeNotes()->count(), 0);
        reloaded.unarchive(reloaded.noteById("keep"));
        reloaded.flush();
        const QByteArray restored = read(path);
        QVERIFY(restored.contains("archived: false\n"));
        QVERIFY(restored.contains("my_key: custom value # note\n"));
        QVERIFY(restored.endsWith("\n---\n- [ ] milk\n- [x] eggs\n"));
    }

    void debounceFlushAndOwnWrites() {
        QTemporaryDir dir;
        NoteStore store(dir.path());
        QVERIFY(store.load());
        store.setSaveDelay(60);
        QCOMPARE(store.saveDelay(), 60);
        auto *note = store.createNote();
        QVERIFY(note);
        const auto original = read(note->filePath());
        QSignalSpy reloaded(&store, &NoteStore::noteReloaded);
        note->setBody("first");
        QVERIFY(note->isDirty());
        QCOMPARE(read(note->filePath()), original);
        note->setBody("second");
        QTRY_VERIFY_WITH_TIMEOUT(!note->isDirty(), 2000);
        QVERIFY(read(note->filePath()).endsWith("second"));
        QTest::qWait(400);
        QCOMPARE(reloaded.count(), 0);
        note->setBody("flushed");
        store.flush();
        QVERIFY(!note->isDirty());
        QVERIFY(read(note->filePath()).endsWith("flushed"));
        QTest::qWait(400);
        QCOMPARE(reloaded.count(), 0);
        // A byte-identical external replacement is suppressed too, and the watch must be rearmed.
        QVERIFY(write(note->filePath(), read(note->filePath())));
        QTest::qWait(400);
        QCOMPARE(reloaded.count(), 0);
        QVERIFY(write(note->filePath(), "externally changed"));
        QTRY_COMPARE_WITH_TIMEOUT(note->body(), "externally changed", 3000);
        QCOMPARE(reloaded.count(), 1);
        // Destructor flushes synchronously.
        QString path;
        {
            NoteStore another(dir.filePath("destructor"));
            QVERIFY(another.load());
            auto *n = another.createNote();
            QVERIFY(n);
            path = n->filePath();
            n->setBody("saved on destruction");
        }
        QVERIFY(read(path).endsWith("saved on destruction"));
    }
    void externalChanges() {
        QTemporaryDir dir;
        NoteStore store(dir.path());
        QVERIFY(store.load());
        auto *note = store.createNote();
        QVERIFY(note);
        QSignalSpy reload(&store, &NoteStore::noteReloaded);
        const auto path = note->filePath();
        QVERIFY(write(path, "---\nid: mismatched\narchived: true\nunknown: keep\n---\nExternal"));
        QTRY_COMPARE_WITH_TIMEOUT(reload.count(), 1, 3000);
        QCOMPARE(store.noteById(note->id()), note);
        QCOMPARE(note->body(), "External");
        QCOMPARE(store.archivedNotes()->get(0), note);
        QCOMPARE(store.activeNotes()->count(), 0);
        QVERIFY(!note->isDirty());
        const auto addedPath = dir.filePath("external.md");
        QVERIFY(write(addedPath, "New external note"));
        QTRY_COMPARE_WITH_TIMEOUT(store.count(), 2, 3000);
        QVERIFY(store.noteById("external"));
        QVERIFY(QFile::remove(addedPath));
        QTRY_COMPARE_WITH_TIMEOUT(store.count(), 1, 3000);
        QVERIFY(!store.noteById("external"));
        QVERIFY(QFile::remove(path));
        QTRY_COMPARE_WITH_TIMEOUT(store.count(), 0, 3000);
    }
    void dirtyWins() {
        QTemporaryDir dir;
        NoteStore store(dir.path());
        QVERIFY(store.load());
        store.setSaveDelay(2000);
        auto *note = store.createNote();
        QVERIFY(note);
        note->setBody("in-app wins");
        QVERIFY(write(note->filePath(), "outside edit"));
        QTest::qWait(400);
        QCOMPARE(note->body(), "in-app wins");
        store.flush();
        QVERIFY(read(note->filePath()).endsWith("in-app wins"));
        note->setBody("resurrected");
        QVERIFY(QFile::remove(note->filePath()));
        QTest::qWait(400);
        QCOMPARE(store.count(), 1);
        store.flush();
        QVERIFY(QFile::exists(note->filePath()));
        QVERIFY(read(note->filePath()).endsWith("resurrected"));
        // Reverting an in-app metadata edit must not let an external overwrite win merely
        // because the serialized bytes match the store's cached previous save.
        const auto previous = read(note->filePath());
        note->setPinned(true);
        note->setPinned(false);
        QVERIFY(write(note->filePath(), "external overwrite while dirty"));
        QTest::qWait(400);
        store.flush();
        QCOMPARE(read(note->filePath()), previous);
    }
    void errors() {
        QTemporaryDir dir;
        QVERIFY(write(dir.filePath("file"), "not a directory"));
        NoteStore invalid(dir.filePath("file/nested"));
        QVERIFY(!invalid.load());
        QVERIFY(!invalid.errorString().isEmpty());
        QVERIFY(!invalid.createNote());
        NoteStore store(dir.path());
        QVERIFY(store.load());
        auto *note = store.createNote();
        QVERIFY(note);
        QSignalSpy failed(&store, &NoteStore::saveFailed);
        // A directory at the target path makes an atomic replacement fail, even as root.
        QVERIFY(QFile::remove(note->filePath()));
        QVERIFY(QDir().mkdir(note->filePath()));
        note->setBody("unsaved");
        store.flush();
        QCOMPARE(failed.count(), 1);
        QVERIFY(note->isDirty());
        QVERIFY(QDir().rmdir(note->filePath()));
        store.flush();
        QVERIFY(!note->isDirty());
    }
    void failedDeletePreservesTrash() {
        QTemporaryDir dir;
        NoteStore store(dir.path());
        QVERIFY(store.load());
        auto *note = store.createNote();
        QVERIFY(note);
        const auto trash = dir.filePath(".trash/" + note->id() + ".md");
        QVERIFY(QDir().mkpath(QFileInfo(trash).absolutePath()));
        QVERIFY(write(trash, "precious old trash"));
        QVERIFY(QFile::remove(note->filePath()));
        QVERIFY(!store.deleteNote(note));
        QCOMPARE(read(trash), "precious old trash");
    }
    void directoryRecreation() {
        QTemporaryDir parent;
        const auto path = parent.filePath("notes");
        NoteStore store(path);
        QVERIFY(store.load());
        auto *note = store.createNote();
        QVERIFY(note);
        const auto oldId = note->id();
        QVERIFY(QDir(path).removeRecursively());
        QTest::qWait(400);
        QCOMPARE(store.count(), 1); // unavailable directory is not mass deletion
        QVERIFY(QDir().mkpath(path));
        QVERIFY(write(path + "/external.md", "new external"));
        QTRY_VERIFY_WITH_TIMEOUT(store.noteById("external"), 3000);
        QVERIFY(!store.noteById(oldId));
        QVERIFY(write(path + "/external.md", "edited again"));
        QTRY_COMPARE_WITH_TIMEOUT(store.noteById("external")->body(), "edited again", 3000);
    }
    void recoveryCopiesPreserveDirtyState() {
        QTemporaryDir dir;
        NoteStore store(dir.filePath("notes"));
        QVERIFY(store.load());
        auto *note = store.createNote();
        QVERIFY(note);
        QVERIFY(!store.hasUnsavedNotes());
        note->setBody("precious unsaved draft");
        QVERIFY(store.hasUnsavedNotes());
        const auto recovery = dir.filePath("recovery");
        QVERIFY(store.saveRecoveryCopies(recovery));
        const auto path = QDir(recovery).filePath(note->id() + ".md");
        QVERIFY(read(path).endsWith("precious unsaved draft"));
        QVERIFY(note->isDirty());
        note->setBody("newer unsaved draft");
        QVERIFY(!store.saveRecoveryCopies(recovery)); // never overwrite the earlier recovery
        QVERIFY(read(path).endsWith("precious unsaved draft"));
        QVERIFY(store.saveRecoveryCopies(dir.filePath("second-recovery")));
        store.flush();
        QVERIFY(!store.hasUnsavedNotes());
    }
    void watcherSingleSave() {
        QTemporaryDir dir;
        for (int i = 0; i < 200; ++i)
            QVERIFY(write(dir.filePath(QString::number(i) + ".md"), "body"));
        NoteStore store(dir.path());
        QVERIFY(store.load());
        QTest::qWait(300);
        const auto marker = qEnvironmentVariable("STICKY_WATCH_TRACE_MARKER");
        if (!marker.isEmpty()) QVERIFY(write(marker + "-begin", "begin"));
        QSignalSpy reload(&store, &NoteStore::noteReloaded);
        auto *note = store.noteById("0");
        note->setBody("one changed body");
        store.flush();
        QTest::qWait(400);
        QCOMPARE(reload.count(), 0);
        if (!marker.isEmpty()) QVERIFY(write(marker + "-end", "end"));
        QVERIFY(write(dir.filePath("1.md"), "external replacement"));
        QTRY_COMPARE_WITH_TIMEOUT(store.noteById("1")->body(), "external replacement", 3000);
    }
    void performance() {
        QTemporaryDir dir;
        for (int i = 0; i < 2000; ++i) {
            const auto name = QString::number(i).rightJustified(4, '0');
            QVERIFY(write(dir.filePath(name + ".md"), "---\nid: " + name.toUtf8() + "\ntitle: Note\ncolor: yellow\ncreated: 2026-11-17T12:00:00+11:00\nmodified: 2026-11-17T12:00:00+11:00\norder: " + QByteArray::number(i) + "\n---\n# Body\n- [ ] task\n"));
        }
        NoteStore store(dir.path());
        QElapsedTimer timer;
        timer.start();
        QVERIFY(store.load());
        const auto elapsed = timer.elapsed();
        qInfo() << "Loaded 2000 notes in" << elapsed << "ms";
        QCOMPARE(store.count(), 2000);
        QVERIFY2(elapsed < 1500, qPrintable(QString::number(elapsed) + " ms exceeds Debug load budget"));
    }
};
QTEST_GUILESS_MAIN(StoreTest)
#include "tst_notestore.moc"
