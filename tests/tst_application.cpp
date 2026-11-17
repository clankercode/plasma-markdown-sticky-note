// SPDX-License-Identifier: MIT
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_UNIX
#include <csignal>
#include <unistd.h>
#endif

class ApplicationTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void gracefulShutdown_data()
    {
        QTest::addColumn<int>("signalNumber");
#ifdef Q_OS_UNIX
        QTest::newRow("SIGTERM") << SIGTERM;
        QTest::newRow("SIGINT") << SIGINT;
#endif
    }

    void gracefulShutdown()
    {
#ifdef Q_OS_UNIX
        QFETCH(int, signalNumber);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString notes = temporary.filePath(QStringLiteral("notes"));
        const QString config = temporary.filePath(QStringLiteral("config"));
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        environment.insert(QStringLiteral("XDG_DATA_HOME"), temporary.filePath(QStringLiteral("data")));
        environment.insert(QStringLiteral("XDG_CACHE_HOME"), temporary.filePath(QStringLiteral("cache")));
        QProcess process;
        process.setProcessEnvironment(environment);
        process.start(QString::fromLocal8Bit(STICKY_NOTES_BINARY),
                      {QStringLiteral("--notes-dir"), notes, QStringLiteral("--new")});
        QVERIFY(process.waitForStarted());
        QTRY_COMPARE_WITH_TIMEOUT(QDir(notes).entryList({QStringLiteral("*.md")}, QDir::Files).size(), 1, 5000);
        QCOMPARE(::kill(process.processId(), signalNumber), 0);
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QByteArray diagnostics = process.readAllStandardError();
        // Raising is deliberately unsupported by Qt's offscreen platform, not a QML failure.
        diagnostics.replace("This plugin does not support raise()\n", "");
        QVERIFY2(diagnostics.isEmpty(), diagnostics.constData());
        QFile settings(config + QStringLiteral("/clankercode/sticky-notes.conf"));
        QVERIFY(settings.open(QIODevice::ReadOnly));
        QVERIFY(settings.readAll().contains("currentNoteId="));
#else
        QSKIP("POSIX shutdown signals are only available on Unix");
#endif
    }
};

QTEST_GUILESS_MAIN(ApplicationTest)
#include "tst_application.moc"
