#include <QLabel>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include "TransferProtocol.h"

TransferProtocol::TransferProtocol(QObject *parent) :
    QObject(parent),
    m_serial(new SerialPort)
{
    connect(this, &TransferProtocol::writeData, m_serial, &SerialPort::write_data);

}

TransferProtocol::~TransferProtocol()
{

}

void TransferProtocol::calculateCheckSum(sMSG_PACKET_FORMAT *psPacket)
{
    WORD wChecksum = 0;

    wChecksum = 0;
    wChecksum += psPacket->sPacketHeader.cSource;
    wChecksum += psPacket->sPacketHeader.cDestination;
    wChecksum += psPacket->sPacketHeader.cPacketType;
    wChecksum += (psPacket->sPacketHeader.wSeqId & 0x00FF);
    wChecksum += ((psPacket->sPacketHeader.wSeqId >> 8) & 0x00FF);
    wChecksum += (psPacket->sPacketHeader.wPacketSize & 0x00FF);
    wChecksum += ((psPacket->sPacketHeader.wPacketSize >> 8) & 0x00FF);

    for(qint32 i = 0; i < psPacket->sPacketHeader.wPacketSize; i++)
    {
        wChecksum += psPacket->uFormat.acPayload[i];
    }

    psPacket->sPacketHeader.wChecksum = ((~wChecksum + 1) & 0xFFFF);
}

void TransferProtocol::packetBuild(sMSG_PACKET_FORMAT *psPacket, eDEVICE eSource, eDEVICE eDestination, ePAYLOAD_TYPE ePayloadType, WORD wSeqId, WORD wDataSize, sPAYLOAD *psPayload)
{
    psPacket->cMagicNumber1 = MAGICNUMBER1;
    psPacket->cMagicNumber2 = MAGICNUMBER2;
    psPacket->sPacketHeader.cSource = eSource;
    psPacket->sPacketHeader.cDestination = eDestination;
    psPacket->sPacketHeader.cPacketType = ePayloadType;
    psPacket->sPacketHeader.wSeqId = wSeqId;
    psPacket->sPacketHeader.wPacketSize = wDataSize;
    psPacket->uFormat.sPayload = *psPayload;
    calculateCheckSum(psPacket);
}

void TransferProtocol::packageAck(sMSG_PACKET_FORMAT *psPacket, eDEVICE eSource, eDEVICE eDestination, eACK_TYPE eAckType)
{
    psPacket->cMagicNumber1 = MAGICNUMBER1;
    psPacket->cMagicNumber2 = MAGICNUMBER2;
    psPacket->sPacketHeader.cSource = eSource;
    psPacket->sPacketHeader.cDestination = eDestination;
    psPacket->sPacketHeader.cPacketType = ePAYLOAD_TYPE_ACK;
    psPacket->sPacketHeader.wPacketSize = 1;
    psPacket->uFormat.acPayload[0] = eAckType;
    calculateCheckSum(psPacket);

    packageSend(psPacket);
}

void TransferProtocol::packageSend(sMSG_PACKET_FORMAT *psPacket)
{
    QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char*>(psPacket), (psPacket->sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));

    writeData(PackageOut);
}

bool TransferProtocol::commandWrite(eIAP_CMD eCmd, WORD wSize, BYTE *pcData)
{
    bool ret = false;
    quint8 retry = 5;
    sPAYLOAD sPayload;
    sMSG_PACKET_FORMAT sPackage;
    sMSG_PACKET_FORMAT *psRetMsg;

    sPayload.cModule = 0;
    sPayload.cSubCmd = eCmd;

    if(0 != wSize)
    {
        memcpy(sPayload.acData, pcData, wSize);
    }

    packetBuild(&sPackage, eDEVICE_APP, eDEVICE_BOOTCODE, ePAYLOAD_TYPE_EXE_WRITE, wSeqId++, (wSize+2), &sPayload);

    m_serial->m_port->clear();
    QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char *>(&sPackage), (sPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));
    writeData(PackageOut);

    //Blocking read
    if(m_serial->m_port->waitForReadyRead(5000))
    {
        QByteArray responseData = m_serial->m_port->readAll();

        while(retry--)
        {
            if(m_serial->m_port->waitForReadyRead(10))
            {
                responseData.append(m_serial->m_port->readAll());
            }
        }

        emit recived_data(responseData);

        psRetMsg = reinterpret_cast<sMSG_PACKET_FORMAT *>(responseData.data());

        if((ePAYLOAD_TYPE_ACK == psRetMsg->sPacketHeader.cPacketType)&&(eACK_TYPE_ACK == psRetMsg->uFormat.acPayload[0]))
        {
            ret = true;
        }
    }

    return ret;
}

bool TransferProtocol::commandRead(eIAP_CMD eCmd, QByteArray &data)
{
    bool ret = false;
    quint8 retry = 5;
    sPAYLOAD sPayload;
    sMSG_PACKET_FORMAT sPackage;
    sMSG_PACKET_FORMAT *psRetMsg;

    sPayload.cModule = 0;
    sPayload.cSubCmd = eCmd;

    packetBuild(&sPackage, eDEVICE_APP, eDEVICE_BOOTCODE, ePAYLOAD_TYPE_EXE_READ, wSeqId++, 2, &sPayload);

    m_serial->m_port->clear();
    QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char *>(&sPackage), (sPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));
    writeData(PackageOut);

    //Blocking read
    if(m_serial->m_port->waitForReadyRead(5000))
    {
        data = m_serial->m_port->readAll();

        while(retry--)
        {
            if(m_serial->m_port->waitForReadyRead(10))
            {
                data.append(m_serial->m_port->readAll());
            }
        }

        emit recived_data(data);

        psRetMsg = reinterpret_cast<sMSG_PACKET_FORMAT *>(data.data());

        if(ePAYLOAD_TYPE_REPLY == psRetMsg->sPacketHeader.cPacketType)
        {
            data.remove(0, HEADERSIZE + MAGICNUMBERSIZE);
            ret = true;
        }
    }

    return ret;
}

#if 0
void TransferProtocol::resetState()
{
    memset(&m_sMsgState, 0x00, sizeof (sMSG_STATE_DATA));
}

void TransferProtocol::receive_data(const QByteArray &data)
{
    //qDebug() << "TransferProtocol receive_data is:" << QThread::currentThreadId();
    for (qint32 i = 0; i < data.size(); i++)
    {
        m_qRecivedQueue.enqueue(data.at(i));
    }

    parser(data.at(0));
}

void TransferProtocol::parser(char data)
{
    //while(1)
    {
        //qDebug() << "TransferProtocol parser is:" << QThread::currentThreadId();
        while (!m_qRecivedQueue.empty())
        {
            //QMutexLocker locker(&m_Mutex);
            BYTE cReadBuffer = static_cast<BYTE>(m_qRecivedQueue.dequeue());
            //BYTE cReadBuffer = static_cast<BYTE>(data);
            qDebug() << cReadBuffer;

            switch(m_sMsgState.eMsgParsingState)
            {
            case eMSG_STATE_MAGIC_NUMBER:
            {
                m_sMsgState.sMsgPacket.cMagicNumber2 = cReadBuffer;

                if((MAGICNUMBER1 == m_sMsgState.sMsgPacket.cMagicNumber1) && (MAGICNUMBER2 == m_sMsgState.sMsgPacket.cMagicNumber2))
                {
                    m_sMsgState.eMsgParsingState = eMSG_STATE_PACKET_HEADER;
                    m_sMsgState.wRecivedByteCount = 0;
                    m_sMsgState.wRecivedByteCRC = 0;
                }
                else
                {
                    m_sMsgState.sMsgPacket.cMagicNumber1 = m_sMsgState.sMsgPacket.cMagicNumber2;
                }
            }
                break;

            case eMSG_STATE_PACKET_HEADER:
            {
                BYTE *pcBuffer = reinterpret_cast<BYTE*>(&m_sMsgState.sMsgPacket.sPacketHeader);
                *(pcBuffer + m_sMsgState.wRecivedByteCount++) = cReadBuffer;

                if(HEADERSIZE == m_sMsgState.wRecivedByteCount)
                {
                    if(MAX_PACKET_SIZE >= m_sMsgState.sMsgPacket.sPacketHeader.wPacketSize)
                    {
                        m_sMsgState.wRecivedByteCount = 0;
                        m_sMsgState.eMsgParsingState = eMSG_STATE_PACKET_PAYLOAD;

                        // 計算CRC
                        m_sMsgState.wRecivedByteCRC += m_sMsgState.sMsgPacket.sPacketHeader.cSource;
                        m_sMsgState.wRecivedByteCRC += m_sMsgState.sMsgPacket.sPacketHeader.cDestination;
                        m_sMsgState.wRecivedByteCRC += m_sMsgState.sMsgPacket.sPacketHeader.cPacketType;
                        m_sMsgState.wRecivedByteCRC += (m_sMsgState.sMsgPacket.sPacketHeader.wSeqId & 0xFF00) >> 8;
                        m_sMsgState.wRecivedByteCRC += (m_sMsgState.sMsgPacket.sPacketHeader.wSeqId & 0x00FF);
                        m_sMsgState.wRecivedByteCRC += (m_sMsgState.sMsgPacket.sPacketHeader.wPacketSize & 0xFF00) >> 8;
                        m_sMsgState.wRecivedByteCRC += (m_sMsgState.sMsgPacket.sPacketHeader.wPacketSize & 0x00FF);

                        // 不帶Payload資料,檢查CRC
                        if(0 == m_sMsgState.sMsgPacket.sPacketHeader.wPacketSize)
                        {
                            if(0 == (0xFFFF & (m_sMsgState.sMsgPacket.sPacketHeader.wChecksum + m_sMsgState.wRecivedByteCRC)))
                            {
                                m_sMsgState.eMsgParsingState = eMSG_STATE_DATA_READY;
                            }
                            else
                            {
                                m_sMsgState.eMsgParsingState = eMSG_STATE_BAD_PACKET;
                            }

                            goto end;
                        }
                    }
                    else
                    {
                        m_sMsgState.eMsgParsingState = eMSG_STATE_BAD_PACKET;

                        goto end;
                    }
                }
            }
                break;

            case eMSG_STATE_PACKET_PAYLOAD:
            {
                m_sMsgState.wRecivedByteCRC += cReadBuffer;
                m_sMsgState.sMsgPacket.uFormat.acPayload[m_sMsgState.wRecivedByteCount++] = cReadBuffer;

                if(m_sMsgState.sMsgPacket.sPacketHeader.wPacketSize == m_sMsgState.wRecivedByteCount)
                {
                    if(0 == (0xFFFF & (m_sMsgState.sMsgPacket.sPacketHeader.wChecksum + m_sMsgState.wRecivedByteCRC)))
                    {
                        m_sMsgState.eMsgParsingState = eMSG_STATE_DATA_READY;
                    }
                    else
                    {
                        m_sMsgState.eMsgParsingState = eMSG_STATE_BAD_PACKET;
                    }

                    goto end;
                }
            }
                break;

            default:
                goto end;
            }
        }

end:
        sMSG_PACKET_FORMAT retPackage = m_sMsgState.sMsgPacket;

        switch(m_sMsgState.eMsgParsingState)
        {
        case eMSG_STATE_MAGIC_NUMBER:
        case eMSG_STATE_PACKET_HEADER:
        case eMSG_STATE_PACKET_PAYLOAD:
            // Packet unpacking
            break;

        case eMSG_STATE_DATA_READY:
        {
            m_qMegQueue.enqueue(m_sMsgState.sMsgPacket);
            emit ready();

            QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char*>(&retPackage), (retPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));
            qDebug()    << PackageOut.toHex();
            qDebug()    << "Source ="       << retPackage.sPacketHeader.cSource
                        << "Destination ="  << retPackage.sPacketHeader.cDestination
                        << "Typde ="        << retPackage.sPacketHeader.cPacketType
                        << "SeqId ="        << retPackage.sPacketHeader.wSeqId
                        << "Size ="         << retPackage.sPacketHeader.wPacketSize
                        << "Module ="       << retPackage.uFormat.sPayload.cModule
                        << "SubCmd ="       << retPackage.uFormat.sPayload.cSubCmd
                        << "Data ="         << retPackage.uFormat.sPayload.acData[0];

            if(ePAYLOAD_TYPE_EXE_WRITE == retPackage.sPacketHeader.cPacketType)
                packageAck(&retPackage, static_cast<eDEVICE>(retPackage.sPacketHeader.cDestination), static_cast<eDEVICE>(retPackage.sPacketHeader.cSource), eACK_TYPE_ACK);

            // Restart parsing process
            resetState();
        }
            break;

        case eMSG_STATE_BAD_PACKET:
            // Build Ack and push in output ring buffer
            qDebug() << "BAD_PACKET";
            packageAck(&retPackage, static_cast<eDEVICE>(m_sMsgState.sMsgPacket.sPacketHeader.cDestination), static_cast<eDEVICE>(retPackage.sPacketHeader.cSource), eACK_TYPE_BADPACKET);
            resetState();
            break;

        case eMSG_STATE_TIMEOUT:
            // Build Ack and push in output ring buffer
            qDebug() << "TIMEOUT";
            packageAck(&retPackage, static_cast<eDEVICE>(retPackage.sPacketHeader.cDestination), static_cast<eDEVICE>(retPackage.sPacketHeader.cSource), eACK_TYPE_TIMEOUT);
            resetState();
            break;

        case eMSG_STATE_RUN_OUT_OF_MEMORY:
            // Build Ack and push in output ring buffer
            qDebug() << "RUN_OUT_OF_MEMORY";
            packageAck(&retPackage, static_cast<eDEVICE>(retPackage.sPacketHeader.cDestination), static_cast<eDEVICE>(retPackage.sPacketHeader.cSource), eACK_TYPE_BADPACKET);
            resetState();
            break;

        default:
            break;
        }

        //m_parserThread->msleep(1);
    }
}

bool TransferProtocol::returnPayload(sMSG_PACKET_FORMAT *psMsg)
{
    bool ret = false;

    if(0 != m_qMegQueue.size())
    {
        *psMsg = m_qMegQueue.dequeue();
        ret = true;
    }

    return ret;
}

bool TransferProtocol::commandWrite(quint8 cCmd, quint16 wSize, quint8 *pcData)
{
    qDebug() << "TransferProtocol commandWrite is:" << QThread::currentThreadId();
    bool ret = false;
    sMSG_PACKET_FORMAT sPackage;
    sPAYLOAD sPayload;
    sMSG_PACKET_FORMAT sRetMsg;

    sPayload.cModule = 0;
    sPayload.cSubCmd = cCmd;

    if(0 != wSize)
        memcpy(sPayload.acData, pcData, wSize);

    packetBuild(&sPackage, eDEVICE_APP, eDEVICE_BOOTCODE, ePAYLOAD_TYPE_EXE_WRITE, wSeqId++, (wSize+2), &sPayload);

    QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char *>(&sPackage), (sPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));
    writeData(PackageOut);

    waitReady();

    qDebug() << "ready";

    if(true == returnPayload(&sRetMsg))
    {
        if((ePAYLOAD_TYPE_ACK == sRetMsg.sPacketHeader.cPacketType)&&(eACK_TYPE_ACK == sRetMsg.uFormat.acPayload[0]))
        {
            qDebug() << "Ack";
            ret = true;
        }
    }

    return ret;
}

bool TransferProtocol::commandRead(quint8 cCmd, quint16 wSize, quint8 *pcData)
{
    bool ret = false;
    sMSG_PACKET_FORMAT sPackage;
    sPAYLOAD sPayload;
    //sMSG_PACKET_FORMAT sRetMsg;

    sPayload.cModule = 0;
    sPayload.cSubCmd = cCmd;

    if(0 != wSize)
        memcpy(sPayload.acData, pcData, wSize);

    packetBuild(&sPackage, eDEVICE_APP, eDEVICE_BOOTCODE, ePAYLOAD_TYPE_EXE_READ, wSeqId++, (wSize+2), &sPayload);

    QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char *>(&sPackage), (sPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));
    writeData(PackageOut);

#if 0
    while(true == m_transferProtocol->returnPayload(&sRetMsg))
    {
        if(TransferProtocol::ePAYLOAD_TYPE::ePAYLOAD_TYPE_REPLY == sRetMsg.sPacketHeader.cPacketType)
        {
            qDebug() << "Get Data";
            ret = true;
            break;
        }
    }
#endif // 0
    return ret;
}

void TransferProtocol::waitReady()
{
}
#endif //0
