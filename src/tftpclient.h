#pragma once

#include "qmlhelpers.h"
#include "ctpl_stl.h"
#include <QUdpSocket>
#include <atomic>
#include <QMutex>

class TftpClient : public QObject
{
    Q_OBJECT
    QML_WRITABLE_PROPERTY(QString, hosts, setHosts, "")
    QML_WRITABLE_PROPERTY(QString, files, setFiles, "")
    QML_WRITABLE_PROPERTY(QString, workingFolder, setWorkingFolder, "")
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY fileCountChanged)
    QML_READABLE_PROPERTY(int, addrCount, setAddrCount, 0)
    QML_READABLE_PROPERTY(int, addrIndex, setAddrIndex, 0)
    QML_READABLE_PROPERTY(QString, currentAddress, setCurrentAddress, "")
    QML_READABLE_PROPERTY(int, fileIndex, setFileIndex, 0)
    //settings props
    QML_WRITABLE_PROPERTY(int, serverPort, setServerPort, DEFAULT_PORT)
    QML_WRITABLE_PROPERTY(int, readDelayMs, setReadDelayMs, READ_DELAY_MS)
    QML_WRITABLE_PROPERTY(int, numWorkers, setNumWorkers, DEFAULT_NUM_WORKERS)
public:
    explicit TftpClient(QObject *parent = nullptr);
    Q_INVOKABLE void startDownload();
    Q_INVOKABLE void stopDownload();
    Q_INVOKABLE QString toLocalFile(const QUrl &url);
    Q_INVOKABLE bool parseAddressList();
    Q_INVOKABLE bool parseFileList();
    bool running() const { return _running; }
    void setRunning(bool val) {
        if (_running != val) {
            _running = val;
            emit runningChanged();
        }
    }
    int fileCount() const { return _filesList.size(); }
signals:
    void error(const QString &title, const QString &msg);
    void info(const QString &msg);
    void runningChanged();
    void fileCountChanged();
private:
    enum { DEFAULT_PORT = 69, MAX_PACKET_SIZE = 512, READ_DELAY_MS = 1000,
           DEFAULT_NUM_WORKERS = 4 };
    void downloadFileList(const QString &address);
    void dumpStats();
    bool get(int i, const QString &serverAddress, const QString &filename);
    bool bindSocket(int i);
    void updateInfo();
    QByteArray getFilePacket(const QString &filename);
    QByteArray putFilePacket(const QString &filename);
    struct SocketInfo {
        QUdpSocket socket;
        QString lastError;
    };
    QScopedPointer<SocketInfo> _socketInfo;
    QStringList _filesList;
    QMap<QString, QString> _stats;//address is the key
    QMutex _statsMutex;
    std::atomic<bool> _running;
    QVector<QString> _singleAddresses;
    QVector<QPair<quint32, quint32> > _pairAddresses;
    ctpl::thread_pool _threadPool;
};
