#include "tftpclient.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <thread>

TftpClient::TftpClient(QObject *parent) : QObject(parent)
{
    setWorkingFolder(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    setObjectName("client");
    setRunning(false);

    connect(this, &TftpClient::numWorkersChanged, [&]() {
        // BIND OUR LOCAL SOCKET TO AN IP ADDRESS AND PORT
        _socketInfo.reset(new SocketInfo[_numWorkers]);
        for (int i = 0; i < _numWorkers; ++i) {
            bindSocket(i);
        }
    });

    _numWorkers = static_cast<int>(std::thread::hardware_concurrency());
    if (2 > _numWorkers) {
        _numWorkers = DEFAULT_NUM_WORKERS;
    }
    emit numWorkersChanged();
}

void TftpClient::startDownload()
{
    _stats.clear();
    updateInfo();
    setRunning(true);

    std::thread th([this]() {
        _threadPool.init();
        _threadPool.resize(_numWorkers);
        bool stopped = false;
        setAddrIndex(0);
        for (const auto &ip: _singleAddresses) {
            downloadFileList(ip);
            if (!_running) {
                qWarning() << "Stopped by user";
                stopped = true;
                break;
            }
        }
        if (!stopped) {
            for (const auto &pairIp: _pairAddresses) {
                for (quint32 ipNum = pairIp.first; ipNum <= pairIp.second; ++ipNum) {
                    QHostAddress hostAddr(ipNum);
                    downloadFileList(hostAddr.toString());
                    if (!_running) {
                        qWarning() << "Stopped by user";
                        stopped = true;
                        break;
                    }
                }
                if (stopped) {
                    break;
                }
            }
        }
        //wait until all threads finish
        _threadPool.stop(_running);
        dumpStats();
        setRunning(false);
    });
    th.detach();
}

void TftpClient::stopDownload()
{
    setRunning(false);
}

QString TftpClient::toLocalFile(const QUrl &url)
{
    QString out;
    if (url.isLocalFile()) {
        out = QDir::toNativeSeparators(url.toLocalFile());
    } else {
        out = url.toString();
    }
    return out;
}

bool TftpClient::get(int i, const QString &serverAddress,
                     const QString &filename)
{
    if ((0 > i) || (_numWorkers <= i)) {
        qCritical() << "Invalid index";
        return false;
    }

    SocketInfo *sockInfo = _socketInfo.get() + i;
    if (serverAddress.isEmpty()) {
        sockInfo->lastError = tr("Server address cannot be empty");
        qCritical() << sockInfo->lastError;
        return false;
    }
    if (filename.isEmpty()) {
        sockInfo->lastError = tr("Filename cannot be empty");
        qCritical() << sockInfo->lastError;
        return false;
    }

    // MAKE A LOCAL COPY OF THE REMOTE HOST ADDRESS AND PORT NUMBER
    const QHostAddress hostAddress(serverAddress);

    // CLEAN OUT ANY INCOMING PACKETS
    while (sockInfo->socket.hasPendingDatagrams()){
        QByteArray byteArray;
        byteArray.resize(static_cast<int>(sockInfo->socket.pendingDatagramSize()));
        sockInfo->socket.readDatagram(byteArray.data(), byteArray.length());
    }

    // CREATE REQUEST PACKET AND SEND TO HOST
    // WAIT UNTIL MESSAGE HAS BEEN SENT, QUIT IF TIMEOUT IS REACHED
    QByteArray reqPacket=getFilePacket(filename);
    if (sockInfo->socket.writeDatagram(reqPacket, hostAddress, _serverPort) != reqPacket.length()) {
        sockInfo->lastError =  QString("Cannot send packet to host : %1").arg(sockInfo->socket.errorString());
        qCritical() << sockInfo->lastError;
        return false;
    }

    // CREATE PACKET COUNTERS TO KEEP TRACK OF MESSAGES
    unsigned short incomingPacketNumber = 1;
    unsigned short outgoingPacketNumber = 1;

    // NOW WAIT HERE FOR INCOMING DATA
    bool messageCompleteFlag = false;
    QByteArray requestedFile;
    while (!messageCompleteFlag){
        // WAIT FOR AN INCOMING PACKET
        if (sockInfo->socket.hasPendingDatagrams() ||
                sockInfo->socket.waitForReadyRead(_readDelayMs)) {
            // ITERATE HERE AS LONG AS THERE IS ATLEAST A
            // PACKET HEADER'S WORTH OF DATA TO READ
            QByteArray incomingDatagram;
            incomingDatagram.resize(static_cast<int>(sockInfo->socket.pendingDatagramSize()));
            sockInfo->socket.readDatagram(incomingDatagram.data(), incomingDatagram.length());

            // MAKE SURE FIRST BYTE IS 0
            char *buffer=incomingDatagram.data();
            char zeroByte=buffer[0];
            if (zeroByte != 0x00) {
                sockInfo->lastError = QString("Incoming packet has invalid first byte (%1).").arg(static_cast<int>(zeroByte));
                qCritical() << sockInfo->lastError;
                return false;
            }

            // READ UNSIGNED SHORT PACKET NUMBER USING LITTLE ENDIAN FORMAT
            // FOR THE INCOMING UNSIGNED SHORT VALUE BUT BIG ENDIAN FOR THE
            // INCOMING DATA PACKET
            unsigned short incomingMessageCounter;
            char *const inPtr = reinterpret_cast<char*>(&incomingMessageCounter);
            inPtr[1] = buffer[2];
            inPtr[0] = buffer[3];

            // CHECK INCOMING MESSAGE ID NUMBER AND MAKE SURE IT MATCHES
            // WHAT WE ARE EXPECTING, OTHERWISE WE'VE LOST OR GAINED A PACKET
            if (incomingMessageCounter == incomingPacketNumber){
                incomingPacketNumber++;
            } else {
                sockInfo->lastError = QString("Error on incoming packet number %1 vs expected %2").arg(incomingMessageCounter).arg(incomingPacketNumber);
                qCritical() << sockInfo->lastError;
                return false;
            }

            // COPY THE INCOMING FILE DATA
            QByteArray incomingByteArray(&buffer[4], incomingDatagram.length()-4);

            // SEE IF WE RECEIVED A COMPLETE 512 BYTES AND IF SO,
            // THEN THERE IS MORE INFORMATION ON THE WAY
            // OTHERWISE, WE'VE REACHED THE END OF THE RECEIVING FILE
            if (incomingByteArray.length() < MAX_PACKET_SIZE) {
                messageCompleteFlag=true;
            }

            // APPEND THE INCOMING DATA TO OUR COMPLETE FILE
            requestedFile.append(incomingByteArray);

            // CHECK THE OPCODE FOR ANY ERROR CONDITIONS
            char opCode=buffer[1];
            if (opCode != 0x03) { /* ack packet should have code 3 (data) and should be ack+1 the packet we just sent */
                sockInfo->lastError = QString("Incoming packet returned invalid operation code (%1).").arg(static_cast<int>(opCode));
                qCritical() << sockInfo->lastError;
                return false;
            } else {
                // SEND PACKET ACKNOWLEDGEMENT BACK TO HOST REFLECTING THE INCOMING PACKET NUMBER
                QByteArray ackByteArray;
                ackByteArray.append(static_cast<char>(0x00));
                ackByteArray.append(static_cast<char>(0x04));
                ackByteArray.append(inPtr[1]);
                ackByteArray.append(inPtr[0]);

                // SEND THE PACKET AND MAKE SURE IT GETS SENT
                if (sockInfo->socket.writeDatagram(ackByteArray, hostAddress, _serverPort) != ackByteArray.length()) {
                    sockInfo->lastError = QString("Cannot send ack packet to host : %1").arg(sockInfo->socket.errorString());
                    qCritical() << sockInfo->lastError;
                    return false;
                }

                // NOW THAT WE'VE SENT AN ACK SIGNAL, INCREMENT SENT MESSAGE COUNTER
                outgoingPacketNumber++;
            }
        } else {
            sockInfo->lastError = QString("No message received from host : %1").arg(sockInfo->socket.errorString());
            qCritical() << sockInfo->lastError;
            return false;
        }
    }

    //must create folder only once, after the file is downloaded
    //make sure that the destination folder exists
    const QString filePath(_workingFolder + "/" + serverAddress);
    QDir().mkpath(filePath);

    //open file for writing
    QFile ofile(filePath + "/" + filename);
    if (ofile.exists()) {
        sockInfo->lastError = tr("File ") + ofile.fileName() + tr(" will be overwritten");
        qWarning() << sockInfo->lastError;
    }
    if (!ofile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QDir().rmdir(filePath);
        sockInfo->lastError = tr("Cannot open file for writing ") + filename;
        qCritical() << sockInfo->lastError;
        return false;
    }
    qint64 len = ofile.write(requestedFile);
    if (len != requestedFile.size()) {
        ofile.remove();
        qCritical() << "Cannot write received content to file" << len << requestedFile.size();
        return false;
    }
    const QString msg = tr("Downloaded ") + ofile.fileName();
    qInfo() << msg;
    emit info(msg);

    QMutexLocker locker(&_statsMutex);
    _stats[serverAddress] = ofile.fileName();
    updateInfo();

    return true;
}

bool TftpClient::bindSocket(int i)
{
    if ((0 > i) || (_numWorkers <= i)) {
        qCritical() << "Invalid index";
        return false;
    }

    SocketInfo *sockInfo = _socketInfo.get() + i;
    bool rc = sockInfo->socket.bind();
    if (!rc) {
        qCritical() << "Cannot bind socket" << i << ":"
                    << sockInfo->socket.errorString();
    }
    return rc;
}

QByteArray TftpClient::getFilePacket(const QString &filename)
{
    QByteArray byteArray(filename.toLatin1());
    byteArray.prepend(static_cast<char>(0x01)); // OPCODE
    byteArray.prepend(static_cast<char>(0x00));
    byteArray.append(static_cast<char>(0x00));
    byteArray.append(QString("octet").toLatin1()); // MODE
    byteArray.append(static_cast<char>(0x00));

    return(byteArray);
}

QByteArray TftpClient::putFilePacket(const QString &filename)
{
    QByteArray byteArray;
    byteArray.append(static_cast<char>(0x00));
    byteArray.append(static_cast<char>(0x02)); // OPCODE
    byteArray.append(filename.toLatin1());
    byteArray.append(static_cast<char>(0x00));
    byteArray.append(QString("octet").toLatin1()); // MODE
    byteArray.append(static_cast<char>(0x00));

    return(byteArray);
}

bool TftpClient::parseAddressList()
{
    setAddrCount(0);
    QFile ifile(_hosts);
    if (!ifile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString msg = tr("Cannot open ") + ifile.fileName();
        qCritical() << msg;
        emit error(tr("Error"), msg);
        return false;
    }
    QHostAddress hostAddr;
    QTextStream in(&ifile);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        hostAddr.setAddress(line);
        if (QAbstractSocket::IPv4Protocol == hostAddr.protocol()) {
            _singleAddresses.append(line);
            ++_addrCount;
        } else {
            const auto tok = line.split('-');
            if (2 == tok.size()) {
                //range of IP addresses
                hostAddr.setAddress(tok.at(0).trimmed());
                QHostAddress hostAddrLast(tok.at(1).trimmed());
                if ((QAbstractSocket::IPv4Protocol == hostAddr.protocol()) &&
                        (QAbstractSocket::IPv4Protocol == hostAddrLast.protocol())) {
                    const quint32 first = hostAddr.toIPv4Address();
                    const quint32 last = hostAddrLast.toIPv4Address();
                    if (first <= last) {
                        _pairAddresses.append(QPair<quint32, quint32>(first, last));
                        _addrCount += (last - first + 1);
                    }
                }
            }
        }
    }
    emit addrCountChanged();
    return true;
}

bool TftpClient::parseFileList()
{
    _filesList.clear();
    emit fileCountChanged();
    if (!QFile::exists(_files)) {
        //assume that this is the filename to be downloaded
        _filesList.append(_files);
        emit fileCountChanged();
        return true;
    }
    //got list of files
    QFile ifile(_files);
    if (!ifile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString msg = tr("Cannot open ") + ifile.fileName();
        qCritical() << msg;
        emit error(tr("Error"), msg);
        return false;
    }
    QTextStream in(&ifile);
    while (!in.atEnd()) {
        _filesList.append(in.readLine().trimmed());
    }
    emit fileCountChanged();
    return true;
}

void TftpClient::downloadFileList(const QString &address)
{
    setCurrentAddress(address);
    setFileIndex(0);
    for (const auto &file: _filesList) {
        setFileIndex(_fileIndex + 1);
        auto f = _threadPool.push([this, address, file](int i) {
            get(i, address, file);
        });

        while (0 == _threadPool.n_idle()) {
            //sleep until some threads become available
            std::this_thread::sleep_for (std::chrono::milliseconds(100));
        }

        if (!_running) {
            qWarning() << "Stopped by user";
            break;
        }
    }

    setAddrIndex(_addrIndex + 1);
}

void TftpClient::dumpStats()
{
    if (_stats.isEmpty()) {
        emit info(tr("No files have been downloaded"));
        return;
    }
    QFile ofile(_workingFolder + "/stats.txt");
    if (!ofile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString msg = tr("Cannot open file for writing ") + ofile.fileName();
        qCritical() << msg;
        emit error(tr("Error"), msg);
        return;
    }
    QTextStream stream(&ofile);
    for (const auto &addr: _stats.keys()) {
        stream << addr << ": " << _stats[addr] << endl;
    }
    updateInfo();
}

void TftpClient::updateInfo()
{
    QString msg;
    if (1 < _stats.size()) {
        msg = QString::number(_stats.size()) + tr(" files have been downloaded");
    } else if (1 == _stats.size()) {
        msg = tr("1 file has been downloaded");
    } else {
        msg = tr("No files have been downloaded");
    }
    emit info(msg);
}
