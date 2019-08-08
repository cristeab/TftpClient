#include "tftpclient.h"
#include <QUdpSocket>

TftpClient::TftpClient(QObject *parent) : QObject(parent)
{
    setObjectName("client");
}

bool TftpClient::put(const QString &filename)
{

}

bool TftpClient::get(const QString &filename)
{

}

bool TftpClient::bindSocket()
{

}

QByteArray TftpClient::getFilePacket(const QString &filename)
{

}

QByteArray TftpClient::putFilePacket(const QString &filename)
{

}
