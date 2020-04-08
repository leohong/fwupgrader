#include <QLabel>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include "TransferProtocol.h"

TransferProtocol::TransferProtocol(QObject *parent) :
    QObject(parent),
    m_serial(new SerialPort),
    m_wSeqId(0)
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
        wChecksum += psPacket->uFormat.acPacketPayload[i];
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
    psPacket->uFormat.acPacketPayload[0] = eAckType;
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
    qDebug() << "\ncommandWrite";

    do
    {
        sPayload.cModule = 0;
        sPayload.cSubCmd = eCmd;

        if(0 != wSize) {
            memcpy(sPayload.acData, pcData, wSize);
        }

        packetBuild(&sPackage, eDEVICE_APP, eDEVICE_BOOTCODE, ePAYLOAD_TYPE_EXE_WRITE, m_wSeqId, (wSize+2), &sPayload);

        m_serial->m_port->clear();
        QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char *>(&sPackage), (sPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));

        writeData(PackageOut);
        resetState(&m_sMsgState);
        while(eMSG_STATE_DATA_READY > m_sMsgState.eMsgParsingState) {
            utilHost_StateProcess(&m_sMsgState, 5000);

            if(eMSG_STATE_DATA_READY == m_sMsgState.eMsgParsingState) {
                if(m_wSeqId == m_sMsgState.sMsgPacket.sPacketHeader.wSeqId) {
                    if((ePAYLOAD_TYPE_ACK == m_sMsgState.sMsgPacket.sPacketHeader.cPacketType)
                       && (eACK_TYPE_ACK == m_sMsgState.sMsgPacket.uFormat.acPacketPayload[0]))  {
                        ret = true;
                    }
                } else {
                    resetState(&m_sMsgState);
                }
            }
        }

        switch(m_sMsgState.eMsgParsingState) {
        case eMSG_STATE_BAD_PACKET:
        case eMSG_STATE_TIMEOUT:
        case eMSG_STATE_RUN_OUT_OF_MEMORY:
        case eMSG_STATE_INITIAL_ERROR:
            //usleep(500 * 1000);
            ret = false;
            break;

        default:
            break;
        }

        m_wSeqId++;
    } while((retry--) && (ret == false));

    return ret;
}

bool TransferProtocol::commandRead(eIAP_CMD eCmd, QByteArray &data)
{
    bool ret = false;
    quint8 retry = 5;
    sPAYLOAD sPayload;
    sMSG_PACKET_FORMAT sPackage;
    qDebug() << "\ncommandRead";

    do
    {
        sPayload.cModule = 0;
        sPayload.cSubCmd = eCmd;

        packetBuild(&sPackage, eDEVICE_APP, eDEVICE_BOOTCODE, ePAYLOAD_TYPE_EXE_READ, m_wSeqId, 2, &sPayload);

        m_serial->m_port->clear();
        QByteArray PackageOut = QByteArray::fromRawData(reinterpret_cast<char *>(&sPackage), (sPackage.sPacketHeader.wPacketSize + HEADERSIZE + MAGICNUMBERSIZE));
        writeData(PackageOut);
        resetState(&m_sMsgState);

        while(eMSG_STATE_DATA_READY > m_sMsgState.eMsgParsingState) {
            utilHost_StateProcess(&m_sMsgState, 5000);

            if(eMSG_STATE_DATA_READY == m_sMsgState.eMsgParsingState) {
                if(m_wSeqId == m_sMsgState.sMsgPacket.sPacketHeader.wSeqId) {
                    if(ePAYLOAD_TYPE_REPLY == m_sMsgState.sMsgPacket.sPacketHeader.cPacketType) {
                        data = QByteArray::fromRawData(reinterpret_cast<char *>(m_sMsgState.sMsgPacket.uFormat.acPacketPayload), m_sMsgState.sMsgPacket.sPacketHeader.wPacketSize);
                        ret = true;
                    }
                } else {
                    resetState(&m_sMsgState);
                }
            }
        }

        switch(m_sMsgState.eMsgParsingState) {
            case eMSG_STATE_TIMEOUT:
            case eMSG_STATE_BAD_PACKET:
            case eMSG_STATE_RUN_OUT_OF_MEMORY:
            case eMSG_STATE_INITIAL_ERROR:
                resetState(&m_sMsgState);
                ret = false;
                break;

            default:
                break;
        }

        m_wSeqId++;
    } while((retry--) && (ret == false));

    return ret;
}

void TransferProtocol::resetState(sMSG_STATE_DATA *psMsgData)
{
    memset(&psMsgData->sMsgPacket, 0, sizeof(sMSG_PACKET_FORMAT) / sizeof(BYTE));
    psMsgData->eMsgParsingState = eMSG_STATE_MAGIC_NUMBER;
    psMsgData->wRecivedByteCount = 0;
    psMsgData->wRecivedByteCRC = 0;
}

TransferProtocol::eMSG_STATE TransferProtocol::utilHost_StateProcess(sMSG_STATE_DATA *psMsgData, DWORD dwMilliSecond)
{
    unsigned char cReadBuffer = 0;

    if(m_serial->m_port->waitForReadyRead(dwMilliSecond))
    {
        m_serial->m_port->read(reinterpret_cast<char *>(&cReadBuffer), 1);

        switch(psMsgData->eMsgParsingState)
        {
            case eMSG_STATE_MAGIC_NUMBER:
            {
                psMsgData->sMsgPacket.cMagicNumber2 = cReadBuffer;

                if((MAGICNUMBER1 == psMsgData->sMsgPacket.cMagicNumber1) && (MAGICNUMBER2 == psMsgData->sMsgPacket.cMagicNumber2))
                {
                    psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_PACKET_HEADER;
                    psMsgData->wRecivedByteCount = 0;
                    psMsgData->wRecivedByteCRC = 0;
                }
                else
                {
                    psMsgData->sMsgPacket.cMagicNumber1 = psMsgData->sMsgPacket.cMagicNumber2;
                }
            }
            break;

            case eMSG_STATE_PACKET_HEADER:
            {
                BYTE *pcBuffer = (BYTE *)&psMsgData->sMsgPacket.sPacketHeader;
                *(pcBuffer + psMsgData->wRecivedByteCount++) = cReadBuffer;

                if(HEADERSIZE == psMsgData->wRecivedByteCount)
                {
                    if(MAX_PACKET_SIZE >= psMsgData->sMsgPacket.sPacketHeader.wPacketSize)
                    {
                        psMsgData->wRecivedByteCount = 0;
                        psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_PACKET_PAYLOAD;

                        psMsgData->wRecivedByteCRC = 0;
                        psMsgData->wRecivedByteCRC += psMsgData->sMsgPacket.sPacketHeader.cSource;
                        psMsgData->wRecivedByteCRC += psMsgData->sMsgPacket.sPacketHeader.cDestination;
                        psMsgData->wRecivedByteCRC += psMsgData->sMsgPacket.sPacketHeader.cPacketType;
                        psMsgData->wRecivedByteCRC += ((psMsgData->sMsgPacket.sPacketHeader.wSeqId >> 8) & 0x00FF);
                        psMsgData->wRecivedByteCRC += (psMsgData->sMsgPacket.sPacketHeader.wSeqId & 0x00FF);
                        psMsgData->wRecivedByteCRC += ((psMsgData->sMsgPacket.sPacketHeader.wPacketSize >> 8) & 0x00FF) ;
                        psMsgData->wRecivedByteCRC += (psMsgData->sMsgPacket.sPacketHeader.wPacketSize & 0x00FF);

                        if(0 == psMsgData->sMsgPacket.sPacketHeader.wPacketSize)
                        {
                            if(0 == (0xFFFF & (psMsgData->sMsgPacket.sPacketHeader.wChecksum + psMsgData->wRecivedByteCRC)))
                            {
                                psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_DATA_READY;
                            }
                            else
                            {
                                qDebug() << "CRC ERROR";
                                //utilHost_Package_Print(psMsgData);
                                psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_BAD_PACKET;
                            }

                            return psMsgData->eMsgParsingState;
                        }
                    }
                    else
                    {
                        qDebug() << "OVER SIZE";
                        psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_BAD_PACKET;
                        return TransferProtocol::eMSG_STATE_BAD_PACKET;
                    }
                }
            }
            break;

            case eMSG_STATE_PACKET_PAYLOAD:
            {
                psMsgData->wRecivedByteCRC += cReadBuffer;
                psMsgData->sMsgPacket.uFormat.acPacketPayload[psMsgData->wRecivedByteCount++] = cReadBuffer;

                if(psMsgData->sMsgPacket.sPacketHeader.wPacketSize == psMsgData->wRecivedByteCount)
                {
                    if(0 == (0xFFFF & (psMsgData->sMsgPacket.sPacketHeader.wChecksum + psMsgData->wRecivedByteCRC)))
                    {
                        psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_DATA_READY;
                    }
                    else
                    {
                        qDebug() << "CRC ERROR";
                        psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_BAD_PACKET;
                    }

#if 1
                    if(ePAYLOAD_TYPE_REPLY == psMsgData->sMsgPacket.sPacketHeader.cPacketType) {
                        psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_DATA_READY;
                    }
#endif //0
                    return psMsgData->eMsgParsingState;
                }
            }
            break;

            default:
                return psMsgData->eMsgParsingState;
                //break;
        }
    }
    else
    {
        // printf("MSK_LIST_HOST, Read failed!\n");
        psMsgData->eMsgParsingState = TransferProtocol::eMSG_STATE_TIMEOUT;
    }

    return psMsgData->eMsgParsingState;
}
