// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QRect>
#include <QString>
#include <QtQml/qqmlregistration.h>

class NoteStore;
class QSystemTrayIcon;
class QMenu;
class QSettings;
class QQmlEngine;
class QJSEngine;

// Application-level services exposed to QML as the singleton `App` (module StickyNotes).
// main.cpp constructs the single instance and calls AppController::setInstance() before loading QML;
// create() hands that instance to the engine with CppOwnership.
//
// Settings (QSettings, org "clankercode", app "sticky-notes"): notebook geometry, current note id,
// desktopNotesVisible. Writes to QSettings are cheap; they are synced on quit.
class AppController : public QObject
{
    Q_OBJECT
    Q_MOC_INCLUDE("notestore.h")
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(NoteStore *store READ store CONSTANT)
    Q_PROPERTY(QString saveError READ saveError NOTIFY saveErrorChanged)
    Q_PROPERTY(QString recoveryDirectory READ recoveryDirectory NOTIFY recoveryDirectoryChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
    Q_PROPERTY(bool layerShell READ layerShell CONSTANT)
    Q_PROPERTY(bool startMinimized READ startMinimized CONSTANT)
    // Whether popped-out desktop notes are shown (tray "Hide/Show desktop notes").
    Q_PROPERTY(bool desktopNotesVisible READ desktopNotesVisible WRITE setDesktopNotesVisible NOTIFY desktopNotesVisibleChanged)
    // Whether ~/.config/autostart/code.clanker.StickyNotes.desktop exists (writes/removes it).
    Q_PROPERTY(bool startOnLogin READ startOnLogin WRITE setStartOnLogin NOTIFY startOnLoginChanged)
    // Notebook window geometry (x/y may be meaningless on Wayland; width/height always honoured).
    Q_PROPERTY(QRect notebookGeometry READ notebookGeometry WRITE setNotebookGeometry NOTIFY notebookGeometryChanged)
    Q_PROPERTY(QString currentNoteId READ currentNoteId WRITE setCurrentNoteId NOTIFY currentNoteIdChanged)

public:
    AppController(NoteStore *store, bool startMinimized, QObject *parent = nullptr);
    ~AppController() override;

    static void setInstance(AppController *instance);
    static AppController *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);

    NoteStore *store() const { return m_store; }
    QString saveError() const { return m_saveError; }
    QString recoveryDirectory() const { return m_recoveryDirectory; }
    Q_INVOKABLE void retrySave();
    Q_INVOKABLE void saveRecoveryAndQuit();
    Q_INVOKABLE void discardAndQuit(); // only after explicit user confirmation
    void prepareForShutdown(); // best effort recovery for forced/session shutdown
    QString version() const;
    bool trayAvailable() const;
    bool layerShell() const;
    bool startMinimized() const { return m_startMinimized; }
    bool desktopNotesVisible() const { return m_desktopNotesVisible; }
    void setDesktopNotesVisible(bool v);
    bool startOnLogin() const;
    void setStartOnLogin(bool v);
    QRect notebookGeometry() const { return m_notebookGeometry; }
    void setNotebookGeometry(const QRect &r);
    QString currentNoteId() const { return m_currentNoteId; }
    void setCurrentNoteId(const QString &id);

    // Creates the tray icon (no-op if no system tray). Called by main after QML is loaded.
    void setupTray();

    Q_INVOKABLE void openNotesFolder() const; // QDesktopServices::openUrl(notesDir)
    Q_INVOKABLE void openUrl(const QString &url) const;
    Q_INVOKABLE void quit();                  // flush; block exit on failure until recovery or explicit discard

public Q_SLOTS:
    // Requests from tray / second instance; QML (Main.qml) connects to the signals below.
    void requestShowNotebook();
    void requestToggleNotebook();
    void requestNewNote();

Q_SIGNALS:
    void saveErrorChanged();
    void recoveryDirectoryChanged();
    void preparingToQuit(); // synchronous last-moment geometry/state commit
    void showNotebookRequested();
    void toggleNotebookRequested();
    void newNoteRequested();
    void desktopNotesVisibleChanged();
    void startOnLoginChanged();
    void notebookGeometryChanged();
    void currentNoteIdChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool saveRecovery();
    QString m_saveError;
    QString m_recoveryDirectory;
    bool m_discardOnQuit = false;
    bool m_recoverySaved = false;
    NoteStore *m_store;
    QSettings *m_settings = nullptr;
    bool m_startMinimized;
    bool m_desktopNotesVisible = true;
    QRect m_notebookGeometry;
    QString m_currentNoteId;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
};
