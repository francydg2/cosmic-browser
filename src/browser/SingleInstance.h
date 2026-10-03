#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;

    static QString serverName();

    bool acquire();
    bool isPrimary() const { return m_server != nullptr; }
    static bool forward(const QString &message);

signals:
    void messageReceived(const QString &message);

private:
    void onNewConnection();

    QLocalServer *m_server = nullptr;
};
