// SPDX-License-Identifier: MIT
#include "appcontroller.h"
#include "note.h"
#include "notestore.h"
#include <QApplication>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class UiTest : public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    std::unique_ptr<NoteStore> store;
    std::unique_ptr<AppController> controller;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QObject *notebook = nullptr;
    QVariant eval(QObject *object, const QString &code) {
        QQmlExpression expression(qmlContext(object), object, code);
        auto result = expression.evaluate();
        if (expression.hasError()) qWarning() << expression.error();
        return result;
    }
    QObject *named(const QString &name) {
        return notebook->findChild<QObject *>(name);
    }
    QObject *body() { return named("noteBody"); }
private slots:
    void init() {
        QSettings().clear();
        store = std::make_unique<NoteStore>(dir.filePath(QUuid::createUuid().toString()));
        QVERIFY(store->load());
        QVERIFY(store->createNote());
        controller = std::make_unique<AppController>(store.get(), false);
        AppController::setInstance(controller.get());
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->loadFromModule("StickyNotes", "Main");
        QVERIFY(!engine->rootObjects().isEmpty());
        notebook = engine->rootObjects().first()->property("notebook").value<QObject *>();
        QVERIFY(notebook);
    }
    void cleanup() {
        engine.reset(); controller.reset(); store.reset();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void sourceSynchronizationIsReadOnly() {
        auto *note = notebook->property("currentNote").value<Note *>();
        for (const auto separator : {QChar('\r'), QChar(0x2028), QChar(0x2029)}) {
            const QString source = QStringLiteral("a") + separator + "b";
            note->loadFromText(source);
            QCOMPARE(note->body(), source);
            QVERIFY(!note->isDirty());
        }
    }
    void literalTaskLinksDoNotShiftCheckboxes() {
        auto *note = notebook->property("currentNote").value<Note *>();
        note->setBody("```\n[☐](task:7)\n```\n- [ ] actual\n- [ ] wrong target");
        auto *rendered = named("renderedBody");
        const auto plain = eval(rendered, "getText(0, length)").toString();
        QVERIFY(plain.contains("[☐](task:7)"));
        const int actual = plain.indexOf("actual");
        QVERIFY(actual >= 0);
        QCOMPARE(eval(body(), QString("taskIndexAt(%1)").arg(plain.lastIndexOf(QChar(0x2610), actual))).toInt(), 0);
    }
    void hiddenBodiesDoNotRender() {
        auto *note = notebook->property("currentNote").value<Note *>();
        notebook->setProperty("visible", false);
        note->setBody("# hidden change");
        auto *rendered = named("renderedBody");
        QCOMPARE(eval(rendered, "getText(0, length)").toString(), "");
        notebook->setProperty("visible", true);
        notebook->setProperty("showingArchive", true);
        note->setBody("# archived-view change");
        QCOMPARE(eval(rendered, "getText(0, length)").toString(), "");
        notebook->setProperty("showingArchive", false);
        QVERIFY(eval(rendered, "getText(0, length)").toString().contains("archived-view change"));
    }
    void deleteRetainsTarget() {
        auto *a = notebook->property("currentNote").value<Note *>();
        a->setTitle("First");
        auto *b = store->createNote();
        QVERIFY(b);
        b->setTitle("Second");
        store->flush();
        const auto aId = a->id(), bId = b->id();
        QMetaObject::invokeMethod(named("deleteConfirmation"), "ask");
        eval(notebook, "step(1)");
        QCOMPARE(notebook->property("currentNote").value<Note *>(), a);
        QMetaObject::invokeMethod(named("deleteConfirmation"), "confirmed");
        QVERIFY(!store->noteById(aId));
        QVERIFY(store->noteById(bId));
    }
    void membershipReplacement() {
        auto *a = notebook->property("currentNote").value<Note *>();
        auto *b = store->createNote();
        QVERIFY(b);
        store->archive(b);
        store->flush();
        auto write = [](const QString &path, const QByteArray &bytes) {
            QFile f(path); return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(bytes) == bytes.size();
        };
        QVERIFY(write(a->filePath(), "---\narchived: true\n---\nA"));
        QVERIFY(write(b->filePath(), "---\narchived: false\n---\nB"));
        QTRY_COMPARE_WITH_TIMEOUT(notebook->property("currentNote").value<Note *>(), b, 3000);
    }
    void failedFlushBlocksQuitAndRecoveryPreservesDraft() {
        auto *note = notebook->property("currentNote").value<Note *>();
        QVERIFY(QFile::remove(note->filePath()));
        QVERIFY(QDir().mkdir(note->filePath()));
        note->setBody("precious draft");
        controller->quit();
        QVERIFY(note->isDirty());
        QVERIFY(!controller->property("saveError").toString().isEmpty());
        // Forced shutdown must preserve the draft outside the unavailable notes location.
        QVERIFY(QMetaObject::invokeMethod(qApp, "aboutToQuit", Qt::DirectConnection));
        const QString recovery = controller->property("recoveryDirectory").toString();
        QVERIFY(!recovery.isEmpty());
        QVERIFY(!recovery.startsWith(store->notesDir()));
        QFile copy(QDir(recovery).filePath(note->id() + ".md"));
        QVERIFY(copy.open(QIODevice::ReadOnly));
        QVERIFY(copy.readAll().endsWith("precious draft"));
        QVERIFY(QDir().rmdir(note->filePath()));
        QVERIFY(QMetaObject::invokeMethod(controller.get(), "retrySave", Qt::DirectConnection));
        QVERIFY(!note->isDirty());
        QVERIFY(controller->property("saveError").toString().isEmpty());
    }
    void desktopShortcuts() {
        auto *note = notebook->property("currentNote").value<Note *>();
        note->setPoppedOut(true);
        auto *main = engine->rootObjects().first();
        auto *instantiator = main->property("desktopNotes").value<QObject *>();
        QVERIFY(instantiator);
        QObject *object = nullptr;
        QVERIFY(QMetaObject::invokeMethod(instantiator, "objectAt", Q_RETURN_ARG(QObject *, object), Q_ARG(int, 0)));
        auto *window = qobject_cast<QQuickWindow *>(object);
        QVERIFY(window);
        notebook->setProperty("visible", false);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        const int count = store->count();
        QTest::keyClick(window, Qt::Key_N, Qt::ControlModifier);
        QTRY_COMPARE(store->count(), count + 1);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTest::keyClick(window, Qt::Key_W, Qt::ControlModifier);
        QTRY_VERIFY(note->archived());
    }
    void desktopQuitShortcut() {
        auto *note = notebook->property("currentNote").value<Note *>();
        note->setPoppedOut(true);
        auto *instantiator = engine->rootObjects().first()->property("desktopNotes").value<QObject *>();
        QObject *object = nullptr;
        QVERIFY(QMetaObject::invokeMethod(instantiator, "objectAt", Q_RETURN_ARG(QObject *, object), Q_ARG(int, 0)));
        auto *window = qobject_cast<QQuickWindow *>(object);
        QVERIFY(window);
        notebook->setProperty("visible", false);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QSignalSpy requested(controller.get(), &AppController::preparingToQuit);
        QTest::keyClick(window, Qt::Key_Q, Qt::ControlModifier);
        QVERIFY(!requested.isEmpty());
    }
    void renderedScreenshot() {
        const auto output = qEnvironmentVariable("STICKY_TEST_SCREENSHOT");
        if (output.isEmpty()) QSKIP("Set STICKY_TEST_SCREENSHOT for an isolated visual capture");
        auto *note = notebook->property("currentNote").value<Note *>();
        note->setTitle("A little room to think");
        note->setBody("# The small things\nA quiet place for **ideas**, plans, and the things worth remembering.\n\n- [x] Make a little space\n- [ ] Follow the interesting thought\n- [ ] Take a walk before dinner\n\n> Good ideas need somewhere to land.\n\n```cpp\nauto today = make_something_good();\n```");
        auto *second = store->createNote("pink");
        QVERIFY(second);
        second->setTitle("Weekend ideas");
        auto *window = qobject_cast<QQuickWindow *>(notebook);
        QVERIFY(window);
        QTest::qWait(200);
        const auto image = window->grabWindow();
        QVERIFY(!image.isNull());
        QVERIFY(image.save(output));
    }
    void geometryCommittedAtShutdown() {
        notebook->setProperty("width", 789);
        // aboutToQuit must commit before settings sync, without waiting for the debounce.
        QVERIFY(QMetaObject::invokeMethod(qApp, "aboutToQuit", Qt::DirectConnection));
        QCOMPARE(controller->notebookGeometry().width(), 789);
    }
};
int main(int argc, char **argv) {
    QTemporaryDir environment;
    qputenv("XDG_CONFIG_HOME", environment.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME", environment.filePath("data").toUtf8());
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QApplication app(argc, argv);
    app.setOrganizationName("sticky-test"); app.setApplicationName("ui");
    app.setQuitOnLastWindowClosed(false);
    UiTest test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_ui.moc"
