#pragma once

#include <QUdpSocket>
#include <atomic>
#include "qmlhelpers.h"

class TftpClient : public QObject
{
    Q_OBJECT
    QML_WRITABLE_PROPERTY(QString, hosts, setHosts, "")
    QML_WRITABLE_PROPERTY(QString, files, setFiles, "")
    QML_WRITABLE_PROPERTY(QString, workingFolder, setWorkingFolder, "")
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    QML_READABLE_PROPERTY(int, addrCount, setAddrCount, 0)
    QML_READABLE_PROPERTY(int, addrIndex, setAddrIndex, 0)
public:
    explicit TftpClient(QObject *parent = nullptr);
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void stopDownload();
    Q_INVOKABLE QString toLocalFile(const QUrl &url);
    bool running() const { return _running; }
    void setRunning(bool val) { _running = val; }
signals:
    void error(const QString &title, const QString &msg);
    void info(const QString &msg);
    void runningChanged();
private:
    enum { DEFAULT_PORT = 69, MAX_PACKET_SIZE = 512, READ_DELAY_MS = 1000 };
    bool parseAddressList(const QString &hosts);
    bool parseFileList(const QString &files);
    void downloadFileList(const QString &address);
    void dumpStats();
    bool put(const QString &serverAddress, const QString &filename);
    bool get(const QString &serverAddress, const QString &filename);
    bool bindSocket();
    void updateInfo();
    QByteArray getFilePacket(const QString &filename);
    QByteArray putFilePacket(const QString &filename);
    QScopedPointer<QUdpSocket> _socket;
    uint16_t _serverPort = DEFAULT_PORT;
    QStringList _filesList;
    struct Stats {
        QString address;
        QString filename;
    };
    QVector<Stats> _stats;
    std::atomic<bool> _running;
    QVector<QString> _singleAddresses;
    QVector<QPair<quint32, quint32> > _pairAddresses;
};
