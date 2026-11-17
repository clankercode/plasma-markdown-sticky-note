// SPDX-License-Identifier: MIT
#include "appcontroller.h"
#include "notestore.h"
#include "singleinstance.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QIcon>
#include <QPalette>
#include <QMessageBox>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSocketNotifier>
#include <QSessionManager>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t shutdownSocket = -1;

void requestShutdown(int)
{
    // Only async-signal-safe operations belong here. Qt and disk I/O run on the main thread.
    const int savedErrno = errno;
    const char byte = 1;
    if (shutdownSocket >= 0) {
        ssize_t result;
        do {
            result = ::write(static_cast<int>(shutdownSocket), &byte, sizeof(byte));
        } while (result < 0 && errno == EINTR);
    }
    errno = savedErrno;
}

class ShutdownSignals
{
public:
    bool install(QObject *context)
    {
        if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, sockets) != 0)
            return false;
        notifier = new QSocketNotifier(sockets[0], QSocketNotifier::Read, context);
        QObject::connect(notifier, &QSocketNotifier::activated, context, [this] {
            notifier->setEnabled(false);
            QCoreApplication::quit(); // aboutToQuit flushes notes, destructors sync settings.
        });
        struct sigaction action {};
        action.sa_handler = requestShutdown;
        ::sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESTART;
        shutdownSocket = sockets[1];
        if (::sigaction(SIGTERM, &action, &previousTerm) != 0)
            return false;
        termInstalled = true;
        if (::sigaction(SIGINT, &action, &previousInt) != 0)
            return false;
        intInstalled = true;
        return true;
    }

    ~ShutdownSignals()
    {
        if (termInstalled)
            ::sigaction(SIGTERM, &previousTerm, nullptr);
        if (intInstalled)
            ::sigaction(SIGINT, &previousInt, nullptr);
        shutdownSocket = -1;
        delete notifier;
        for (int socket : sockets) {
            if (socket >= 0)
                ::close(socket);
        }
    }

private:
    int sockets[2] = {-1, -1};
    QSocketNotifier *notifier = nullptr;
    struct sigaction previousTerm {};
    struct sigaction previousInt {};
    bool termInstalled = false;
    bool intInstalled = false;
};
}
#endif

int main(int argc, char *argv[])
{
    QElapsedTimer startup;
    startup.start();
    QApplication app(argc, argv);
#ifdef Q_OS_UNIX
    ShutdownSignals shutdown;
    if (!shutdown.install(&app)) {
        qCritical() << "Cannot install graceful shutdown signal handlers";
        return 1;
    }
#endif
    QCoreApplication::setOrganizationName(QStringLiteral("clankercode"));
    QCoreApplication::setApplicationName(QStringLiteral("sticky-notes"));
    QCoreApplication::setApplicationVersion(QStringLiteral(STICKY_NOTES_VERSION));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Sticky Notes"));
    QGuiApplication::setDesktopFileName(QStringLiteral(STICKY_NOTES_APP_ID));
    QIcon icon(QStringLiteral(":/icons/sticky-notes.svg"));
    if (icon.isNull())
        icon = QIcon::fromTheme(QStringLiteral(STICKY_NOTES_APP_ID));
    QApplication::setWindowIcon(icon);
    // Qt's Markdown importer takes link colours from the application palette.
    QPalette palette = app.palette();
    palette.setColor(QPalette::Link, QColor(QStringLiteral("#1f3c8a")));
    palette.setColor(QPalette::LinkVisited, QColor(QStringLiteral("#1f3c8a")));
    app.setPalette(palette);

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Fast Markdown sticky notes for your desktop."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption directory(QStringLiteral("notes-dir"), QStringLiteral("Read and save notes in this directory."), QStringLiteral("path"));
    const QCommandLineOption minimized(QStringLiteral("minimized"), QStringLiteral("Start with the notebook hidden."));
    const QCommandLineOption newNote(QStringLiteral("new"), QStringLiteral("Create a new note (in the running instance, if any)."));
    parser.addOptions({directory, minimized, newNote});
    parser.process(app);
    QString notesDir = parser.isSet(directory) ? parser.value(directory) : qEnvironmentVariable("STICKY_NOTES_DIR");
    if (notesDir.isEmpty())
        notesDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/sticky-notes/notes");
    notesDir = QDir(notesDir).absolutePath();

    SingleInstance instance(notesDir);
    if (!instance.tryBecomePrimary(parser.isSet(newNote) ? QStringLiteral("new") : QStringLiteral("show")))
        return 0;

    NoteStore store(notesDir);
    if (!store.load()) {
        qCritical().noquote() << "Cannot load notes:" << store.errorString();
        if (QGuiApplication::platformName() != QStringLiteral("offscreen")
            && QGuiApplication::platformName() != QStringLiteral("minimal"))
            QMessageBox::critical(nullptr, QStringLiteral("Sticky Notes"), store.errorString());
        return 1;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &store, &NoteStore::flush);
    QObject::connect(&store, &NoteStore::saveFailed, &app, [](const QString &path, const QString &error) {
        qWarning().noquote() << "Cannot save note" << path << error;
    });
    if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE"))
        QQuickStyle::setStyle(QStringLiteral("Basic"));

    AppController controller(&store, parser.isSet(minimized));
    AppController::setInstance(&controller);
    QObject::connect(&app, &QGuiApplication::commitDataRequest, &controller,
                     [&controller](QSessionManager &) { controller.prepareForShutdown(); }, Qt::DirectConnection);
    if (controller.trayAvailable())
        app.setQuitOnLastWindowClosed(false);
    QObject::connect(&instance, &SingleInstance::messageReceived, &controller, [&controller](const QString &message) {
        if (message == QStringLiteral("new"))
            controller.requestNewNote();
        else if (message == QStringLiteral("show"))
            controller.requestShowNotebook();
    });

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(1);
    }, Qt::QueuedConnection);
    engine.loadFromModule("StickyNotes", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    if (qEnvironmentVariableIntValue("STICKY_NOTES_QUIT_AFTER_FIRST_FRAME") == 1) {
        // Diagnostic-only benchmark: time from main() entry to the notebook's first submitted frame.
        auto *notebook = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()->property("notebook").value<QObject *>());
        if (notebook) {
            QObject::connect(notebook, &QQuickWindow::frameSwapped, &app, [&app, startup, measured = false]() mutable {
                if (measured)
                    return;
                measured = true;
                qInfo().noquote() << "First notebook frame:" << startup.nsecsElapsed() / 1000000.0 << "ms";
                app.quit();
            }, Qt::QueuedConnection);
        }
    }
    controller.setupTray();
    if (parser.isSet(newNote))
        QTimer::singleShot(0, &controller, &AppController::requestNewNote);
    return app.exec();
}
