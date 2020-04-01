#ifndef TRANSFERPROTOCOL_H
#define TRANSFERPROTOCOL_H

#include <QMainWindow>
#include <QObject>
#include <QFile>
#include <QQueue>
#include <QMutex>
#include <QThread>
#include <QDebug>
#include "commonTypeDef.h"
#include "serialport.h"

#define MAX_MSG_PKT     (10)
#define MSG_RETRY       (5)
#define MAX_PACKET_SIZE (sizeof(sPAYLOAD)/sizeof(BYTE))
#define eCMD_MODULE_IAP (10)

#define HEADERSIZE      (sizeof(TransferProtocol::sMSG_PACKET_HEADER)/sizeof(BYTE))
#define MAGICNUMBERSIZE (2)
#define MAGICNUMBER1    (0x55)
#define MAGICNUMBER2    (0x55)

typedef enum
{
    // Common Command
    /* 00 */eIAP_CMD_IDLE               = 0,    // Check Busy
    /* 01 */eIAP_CMD_VERSION            = 1,
    /* 02 */eIAP_CMD_GO_TO_BOOTLOADER   = 2,

    /* 03 */eIAP_CMD_BL_VER             = 3,    // Bootloader Version
    /* 04 */eIAP_CMD_APP_VER,                   // App Code Version
    /* 05 */eIAP_CMD_RUN_APP,                   // Run App Code
    /* 06 */eIAP_CMD_IAP_ENABLE,                // Enable IAP Process
    /* 07 */eIAP_CMD_APP_INFO,                  // Start Address and Size of Bin File
    /* 08 */eIAP_CMD_BIN_ADDRESS,               // Address of 1K buffer of Bin File
    /* 09 */eIAP_CMD_BIN_DATA,                  // Send Bin Data
    /* 10 */eIAP_CMD_PROGRAMING_FINISH,         // Programming Finish
    /* 11 */eIAP_CMD_BIN_CHECK_SUM,             // Bin File Check Sum
    /* 12 */eIAP_CMD_PROGRAMING,                // Program Bin Data to Flash
    /* 13 */eIAP_CMD_APP_CODE_READBACK,         // Read App code
    /* 14 */eIAP_CMD_LOG,                       // Enable debug log

    eIAP_CMD_NUMBERS,
} eIAP_CMD;

class TransferProtocol : public QObject
{
    Q_OBJECT

public:
#pragma pack(push, 1)     /* set alignment to 1 byte boundary */

    typedef enum
    {
        eDEVICE_APP,
        eDEVICE_BOOTCODE,

        eDEVICE_NUMBERS,
    } eDEVICE;

    typedef enum
    {
        ePAYLOAD_TYPE_EXE_WRITE,
        ePAYLOAD_TYPE_EXE_READ,
        ePAYLOAD_TYPE_REPLY,
        ePAYLOAD_TYPE_ACK,

        ePAYLOAD_TYPE_NUMBERS,
    } ePAYLOAD_TYPE;

    typedef enum
    {
        eACK_TYPE_NONACK,
        eACK_TYPE_ACK,
        eACK_TYPE_BADPACKET,
        eACK_TYPE_TIMEOUT,
        eACK_TYPE_UNKNOWN,
        eACK_TYPE_FEEDBACK,
        eACK_TYPE_ERROR,

        eACK_TYPE_NUMBERS,
    } eACK_TYPE;

    typedef enum
    {
        eMSG_STATE_MAGIC_NUMBER,
        eMSG_STATE_PACKET_HEADER,
        eMSG_STATE_PACKET_PAYLOAD,
        eMSG_STATE_DATA_READY,
        eMSG_STATE_BAD_PACKET,
        eMSG_STATE_TIMEOUT,
        eMSG_STATE_RUN_OUT_OF_MEMORY,

        eMSG_STATE_NUMBERS,
    } eMSG_STATE;

    // Payload
    typedef struct
    {
        BYTE    cModule;
        BYTE    cSubCmd;
        BYTE    acData[1024 + 32];
    } sPAYLOAD;

    typedef struct
    {
        BYTE    cSource;
        BYTE    cDestination;
        BYTE    cPacketType;
        WORD    wSeqId;
        WORD    wPacketSize;
        WORD    wChecksum;
    } sMSG_PACKET_HEADER;

    typedef struct
    {
        BYTE                cMagicNumber1;
        BYTE                cMagicNumber2;
        sMSG_PACKET_HEADER  sPacketHeader;

        union uFORMAT
        {
            sPAYLOAD        sPayload;
            BYTE            acPayload[sizeof(sPAYLOAD)];
        } uFormat;
    } sMSG_PACKET_FORMAT;

    typedef struct
    {
        sMSG_PACKET_FORMAT  sMsgPacket;
        eMSG_STATE          eMsgParsingState;
        WORD                wRecivedByteCount;
        WORD                wRecivedByteCRC;
    } sMSG_STATE_DATA;

#pragma pack(pop)   /* restore original alignment from stack */

    TransferProtocol(QObject *parent = nullptr);
    ~TransferProtocol();
    
    //parserStateReset();
    //bool returnPayload(sMSG_PACKET_FORMAT *psMsg);
    void packetBuild(sMSG_PACKET_FORMAT *psPacket, eDEVICE eSource, eDEVICE eDestination, ePAYLOAD_TYPE ePayloadType, WORD wSeqId, WORD wDataSize, sPAYLOAD *psPayload);
    void packageAck(sMSG_PACKET_FORMAT *psPacket, eDEVICE eSource, eDEVICE eDestination, eACK_TYPE eAckType);
    void packageSend(sMSG_PACKET_FORMAT *psPacket);
    SerialPort *m_serial = nullptr;

signals:
    void recived_data(const QByteArray &data);
    void writeData(const QByteArray &data);
    void ready();

public slots:
    //void parser(char data);
    //void receive_data(const QByteArray &data);
    //void readData(const QByteArray &data);
    //void waitReady();
    bool commandWrite(eIAP_CMD eCmd, WORD wSize, BYTE *pcData);
    bool commandRead(eIAP_CMD eCmd, QByteArray &data);

private:
    WORD                        wSeqId;
    //QMutex                      m_Mutex;
    //sMSG_STATE_DATA             m_sMsgState;
    //QQueue<sMSG_PACKET_FORMAT>  m_qMegQueue;
    //QQueue<char>                m_qRecivedQueue;
    //QThread                     *m_parserThread;

    //void resetState();
    void calculateCheckSum(sMSG_PACKET_FORMAT *psPacket);
};

#endif // TRANSFERPROTOCOL_H
