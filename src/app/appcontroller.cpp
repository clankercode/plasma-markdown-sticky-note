// SPDX-License-Identifier: MIT
#include "appcontroller.h"
#include "desktopplacement.h"
#include "notestore.h"
#include "note.h"

#include <QAction>
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QKeyEvent>
#include <QWindow>
#include <QMenu>
#include <QQmlEngine>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QUuid>

namespace {
AppController *s_instance = nullptr;
QString autostartPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/code.clanker.StickyNotes.desktop");
}
QString desktopExecArgument(QString value)
{
    // Desktop entry string escaping is applied after Exec argument escaping.
    value.replace(QLatin1Char('%'), QStringLiteral("%%"));
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    value.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    value.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    value = QLatin1Char('"') + value + QLatin1Char('"');
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    return value;
}
}

AppController::AppController(NoteStore *store, bool startMinimized, QObject *parent)
    : QObject(parent), m_store(store), m_startMinimized(startMinimized)
{
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &AppController::prepareForShutdown);
    connect(store, &NoteStore::saveFailed, this, [this](const QString &path, const QString &error) {
        m_saveError = tr("Could not save %1: %2. Your edits are still in memory.").arg(path, error);
        Q_EMIT saveErrorChanged();
        requestShowNotebook();
    });
    connect(store, &NoteStore::noteAdded, this, [this](Note *note) {
        connect(note, &Note::dirtied, this, [this] { m_recoverySaved = false; });
    });
    for (auto *note : store->notes())
        connect(note, &Note::dirtied, this, [this] { m_recoverySaved = false; });
    QCoreApplication::instance()->installEventFilter(this);
    m_settings = new QSettings(this);
    m_desktopNotesVisible = m_settings->value(QStringLiteral("desktopNotesVisible"), true).toBool();
    m_notebookGeometry = m_settings->value(QStringLiteral("notebookGeometry"), QRect(100, 100, 460, 420)).toRect();
    if (m_notebookGeometry.width() < 160 || m_notebookGeometry.height() < 120)
        m_notebookGeometry = QRect(100, 100, 460, 420);
    m_currentNoteId = m_settings->value(QStringLiteral("currentNoteId")).toString();
}

AppController::~AppController()
{
    delete m_trayMenu;
    if (s_instance == this)
        s_instance = nullptr;
    m_settings->sync();
}

void AppController::setInstance(AppController *instance) { s_instance = instance; }

AppController *AppController::create(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine)
    Q_UNUSED(jsEngine)
    Q_ASSERT(s_instance);
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

QString AppController::version() const { return QStringLiteral(STICKY_NOTES_VERSION); }
bool AppController::trayAvailable() const { return QSystemTrayIcon::isSystemTrayAvailable(); }
bool AppController::layerShell() const { return DesktopPlacement::layerShellAvailable(); }

void AppController::setDesktopNotesVisible(bool v)
{
    if (v == m_desktopNotesVisible)
        return;
    m_desktopNotesVisible = v;
    m_settings->setValue(QStringLiteral("desktopNotesVisible"), v);
    Q_EMIT desktopNotesVisibleChanged();
}

bool AppController::startOnLogin() const { return QFileInfo::exists(autostartPath()); }

void AppController::setStartOnLogin(bool v)
{
    if (v == startOnLogin())
        return;
    const QString path = autostartPath();
    if (v) {
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            qWarning() << "Cannot create autostart directory";
            return;
        }
        const QString binary = QCoreApplication::applicationFilePath();
        const QString entry = QStringLiteral(
            "[Desktop Entry]\nType=Application\nName=Sticky Notes\n"
            "Comment=Markdown sticky notes for your desktop\nExec=%1 --minimized\n"
            "Icon=code.clanker.StickyNotes\nTerminal=false\n"
            "X-GNOME-Autostart-enabled=true\n")
                                  .arg(desktopExecArgument(binary));
        QSaveFile file(path);
        const QByteArray data = entry.toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            qWarning() << "Cannot write autostart entry:" << file.errorString();
            return;
        }
    } else if (!QFile::remove(path)) {
        qWarning() << "Cannot remove autostart entry:" << path;
        return;
    }
    Q_EMIT startOnLoginChanged();
}

void AppController::setNotebookGeometry(const QRect &r)
{
    if (r == m_notebookGeometry || r.width() < 160 || r.height() < 120)
        return;
    m_notebookGeometry = r;
    m_settings->setValue(QStringLiteral("notebookGeometry"), r);
    Q_EMIT notebookGeometryChanged();
}

void AppController::setCurrentNoteId(const QString &id)
{
    if (id == m_currentNoteId)
        return;
    m_currentNoteId = id;
    m_settings->setValue(QStringLiteral("currentNoteId"), id);
    Q_EMIT currentNoteIdChanged();
}

void AppController::setupTray()
{
    if (m_tray || !trayAvailable())
        return;
    m_trayMenu = new QMenu;
    m_trayMenu->addAction(tr("New note"), this, &AppController::requestNewNote);
    m_trayMenu->addAction(tr("Show notebook"), this, &AppController::requestShowNotebook);
    auto *visible = m_trayMenu->addAction(tr("Show desktop notes"));
    visible->setCheckable(true);
    const auto syncVisible = [this, visible] {
        visible->setChecked(desktopNotesVisible());
        visible->setText(desktopNotesVisible() ? tr("Hide all desktop notes") : tr("Show all desktop notes"));
    };
    syncVisible();
    connect(visible, &QAction::triggered, this, &AppController::setDesktopNotesVisible);
    connect(this, &AppController::desktopNotesVisibleChanged, visible, syncVisible);
    auto *autostart = m_trayMenu->addAction(tr("Start on login"));
    autostart->setCheckable(true);
    autostart->setChecked(startOnLogin());
    connect(autostart, &QAction::triggered, this, [this, autostart](bool checked) {
        setStartOnLogin(checked);
        autostart->setChecked(startOnLogin());
    });
    connect(this, &AppController::startOnLoginChanged, autostart, [this, autostart] {
        autostart->setChecked(startOnLogin());
    });
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tr("Quit"), this, &AppController::quit);
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/icons/sticky-notes.svg")), this);
    m_tray->setToolTip(tr("Sticky Notes"));
    m_tray->setContextMenu(m_trayMenu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            requestToggleNotebook();
    });
    m_tray->show();
}

void AppController::openNotesFolder() const
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_store->notesDir()));
}

void AppController::openUrl(const QString &url) const
{
    const QUrl parsed(url);
    // Do not let a link in an externally edited Markdown note execute a local file/handler.
    if (parsed.isValid() && (parsed.scheme() == QStringLiteral("https")
                            || parsed.scheme() == QStringLiteral("http")
                            || parsed.scheme() == QStringLiteral("mailto")))
        QDesktopServices::openUrl(parsed);
}

bool AppController::eventFilter(QObject *watched, QEvent *event)
{
    // One owner for application shortcuts, including when the notebook is hidden.
    if (qobject_cast<QWindow *>(watched)
        && (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress)) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->modifiers() == Qt::ControlModifier && (key->key() == Qt::Key_N || key->key() == Qt::Key_Q)) {
            event->accept();
            if (event->type() == QEvent::KeyPress && !key->isAutoRepeat()) {
                if (key->key() == Qt::Key_N) requestNewNote(); else quit();
            }
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void AppController::retrySave()
{
    m_store->flush();
    if (!m_store->hasUnsavedNotes()) {
        m_saveError.clear();
        Q_EMIT saveErrorChanged();
    }
}
bool AppController::saveRecovery()
{
    if (!m_store->hasUnsavedNotes()) return true;
    const auto suffix = QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-"))
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QStringList roots{
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/recovery"),
        QDir::tempPath() + QStringLiteral("/sticky-notes-recovery")};
    for (const auto &root : roots) {
        const auto path = QDir(root).filePath(suffix);
        // Do not put the fallback on the failing notes location.
        if (path == m_store->notesDir() || path.startsWith(m_store->notesDir() + '/')) continue;
        if (!m_store->saveRecoveryCopies(path)) continue;
        m_recoveryDirectory = path;
        m_recoverySaved = true;
        Q_EMIT recoveryDirectoryChanged();
        qInfo().noquote() << "Unsaved notes recovered to" << path;
        return true;
    }
    m_saveError = tr("Could not save a recovery copy. Keep the app open and restore a writable storage location, or explicitly discard your edits.");
    Q_EMIT saveErrorChanged();
    return false;
}
void AppController::prepareForShutdown()
{
    Q_EMIT preparingToQuit();
    m_store->flush();
    if (!m_discardOnQuit && !m_recoverySaved && m_store->hasUnsavedNotes() && !saveRecovery())
        qCritical() << "Cannot preserve unsaved notes during shutdown:" << m_saveError;
    m_settings->sync();
}
void AppController::saveRecoveryAndQuit()
{
    Q_EMIT preparingToQuit();
    retrySave();
    if (!saveRecovery()) return;
    m_settings->sync();
    QCoreApplication::quit();
}
void AppController::discardAndQuit()
{
    m_discardOnQuit = true;
    Q_EMIT preparingToQuit();
    m_settings->sync();
    QCoreApplication::quit();
}
void AppController::quit()
{
    Q_EMIT preparingToQuit();
    retrySave();
    if (m_store->hasUnsavedNotes()) {
        requestShowNotebook();
        return;
    }
    m_settings->sync();
    QCoreApplication::quit();
}

void AppController::requestShowNotebook() { Q_EMIT showNotebookRequested(); }
void AppController::requestToggleNotebook() { Q_EMIT toggleNotebookRequested(); }
void AppController::requestNewNote() { Q_EMIT newNoteRequested(); }
