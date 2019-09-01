#include "tftpclient.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QSettings>
#include <QGuiApplication>
#include <thread>

#define HOSTS "HOSTS"
#define PREFIX "PREFIX"
#define FILES "FILES"
#define EXT "EXT"
#define WORKING_FOLDER "WORKING_FOLDER"
#define SERVER_PORT "SERVER_PORT"
#define READ_DELAY_MS "READ_DELAY_MS"
#define NUM_WORKERS "NUM_WORKERS"

TftpClient::TftpClient(QObject *parent) : QObject(parent)
{
    setWorkingFolder(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    setObjectName("client");
    setRunning(false);

    loadSettings();

    _socketInfo.reset(new SocketInfo[_numWorkers]);
    parseAddressList();
}

void TftpClient::startDownload()
{
    _stats.clear();
    updateInfo();
    setRunning(true);

    std::thread th([this]() {
        if (1 < _numWorkers) {
            _threadPool.init();
            _threadPool.resize(_numWorkers);
        }
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
    auto &socket = sockInfo->socket;

    if (socket.isNull()) {
        //sockets must be created in the calling thread
        socket.reset(new QUdpSocket());
        bool rc = socket->bind();
        if (!rc) {
            qCritical() << "Cannot bind socket" << i << ":"
                        << socket->errorString();
            return false;
        }
    }

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
    while (socket->hasPendingDatagrams()){
        QByteArray byteArray;
        byteArray.resize(static_cast<int>(socket->pendingDatagramSize()));
        socket->readDatagram(byteArray.data(), byteArray.length());
    }

    // CREATE REQUEST PACKET AND SEND TO HOST
    // WAIT UNTIL MESSAGE HAS BEEN SENT, QUIT IF TIMEOUT IS REACHED
    QByteArray reqPacket=getFilePacket(filename);
    if (socket->writeDatagram(reqPacket, hostAddress,
                                       static_cast<quint16>(_serverPort)) !=
            reqPacket.length()) {
        sockInfo->lastError = QString("Cannot send packet to host : %1").arg(socket->errorString());
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
        if (socket->hasPendingDatagrams() || socket->waitForReadyRead(_readDelayMs)) {
            // ITERATE HERE AS LONG AS THERE IS ATLEAST A
            // PACKET HEADER'S WORTH OF DATA TO READ
            QByteArray incomingDatagram;
            incomingDatagram.resize(static_cast<int>(socket->pendingDatagramSize()));
            socket->readDatagram(incomingDatagram.data(), incomingDatagram.length());

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
                if (socket->writeDatagram(ackByteArray, hostAddress,
                                                   static_cast<quint16>(_serverPort)) != ackByteArray.length()) {
                    sockInfo->lastError = QString("Cannot send ack packet to host : %1").arg(socket->errorString());
                    qCritical() << sockInfo->lastError;
                    return false;
                }

                // NOW THAT WE'VE SENT AN ACK SIGNAL, INCREMENT SENT MESSAGE COUNTER
                outgoingPacketNumber++;
            }
        } else {
            //sockInfo->lastError = QString("No message received from host : %1").arg(socket.errorString());
            //qCritical() << sockInfo->lastError;
            return false;
        }
    }

    //only one thread accesses the harddisk
    QMutexLocker locker(&_statsMutex);

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

    _stats[serverAddress] = ofile.fileName();
    updateInfo();

    return true;
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
    _singleAddresses.clear();
    _pairAddresses.clear();
    setAddrCount(0);
    if (_hosts.isEmpty()) {
        qWarning() << "Hosts file is empty";
        return false;
    }

    //one address provided
    QHostAddress hostAddr(_hosts.trimmed());
    if (QAbstractSocket::IPv4Protocol == hostAddr.protocol()) {
        _singleAddresses.append(_hosts.trimmed());
        return true;
    }

    //addresses provided in a file
    QFile ifile(_hosts);
    if (!ifile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString msg = tr("Cannot open ") + ifile.fileName();
        qCritical() << msg;
        emit error(tr("Error"), msg);
        return false;
    }
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
    qDebug() << "Single addresses" << _singleAddresses.size();
    qDebug() << "Address ranges" << _pairAddresses.size();
    emit addrCountChanged();
    return true;
}

void TftpClient::downloadFileList(const QString &address)
{
    setCurrentAddress(address);
    setCurrentFilename("");
    _found = false;

    if (!QFile::exists(_files)) {
        //assume that this is the filename to be downloaded
        const QString file = generateFilename(_files);
        get(0, address, file);
        return;
    }

    //got list of files
    QFile ifile(_files);
    if (!ifile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString msg = tr("Cannot open ") + ifile.fileName();
        qCritical() << msg;
        emit error(tr("Error"), msg);
        return;
    }
    QTextStream in(&ifile);
    while (!in.atEnd()) {
        const QString file = generateFilename(in.readLine().trimmed());
        setCurrentFilename(file);
        if (1 < _numWorkers) {
            auto f = _threadPool.push([this, address, file](int i) {
                _found = get(i, address, file);
            });

            while (0 == _threadPool.n_idle() && !_found) {
                //sleep until some threads become available
                std::this_thread::sleep_for (std::chrono::milliseconds(100));
            }
        } else {
            _found = get(0, address, file);
        }

        if (_found) {
            break;
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

void TftpClient::loadSettings()
{
    QSettings settings(qApp->organizationName(), qApp->applicationName());

    setHosts(settings.value(HOSTS).toString());
    setPrefix(settings.value(PREFIX).toString());
    setFiles(settings.value(FILES).toString());
    setExtension(settings.value(EXT).toString());
    setWorkingFolder(settings.value(WORKING_FOLDER, _workingFolder).toString());

    setServerPort(settings.value(SERVER_PORT, DEFAULT_PORT).toInt());
    setReadDelayMs(settings.value(READ_DELAY_MS, DEFAULT_READ_DELAY_MS).toInt());

    //default value
    _numWorkers = static_cast<int>(std::thread::hardware_concurrency());
    if (DEFAULT_NUM_WORKERS > _numWorkers) {
        _numWorkers = DEFAULT_NUM_WORKERS;
    }
    //then value from settings if any
    setNumWorkers(settings.value(NUM_WORKERS, _numWorkers).toInt());
}

void TftpClient::saveSettings()
{
    QSettings settings(qApp->organizationName(), qApp->applicationName());

    settings.setValue(HOSTS, _hosts);
    settings.setValue(PREFIX, _prefix);
    settings.setValue(FILES, _files);
    settings.setValue(EXT, _extension);
    settings.setValue(WORKING_FOLDER, _workingFolder);
    settings.setValue(SERVER_PORT, _serverPort);
    settings.setValue(READ_DELAY_MS, _readDelayMs);
    settings.setValue(NUM_WORKERS, _numWorkers);
}

QString TftpClient::generateFilename(const QString &suffix)
{
    QString fn = _prefix + suffix;
    if (!_extension.isEmpty()) {
        fn += "." + _extension;
    }
    return fn;
}
