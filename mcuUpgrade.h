#ifndef MCUUPGRADE_H
#define MCUUPGRADE_H

#include <QMainWindow>
#include <QObject>
#include <QFile>
#include <QQueue>
#include <QMutex>
#include "commonTypeDef.h"
#include "TransferProtocol.h"
#include "inteltobin.h"
#include "serialport.h"

#define CALIBRE (1)
#define OPTOMA  (2)
#define BOOTCODE_BAUDRATE (115200)
#define APP_START_ADDRESS (0x8000)

#define COMPANY_MEMBERS         (5)
#define COMPANY_OFFSET(x)       (x * COMPANY_MEMBERS)
#define GET_COMPANY_ID(x)       (0 + COMPANY_OFFSET(x))
#define GET_COMPANY_BAUDRATE(x) (1 + COMPANY_OFFSET(x))
#define GET_COMPANY_BOOTCLI(x)  (2 + COMPANY_OFFSET(x))
#define GET_COMPANY_READCLI(x)  (3 + COMPANY_OFFSET(x))
#define GET_COMPANY_NAME(x)     (4 + COMPANY_OFFSET(x))
#define GET_COMPANY_SIZE        (m_companyCntfromJson)

class McuUpgrade : public QObject
{
    Q_OBJECT

#pragma pack(push, 1)     /* set alignment to 1 byte boundary */
    typedef struct
    {
        DWORD dwStart;
        DWORD dwSize;
    } sIAP_INFO;

    typedef struct
    {
        DWORD dwCodeVersion;
        DWORD dwCodeStartupAddress;
        DWORD dwCodeSize;
        DWORD dwCodeChecksum;
        DWORD dwCompany;
    } sMEM_TAG_PARAM;
#pragma pack(pop)     /* set alignment to 1 byte boundary */

signals:
    void updateProgress(unsigned int min, unsigned int  max, unsigned int  value);
    void showString(QString string);
    bool commandWrite(eIAP_CMD eCmd, WORD wSize, BYTE *pcData);
    bool commandRead(eIAP_CMD eCmd, QByteArray &data);
    Settings getPortSetting();
    void popMessage(QString Message);
    bool changeBaudrate(qint32 newBaudrate);
    void enableButtons(bool enable);

public slots:
    void loadFile(const QString &fileName);
    void upgrade();
    void readFwVersion();

public:
    McuUpgrade(QObject *parent = nullptr);
    ~McuUpgrade();

    void openConfigFile();
    TransferProtocol *m_transferProtocol = nullptr;
    IntelHex *m_intelToBin = nullptr;

private:
    DWORD m_dwFwVersion = 0;
    DWORD m_dwCompany = 0;
    WORD m_wSeqId = 0;
    QList<QString> m_strCompanyCfg;
    int m_companyCntfromJson;

    bool goToBootloader(DWORD dwCompany);
    bool setIapEnable(DWORD dwStartAddress, DWORD dwBinSize);
    bool writePageBinary(WORD wSize, BYTE *pcBuffer);
    bool checkPageChecksum(DWORD dwChecksum);
    bool programPage(DWORD dwAddress);

    bool writeTag(DWORD dwFwVersion, DWORD dwCompany);
    bool goToAppCode();

    QThread *m_UpgradeThread = nullptr;
};

#endif // MCUUPGRADE_H
