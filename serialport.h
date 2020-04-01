#ifndef SERIALPORT_H
#define SERIALPORT_H

#include <QObject>
#include <QWidget>
#include <QSerialPort>
#include <QMutex>

struct Settings {
    QString name;
    qint32 baudRate;
    QString stringBaudRate;
    QSerialPort::DataBits dataBits;
    QString stringDataBits;
    QSerialPort::Parity parity;
    QString stringParity;
    QSerialPort::StopBits stopBits;
    QString stringStopBits;
    QSerialPort::FlowControl flowControl;
    QString stringFlowControl;
    bool localEchoEnabled;
};

class SerialPort : public QObject
{
    Q_OBJECT

public:
    explicit SerialPort(QObject *parent = nullptr);
    ~SerialPort();

    bool changeBaudrate(qint32 newBaudrate);
    QSerialPort *m_port;

public slots:
    void handle_data();
    void write_data(const QByteArray &data);
    bool openSerialPort(Settings &Settings);
    void closeSerialPort();

signals:
    void receive_data(const QByteArray &data);
    void openSerialPortError(QString errorString);
    void openSerialPortSucess();
    void handleError(QSerialPort::SerialPortError error);

private:
    QThread *my_thread;
    QMutex  m_Mutex;
};

#endif // SERIALPORT_H
