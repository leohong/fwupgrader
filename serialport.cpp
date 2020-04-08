#include <QLabel>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QDataStream>
#include <QDebug>
#include <QThread>
#include <QMutex>

#include "serialport.h"

SerialPort::SerialPort(QObject *parent) :
    QObject(parent),
    m_port(new QSerialPort(this))
{
}

SerialPort::~SerialPort()
{
    m_port->close();
    m_port->deleteLater();
    //my_thread->quit();
    //my_thread->wait();
    //my_thread->deleteLater();
}

bool SerialPort::openSerialPort(Settings &Settings)
{
    bool ret = false;
    m_port->setPortName(Settings.name);
    m_port->setBaudRate(Settings.baudRate);
    m_port->setDataBits(Settings.dataBits);
    m_port->setParity(Settings.parity);
    m_port->setStopBits(Settings.stopBits);
    m_port->setFlowControl(Settings.flowControl);
    m_port->setReadBufferSize(1);

    if(m_port->open(QIODevice::ReadWrite))
    {
        //my_thread = new QThread();
        //this->moveToThread(my_thread);
        //m_port->moveToThread(my_thread);
        //my_thread->start();

        emit openSerialPortSucess();
        ret = true;
    }
    else {
        emit openSerialPortError(m_port->errorString());
    }

    //connect(m_port, &QSerialPort::readyRead, this, &SerialPort::handle_data);

    return ret;
}

void SerialPort::closeSerialPort()
{
    if(m_port->isOpen())
    {
        m_port->close();
        //my_thread->quit();
        //my_thread->wait();
        //my_thread->deleteLater();
    }
    emit handleError(this->m_port->error());
}

void SerialPort::handle_data()
{
    QByteArray data = m_port->readAll();
    emit receive_data(data);
}

void SerialPort::write_data(const QByteArray &data)
{
    m_Mutex.lock();
    m_port->write(data);
    m_port->waitForBytesWritten();
    emit handleError(this->m_port->error());
    m_Mutex.unlock();
}

bool SerialPort::changeBaudrate(qint32 newBaudrate)
{
    bool ret = false;
    if(m_port->isOpen()) {
        m_port->close();
        m_port->setBaudRate(newBaudrate);
    }

    if(m_port->open(QIODevice::ReadWrite)) {
        emit handleError(this->m_port->error());
        ret = true;
    }

    return ret;
}
