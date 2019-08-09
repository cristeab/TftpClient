#include "tftpclient.h"
#include <QFile>
#include <QStandardPaths>

TftpClient::TftpClient(QObject *parent) : QObject(parent)
{
    setWorkingFolder(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    setObjectName("client");
}

void TftpClient::startDownload(const QString &hosts, const QString &files)
{
    qDebug() << "Start download" << hosts << files;
    _serverAddress = hosts;
    get(files);
}

void TftpClient::stopDownload()
{

}

bool TftpClient::put(const QString &filename)
{
    if (_serverAddress.isEmpty()) {
        qCritical() << "Server address cannot be empty";
        return false;
    }

    // try to read file content
    QFile ofile(filename);
    if (!ofile.open(QIODevice::ReadOnly)) {
        qCritical() << "Cannot open file for reading" << filename;
        return false;
    }
    const QByteArray transmittingFile = ofile.readAll();
    if (transmittingFile.isEmpty()) {
        qCritical() << "Input file is empty";
        return false;
    }

    // BIND OUR LOCAL SOCKET TO AN IP ADDRESS AND PORT
    if (!bindSocket()) {
        const QString msg = _socket->errorString();
        qCritical() << msg;
        emit error(msg);
        return false;
    }

    // CREATE REQUEST PACKET AND SEND TO HOST
    // WAIT UNTIL MESSAGE HAS BEEN SENT, QUIT IF TIMEOUT IS REACHED
    const QHostAddress hostAddress(_serverAddress);
    const QByteArray reqPacket = putFilePacket(filename);
    if (_socket->writeDatagram(reqPacket, hostAddress, _serverPort) != reqPacket.length()){
        const QString msg = "Cannot send packet to host " + _socket->errorString();
        qCritical() << msg;
        emit error(msg);
        return false;
    }

    // CREATE PACKET COUNTERS TO KEEP TRACK OF MESSAGES
    unsigned short incomingPacketNumber = 0;
    unsigned short outgoingPacketNumber = 0;
    char *const outPtr = reinterpret_cast<char*>(&outgoingPacketNumber);

    // NOW WAIT HERE FOR INCOMING DATA
    bool messageCompleteFlag = false;
    while (true) {
        // WAIT FOR AN INCOMING PACKET
        if (_socket->hasPendingDatagrams() || _socket->waitForReadyRead(READ_DELAY_MS)){
            // ITERATE HERE AS LONG AS THERE IS ATLEAST A
            // PACKET HEADER'S WORTH OF DATA TO READ
            QByteArray incomingDatagram;
            incomingDatagram.resize(static_cast<int>(_socket->pendingDatagramSize()));
            _socket->readDatagram(incomingDatagram.data(), incomingDatagram.length());

            // MAKE SURE FIRST BYTE IS 0
            char *buffer = incomingDatagram.data();
            char zeroByte = buffer[0];
            if (zeroByte != 0x00) {
                const QString msg = QString("Incoming packet has invalid first byte (%1).").arg(static_cast<int>(zeroByte));
                qCritical() << msg;
                emit error(msg);
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
            if (incomingMessageCounter == incomingPacketNumber) {
                incomingPacketNumber++;
            } else {
                const QString msg = QString("Error on incoming packet number %1 vs expected %2").arg(incomingMessageCounter).arg(incomingPacketNumber);
                qCritical() << msg;
                emit error(msg);
                return false;
            }

            // CHECK THE OPCODE FOR ANY ERROR CONDITIONS
            char opCode = buffer[1];
            if (opCode != 0x04) { /* ack packet should have code 4 and should be ack+1 the packet we just sent */
                const QString msg = QString("Incoming packet returned invalid operation code (%1).").arg(static_cast<int>(opCode));
                qCritical() << msg;
                emit error(msg);
                return false;
            } else {
                // SEE IF WE NEED TO SEND ANYMORE DATA PACKETS BY CHECKING END OF MESSAGE FLAG
                if (messageCompleteFlag) break;

                // SEND NEXT DATA PACKET TO HOST
                QByteArray transmitByteArray;
                transmitByteArray.append(static_cast<char>(0x00));
                transmitByteArray.append(static_cast<char>(0x03)); // send data opcode
                transmitByteArray.append(outPtr[1]);
                transmitByteArray.append(outPtr[0]);

                // APPEND DATA THAT WE WANT TO SEND
                int numBytesAlreadySent = outgoingPacketNumber*MAX_PACKET_SIZE;
                int bytesLeftToSend = transmittingFile.length()-numBytesAlreadySent;
                if (bytesLeftToSend < MAX_PACKET_SIZE) {
                    messageCompleteFlag = true;
                    if (bytesLeftToSend > 0){
                        transmitByteArray.append((transmittingFile.data() + numBytesAlreadySent), bytesLeftToSend);
                    }
                } else {
                    transmitByteArray.append((transmittingFile.data() + numBytesAlreadySent), MAX_PACKET_SIZE);
                }

                // SEND THE PACKET AND MAKE SURE IT GETS SENT
                if (_socket->writeDatagram(transmitByteArray, hostAddress, _serverPort) != transmitByteArray.length()){
                    const QString msg = QString("Cannot send data packet to host :( %1").arg(_socket->errorString());
                    qCritical() << msg;
                    emit error(msg);
                    return false;
                }

                // NOW THAT WE'VE SENT AN ACK SIGNAL, INCREMENT SENT MESSAGE COUNTER
                outgoingPacketNumber++;
            }
        } else {
            const QString msg = QString("No message received from host :( %1").arg(_socket->errorString());
            qCritical() << msg;
            emit error(msg);
            return false;
        }
    }
    qInfo() << "Uploaded" << ofile.fileName();
    return true;
}

bool TftpClient::get(const QString &filename)
{
    if (_serverAddress.isEmpty()) {
        qCritical() << "Server address cannot be empty";
        return false;
    }

    //open file for writing
    QFile ifile(_workingFolder + "/" + filename);
    if (!ifile.open(QIODevice::WriteOnly)) {
        qCritical() << "Cannot open file for writing" << filename;
        return false;
    }

    // BIND OUR LOCAL SOCKET TO AN IP ADDRESS AND PORT
    if (!bindSocket()) {
        const QString msg = _socket->errorString();
        qCritical() << msg;
        emit error(msg);
        return false;
    }

    // MAKE A LOCAL COPY OF THE REMOTE HOST ADDRESS AND PORT NUMBER
    const QHostAddress hostAddress(_serverAddress);

    // CLEAN OUT ANY INCOMING PACKETS
    while (_socket->hasPendingDatagrams()){
        QByteArray byteArray;
        byteArray.resize(static_cast<int>(_socket->pendingDatagramSize()));
        _socket->readDatagram(byteArray.data(), byteArray.length());
    }

    // CREATE REQUEST PACKET AND SEND TO HOST
    // WAIT UNTIL MESSAGE HAS BEEN SENT, QUIT IF TIMEOUT IS REACHED
    QByteArray reqPacket=getFilePacket(filename);
    if (_socket->writeDatagram(reqPacket, hostAddress, _serverPort) != reqPacket.length()) {
        const QString msg =  QString("Cannot send packet to host :( %1").arg(_socket->errorString());
        qCritical() << msg;
        emit error(msg);
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
        if (_socket->hasPendingDatagrams() || _socket->waitForReadyRead(READ_DELAY_MS)) {
            // ITERATE HERE AS LONG AS THERE IS ATLEAST A
            // PACKET HEADER'S WORTH OF DATA TO READ
            QByteArray incomingDatagram;
            incomingDatagram.resize(static_cast<int>(_socket->pendingDatagramSize()));
            _socket->readDatagram(incomingDatagram.data(), incomingDatagram.length());

            // MAKE SURE FIRST BYTE IS 0
            char *buffer=incomingDatagram.data();
            char zeroByte=buffer[0];
            if (zeroByte != 0x00) {
                const QString msg = QString("Incoming packet has invalid first byte (%1).").arg(static_cast<int>(zeroByte));
                qCritical() << msg;
                emit error(msg);
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
                const QString msg = QString("Error on incoming packet number %1 vs expected %2").arg(incomingMessageCounter).arg(incomingPacketNumber);
                qCritical() << msg;
                emit error(msg);
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
                const QString msg = QString("Incoming packet returned invalid operation code (%1).").arg(static_cast<int>(opCode));
                qCritical() << msg;
                emit error(msg);
                return false;
            } else {
                // SEND PACKET ACKNOWLEDGEMENT BACK TO HOST REFLECTING THE INCOMING PACKET NUMBER
                QByteArray ackByteArray;
                ackByteArray.append(static_cast<char>(0x00));
                ackByteArray.append(static_cast<char>(0x04));
                ackByteArray.append(inPtr[1]);
                ackByteArray.append(inPtr[0]);

                // SEND THE PACKET AND MAKE SURE IT GETS SENT
                if (_socket->writeDatagram(ackByteArray, hostAddress, _serverPort) != ackByteArray.length()) {
                    const QString msg = QString("Cannot send ack packet to host :( %1").arg(_socket->errorString());
                    qCritical() << msg;
                    emit error(msg);
                    return false;
                }

                // NOW THAT WE'VE SENT AN ACK SIGNAL, INCREMENT SENT MESSAGE COUNTER
                outgoingPacketNumber++;
            }
        } else {
            const QString msg = QString("No message received from host :( %1").arg(_socket->errorString());
            qCritical() << msg;
            emit error(msg);
            return false;
        }
    }
    qint64 len = ifile.write(requestedFile);
    if (len != requestedFile.size()) {
        qCritical() << "Cannot write received content to file" << len << requestedFile.size();
        return false;
    }
    qInfo() << "Downloaded" << ifile.fileName();
    return true;
}

bool TftpClient::bindSocket()
{
    _socket.reset(new QUdpSocket());
    return _socket->bind();
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
