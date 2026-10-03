#include "browser/SingleInstance.h"
#include "core/Logging.h"

#include <QLocalServer>
#include <QLocalSocket>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

QString SingleInstance::serverName()
{
#ifdef Q_OS_UNIX
    return QStringLiteral("com.cosmic.browser.instance.")
        + QString::number(::getuid());
#else
    return QStringLiteral("com.cosmic.browser.instance.default");
#endif
}

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

SingleInstance::~SingleInstance()
{
    delete m_server;
}

bool SingleInstance::acquire()
{
    {
        QLocalSocket probe;
        probe.connectToServer(serverName());
        if (probe.waitForConnected(500)) {
            return false;
        }
    }
    QLocalServer::removeServer(serverName());
    m_server = new QLocalServer(this);
    if (!m_server->listen(serverName())) {
        qCWarning(lcCosmic).noquote() << "single-instance listen failed:"
                                      << m_server->errorString();
        delete m_server;
        m_server = nullptr;
        return false;
    }
    connect(m_server, &QLocalServer::newConnection, this,
            &SingleInstance::onNewConnection);
    return true;
}

bool SingleInstance::forward(const QString &message)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(1500)) {
        return false;
    }
    socket.write(message.toUtf8() + '\n');
    socket.waitForBytesWritten(1500);
    socket.disconnectFromServer();
    return true;
}

void SingleInstance::onNewConnection()
{
    if (!m_server) {
        return;
    }
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            const QString message =
                QString::fromUtf8(socket->readAll()).trimmed();
            socket->deleteLater();
            emit messageReceived(message);
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        if (socket->bytesAvailable() > 0) {
            const QString message =
                QString::fromUtf8(socket->readAll()).trimmed();
            socket->deleteLater();
            emit messageReceived(message);
        }
    }
}
