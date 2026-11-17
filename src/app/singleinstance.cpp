// SPDX-License-Identifier: MIT
#include "singleinstance.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

namespace {
QString canonicalDirectory(const QString &path)
{
    const QFileInfo info(QDir(path).absolutePath());
    const QString canonical = info.canonicalFilePath();
    if (!canonical.isEmpty())
        return canonical;
    // Resolve existing parent symlinks even when the notes directory is new.
    if (info.absoluteFilePath() == info.absolutePath())
        return info.absoluteFilePath();
    return QDir(canonicalDirectory(info.absolutePath())).filePath(info.fileName());
}
}

SingleInstance::SingleInstance(const QString &notesDir, QObject *parent)
    : QObject(parent), m_server(new QLocalServer(this))
{
    const QString user = qEnvironmentVariable("USER", QDir::homePath());
    const QByteArray key = (user + canonicalDirectory(notesDir)).toUtf8();
    m_name = QStringLiteral("code.clanker.StickyNotes-")
        + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(12));
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = m_server->nextPendingConnection()) {
            auto receive = [this, socket] {
                if (socket->bytesAvailable() > 4096) {
                    socket->abort();
                    return;
                }
                while (socket->canReadLine()) {
                    const QString message = QString::fromUtf8(socket->readLine()).trimmed();
                    qDebug() << "Single-instance message:" << message;
                    Q_EMIT messageReceived(message);
                }
            };
            connect(socket, &QLocalSocket::readyRead, this, receive);
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(1000, socket, [socket] { socket->disconnectFromServer(); });
            receive();
        }
    });
}

bool SingleInstance::tryBecomePrimary(const QString &message)
{
    const auto send = [this, &message] {
        QLocalSocket socket;
        socket.connectToServer(m_name);
        if (!socket.waitForConnected(150))
            return false;
        socket.write(message.toUtf8() + '\n');
        if (socket.bytesToWrite() > 0)
            socket.waitForBytesWritten(500);
        socket.disconnectFromServer();
        return true;
    };
    if (send())
        return false;
    if (m_server->listen(m_name))
        return true;
    // A competing launch may have won the listen race. Never unlink a live server.
    if (send())
        return false;
    QLocalServer::removeServer(m_name);
    if (m_server->listen(m_name))
        return true;
    if (send())
        return false;
    qCritical() << "Cannot create single-instance server:" << m_server->errorString();
    // Continue without a guard rather than silently refusing to start.
    return true;
}
