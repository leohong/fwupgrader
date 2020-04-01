#include <QLabel>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QFileDialog>
#include <QDataStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "mcuUpgrade.h"

McuUpgrade::McuUpgrade(QObject *parent) :
    QObject(parent),
    m_transferProtocol(new TransferProtocol(this)),
    m_intelToBin(new IntelHex),
    m_companyCntfromJson(2),
    m_UpgradeThread(new QThread)
{
    this->moveToThread(m_UpgradeThread);
    m_transferProtocol->moveToThread(m_UpgradeThread);
    m_transferProtocol->m_serial->moveToThread(m_UpgradeThread);
    m_transferProtocol->m_serial->m_port->moveToThread(m_UpgradeThread);
    m_UpgradeThread->start();

    connect(this, &McuUpgrade::commandWrite, m_transferProtocol, &TransferProtocol::commandWrite);
    connect(this, &McuUpgrade::commandRead, m_transferProtocol, &TransferProtocol::commandRead);
    connect(this, &McuUpgrade::changeBaudrate, m_transferProtocol->m_serial, &SerialPort::changeBaudrate);

    openConfigFile();
}

McuUpgrade::~McuUpgrade()
{
}

void McuUpgrade::readFwVersion()
{
    QByteArray acVersion;
    int count = 0;

    for(count = 0; count < GET_COMPANY_SIZE; count++) {
        int retry = 20;

        changeBaudrate(m_strCompanyCfg.at(GET_COMPANY_BAUDRATE(count)).toInt());
        m_transferProtocol->m_serial->write_data(m_strCompanyCfg.at(GET_COMPANY_READCLI(count)).toUtf8());

        if(m_transferProtocol->m_serial->m_port->waitForReadyRead(1000)) {
            acVersion = m_transferProtocol->m_serial->m_port->readAll();

            while(retry--) {
                if(m_transferProtocol->m_serial->m_port->waitForReadyRead(10)) {
                    acVersion.append(m_transferProtocol->m_serial->m_port->readAll());
                }
            }

            break;
        }
    }

    qDebug() << "version =" << acVersion;

    if(count < GET_COMPANY_SIZE) {
        acVersion.remove(0, 2);
        acVersion.insert(0, tr(m_strCompanyCfg.at(GET_COMPANY_NAME(count)).toUtf8()));
        qDebug() << "version =" << acVersion;
        showString(acVersion);
    }
    else {
        popMessage(tr("No Response!!\r\nPlease try again!!"));
    }
}

bool McuUpgrade::goToBootloader(DWORD dwCompany)
{
    bool ret = false;

    changeBaudrate(m_strCompanyCfg.at(GET_COMPANY_BAUDRATE(static_cast<int>(dwCompany))).toInt());

    m_transferProtocol->m_serial->m_port->clear();
    m_transferProtocol->m_serial->write_data(m_strCompanyCfg.at(GET_COMPANY_BOOTCLI(static_cast<int>(dwCompany))).toUtf8());

    this->thread()->sleep(2);

    changeBaudrate(BOOTCODE_BAUDRATE);
    m_transferProtocol->m_serial->m_port->clear();

    return ret;
}

bool McuUpgrade::setIapEnable(DWORD dwStartAddress, DWORD dwBinSize)
{
    bool ret = false;
    BYTE cDummy = 0;
    sIAP_INFO sInfo;

    sInfo.dwStart = dwStartAddress;
    sInfo.dwSize = dwBinSize;

    if(false == commandWrite(eIAP_CMD_APP_INFO, sizeof(sIAP_INFO), reinterpret_cast<BYTE *>(&sInfo)))
        return false;

    ret = commandWrite(eIAP_CMD_IAP_ENABLE, 0, &cDummy);

    return ret;
}

bool McuUpgrade::writePageBinary(WORD wSize, BYTE *pcBuffer)
{
    bool ret = false;

    ret = commandWrite(eIAP_CMD_BIN_DATA, wSize, pcBuffer);

    return ret;
}

bool McuUpgrade::checkPageChecksum(DWORD dwChecksum)
{
    bool ret = false;
    QByteArray retChecksum;
    DWORD *pdwRetChecksum = nullptr;

    ret = commandRead(eIAP_CMD_BIN_CHECK_SUM, retChecksum);

    pdwRetChecksum = reinterpret_cast<DWORD *>(retChecksum.data());

    if(0 == ((*pdwRetChecksum + dwChecksum) & 0xFFFFFFFF))
    {
        ret = true;
    }

    return ret;
}

bool McuUpgrade::programPage(DWORD dwAddress)
{
    bool ret = false;

    ret = commandWrite(eIAP_CMD_PROGRAMING, sizeof(DWORD), reinterpret_cast<BYTE*>(&dwAddress));

    return ret;
}

bool McuUpgrade::writeTag(DWORD dwFwVersion, DWORD dwCompany)
{
    bool ret = false;
    DWORD dwTag[2];
    dwTag[0] = dwFwVersion;
    dwTag[1] = dwCompany;

    ret = commandWrite(eIAP_CMD_PROGRAMING_FINISH, sizeof(DWORD)*2, reinterpret_cast<BYTE*>(dwTag));
    return ret;
}

bool McuUpgrade::goToAppCode()
{
    bool ret = false;
    ret = commandWrite(eIAP_CMD_RUN_APP, 0, nullptr);
    return ret;
}

void McuUpgrade::loadFile(const QString &fileName)
{
    sMEM_TAG_PARAM *psTag = nullptr;
    quint32 address = 0;
    char *data = nullptr;
    QString string;
    int count = 0;

    m_intelToBin->open(fileName, (1024));
    m_intelToBin->reReadAll();
    m_intelToBin->selectSegment(1);
    m_intelToBin->readPage(address, &data, 0);

    psTag = reinterpret_cast<sMEM_TAG_PARAM*>(data);

    m_dwFwVersion = psTag->dwCodeVersion;

    for(count = 0; count < GET_COMPANY_SIZE; count++)
    {
        if(psTag->dwCompany == static_cast<DWORD>(m_strCompanyCfg.at(GET_COMPANY_ID(count)).toInt()))
        {
            m_dwCompany = static_cast<DWORD>(count);
            break;
        }
    }

    if(GET_COMPANY_SIZE > count){
        string = tr("%1 V%2.%3").arg(m_strCompanyCfg.at(GET_COMPANY_NAME(count))).arg((m_dwFwVersion >> 8 )&0x00FF).arg(QString::number(m_dwFwVersion&0x00FF));
        showString(string);
    }
    else {
        popMessage(tr("Firmware file is incorrect!!\r\nPlease try again!!"));
    }
}

void McuUpgrade::upgrade()
{
    WORD wTotalPages = 0;
    DWORD dwChecksum = 0;
    quint32 address = 0;
    char *data = nullptr;

    enableButtons(false);
    showString("Starting please wait...");

    m_intelToBin->reReadAll();
    m_intelToBin->selectSegment(1);

    // 1. go to bootloader
    goToBootloader(m_dwCompany);

    // 2. Enable In Application Programming
    if(false == setIapEnable(APP_START_ADDRESS, m_intelToBin->segmentSize())) {
        popMessage(tr("Can't enter programming mode!\r\nPlease check the connection and try again!!"));
        goto ERROR;
    }

    // 3. write binary data and check the checksum
    while (0 != m_intelToBin->readPage(address, &data, 0)) {
        qDebug() << QString::number(address, 16);
        dwChecksum = 0;

        for (uint32_t i = 0; i < m_intelToBin->pageSize; i++) {
            dwChecksum += static_cast<BYTE>(data[i]);
        }

        if(false == writePageBinary(static_cast<WORD>(m_intelToBin->pageSize), reinterpret_cast<BYTE *>(data))) {
            popMessage(tr("Transfer data Error!\r\nPlease try again!!"));
            goto ERROR;
        }

        if(true == checkPageChecksum(dwChecksum)) {
            programPage(address);
            updateProgress(0, (m_intelToBin->segmentPages()-1), wTotalPages++);
        }
        else {
            popMessage(tr("Checksum Error!\r\nPlease try again!!"));
            goto ERROR;
        }
    }

    // 4. write tag
    if(false == writeTag(m_dwFwVersion, m_dwCompany)){
        popMessage(tr("Transfer data Error!\r\nPlease try again!!"));
        goto ERROR;
    }

    // 5. go to appcode
    goToAppCode();
    enableButtons(true);
    return;

ERROR:
    enableButtons(true);
    showString("Error! Please try again.");
}

void McuUpgrade::openConfigFile()
{
    QFile loadFile("./config.json");

    if (!loadFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open configuration file.");

        m_strCompanyCfg << "1" << "115200" << "set fwupgrade 1\r\n" << "sys read_revision\r\n" << "Calibre" \
                        << "2" << "9600" << "~00997 1\r\n" << "~00122 1\r\n" << "OPTOMA";
        qDebug() << "return";
        return;
    }

    QByteArray configData = loadFile.readAll();
    QJsonParseError json_error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(configData, &json_error);

    loadFile.close();

    qDebug() << json_error.errorString();

    if(json_error.error != QJsonParseError::NoError)
    {
        qDebug() << "json error!";
        m_strCompanyCfg << "1" << "115200" << "set fwupgrade 1\r\n" << "sys read_revision\r\n" << "Calibre" \
                        << "2" << "9600" << "~00997 1\r\n" << "~00122 1\r\n" << "OPTOMA";
        return;
    }

    QJsonObject rootObj = jsonDoc.object();
    QJsonObject subObj;
    QStringList keys = rootObj.keys();
    QStringList readConfig;
    QJsonArray subArray;

    m_companyCntfromJson = keys.size();

    for (int i = 0; i < keys.size(); i++) {
        qDebug() << "Company" << keys.at(i);
        subArray = rootObj.value(keys.at(i)).toArray();

        for (int j = 0; j < subArray.size(); j++) {
            m_strCompanyCfg << subArray.at(j).toString();
        }

        m_strCompanyCfg << keys.at(i);
    }
}
