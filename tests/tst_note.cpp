// SPDX-License-Identifier: MIT
#include "note.h"
#include <QtTest>

class NoteTest : public QObject {
    Q_OBJECT
private slots:
    void defaultsAndSignals() {
        Note note("abc", "/tmp/abc.md");
        QCOMPARE(note.displayTitle(), "Untitled");
        QCOMPARE(note.color(), "yellow");
        QVERIFY(!note.isDirty());
        QSignalSpy dirty(&note, &Note::dirtied), display(&note, &Note::displayTitleChanged);
        QSignalSpy modified(&note, &Note::modifiedChanged), geometry(&note, &Note::geometryChanged);
        note.setGeometry("DP-1", 10, 20, 320, 300);
        QCOMPARE(geometry.count(), 1);
        QCOMPARE(dirty.count(), 1);
        QCOMPARE(modified.count(), 0);
        note.setGeometry("DP-1", 10, 20, 320, 300);
        QCOMPARE(dirty.count(), 1);
        note.setBody("# Hello\n- [ ] task");
        QCOMPARE(display.count(), 1);
        QCOMPARE(note.displayTitle(), "Hello");
        QCOMPARE(modified.count(), 1);
        QVERIFY(note.renderedBody().contains("[☐](task:0)"));
        QCOMPARE(note.renderedBody(), note.renderedBody());
        note.toggleTask(0);
        QVERIFY(note.renderedBody().contains("[☑](task:0)"));
        QCOMPARE(display.count(), 1);
        note.setTitle("Explicit");
        note.setBody("Different");
        QCOMPARE(display.count(), 2);
        note.setTitle("");
        QCOMPARE(display.count(), 3);
        QCOMPARE(note.displayTitle(), "Different");
        const int count = dirty.count();
        note.setBody(note.body());
        note.setColor("invalid");
        QCOMPARE(dirty.count(), count);
        note.clearDirty();
        QVERIFY(!note.isDirty());
    }
    void longUntitledEditBudget() {
        Note note("long", "unused");
        const QString body(1000000, 'a');
        note.setBody(body);
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < 20; ++i) {
            note.setBody(body + QString::number(i));
            QCOMPARE(note.displayTitle(), QString(39, 'a') + QChar(0x2026));
        }
        QVERIFY2(timer.elapsed() < 150, qPrintable(QString::number(timer.elapsed()) + " ms"));
    }
    void roundTrip() {
        const QString text = "---\nid: abc\ntitle: 'Original'\ncolor: pink\ncreated: 2026-11-17T16:34:55+11:00\nmodified: 2026-11-17T16:40:02+11:00\narchived: false\norder: 3\npopped_out: false\npinned: false\nscreen: DP-1\nx: 120\ny: 80\nwidth: 280\nheight: 260\n# preserved\ncustom: 'yes' # comment\n---\nbody\n";
        Note note("abc", "/tmp/abc.md");
        note.loadFromText(text);
        QVERIFY(!note.isDirty());
        QCOMPARE(note.toText(), text);
        note.setTitle("true");
        QVERIFY(note.toText().contains("title: \"true\"\n"));
        QVERIFY(note.toText().contains("# preserved\ncustom: 'yes' # comment\n"));
        Note copy("abc", "/tmp/abc.md");
        copy.loadFromText(note.toText());
        QCOMPARE(copy.title(), "true");
        QCOMPARE(copy.x(), 120);
        QCOMPARE(copy.width(), 280);
        QCOMPARE(copy.created().offsetFromUtc(), 11 * 3600);
    }
    void reload() {
        Note note("filename", "unused");
        note.setBody("old");
        QSignalSpy body(&note, &Note::bodyChanged), dirty(&note, &Note::dirtied);
        const auto fallback = QDateTime::fromString("2026-01-01T12:00:00+02:00", Qt::ISODate);
        note.loadFromText("---\nid: wrong\ncolor: invalid\nwidth: nonsense\narchived: true\n---\nnew", fallback);
        QCOMPARE(note.id(), "filename");
        QCOMPARE(note.color(), "yellow");
        QCOMPARE(note.width(), Note::DefaultWidth);
        QCOMPARE(note.created(), fallback);
        QVERIFY(note.archived());
        QCOMPARE(body.count(), 1);
        QCOMPARE(dirty.count(), 0);
        QVERIFY(!note.isDirty());
        QVERIFY(note.toText().contains("id: filename\n"));
        const auto modified = note.modified();
        note.setArchived(false);
        note.setOrder(10);
        note.setPoppedOut(true);
        note.setPinned(true);
        QCOMPARE(note.modified(), modified);
        note.setColor("blue");
        QVERIFY(note.modified() > modified);
        auto doc = FrontMatter::parse(note.toText());
        QVERIFY(QRegularExpression("(?:[+-][0-9]{2}:[0-9]{2}|Z)$").match(doc.value("modified")).hasMatch());
    }
    void canonicalOrder() {
        Note note("new", "unused");
        const auto serialized = note.toText();
        QVERIFY(serialized.contains("archived: false\n"));
        QVERIFY(serialized.contains("order: 0\n"));
        QVERIFY(serialized.contains("pinned: false\n"));
        const auto doc = FrontMatter::parse(serialized);
        QStringList keys;
        for (const auto &line : doc.lines) keys << line.key;
        QCOMPARE(keys, QStringList({"id", "title", "color", "created", "modified", "archived", "order", "popped_out", "pinned", "screen", "x", "y", "width", "height"}));
        note.loadFromText("---\nmalformed");
        QCOMPARE(FrontMatter::parse(note.toText()).body, "---\nmalformed");
    }
};
QTEST_GUILESS_MAIN(NoteTest)
#include "tst_note.moc"
