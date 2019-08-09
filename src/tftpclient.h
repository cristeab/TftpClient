#pragma once

#include <QUdpSocket>
#include "qmlhelpers.h"

class TftpClient : public QObject
{
    Q_OBJECT
    QML_WRITABLE_PROPERTY(QString, workingFolder, setWorkingFolder, "")
public:
    explicit TftpClient(QObject *parent = nullptr);
    Q_INVOKABLE void startDownload(const QString &hosts, const QString &files);
    Q_INVOKABLE void stopDownload();
signals:
    void error(const QString &msg);
private:
    enum { DEFAULT_PORT = 69, MAX_PACKET_SIZE = 512, READ_DELAY_MS = 1000 };
    bool put(const QString &filename);
    bool get(const QString &filename);
    bool bindSocket();
    QByteArray getFilePacket(const QString &filename);
    QByteArray putFilePacket(const QString &filename);
    QScopedPointer<QUdpSocket> _socket;
    QString _serverAddress;
    uint16_t _serverPort = DEFAULT_PORT;
};
