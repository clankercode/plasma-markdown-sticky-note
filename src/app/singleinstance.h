// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// Per-user (and per notes directory) single-instance guard over QLocalServer.
// Server name: "code.clanker.StickyNotes-" + first 12 hex chars of sha1(username + canonical notesDir).
//
// tryBecomePrimary(): if another instance is listening, sends `message` (UTF-8, newline-terminated) to it
// and returns false. Otherwise starts listening (removing a stale socket first) and returns true.
// The primary emits messageReceived for each message (e.g. "show", "new").
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &notesDir, QObject *parent = nullptr);
    bool tryBecomePrimary(const QString &message);
    QString serverName() const { return m_name; }

Q_SIGNALS:
    void messageReceived(const QString &message);

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};
