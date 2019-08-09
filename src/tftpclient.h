#pragma once

#include <QUdpSocket>
#include <atomic>
#include "qmlhelpers.h"

class TftpClient : public QObject
{
    Q_OBJECT
    QML_WRITABLE_PROPERTY(QString, workingFolder, setWorkingFolder, "")
    QML_READABLE_PROPERTY(bool, inProgress, setInProgress, false)
public:
    explicit TftpClient(QObject *parent = nullptr);
    Q_INVOKABLE void startDownload(const QString &hosts, const QString &files);
    Q_INVOKABLE void stopDownload();
signals:
    void error(const QString &title, const QString &msg);
    void info(const QString &msg);
private:
    enum { DEFAULT_PORT = 69, MAX_PACKET_SIZE = 512, READ_DELAY_MS = 1000 };
    bool parseFileList(const QString &files);
    void downloadFileList(const QString &address);
    void dumpStats();
    bool put(const QString &serverAddress, const QString &filename);
    bool get(const QString &serverAddress, const QString &filename);
    bool bindSocket();
    QByteArray getFilePacket(const QString &filename);
    QByteArray putFilePacket(const QString &filename);
    QScopedPointer<QUdpSocket> _socket;
    uint16_t _serverPort = DEFAULT_PORT;
    QStringList _files;
    struct Stats {
        QString address;
        QString filename;
    };
    QVector<Stats> _stats;
    std::atomic<bool> _running;
};
