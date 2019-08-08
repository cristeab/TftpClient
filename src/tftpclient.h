#pragma once

#include <QObject>

class QUdpSocket;

class TftpClient : public QObject
{
    Q_OBJECT
public:
    explicit TftpClient(QObject *parent = nullptr);

    Q_INVOKABLE bool put(const QString &filename);
    Q_INVOKABLE bool get(const QString &filename);

private:
    bool bindSocket();
    QByteArray getFilePacket(const QString &filename);
    QByteArray putFilePacket(const QString &filename);
    QScopedPointer<QUdpSocket> _socket;
};
