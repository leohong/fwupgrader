/****************************************************************************
**
** Copyright (C) 2012 Denis Shienkov <denis.shienkov@gmail.com>
** Copyright (C) 2012 Laszlo Papp <lpapp@kde.org>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSerialPort module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "console.h"
#include "settingsdialog.h"
#include "Release.h"

#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QDebug>
#include <QMetaType>

//static McuUpgrade *m_mcuUpgrade = new McuUpgrade;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow),
    m_status(new QLabel),
    //m_console(new Console),
    m_settings(new SettingsDialog),
    m_mcuUpgrade(new McuUpgrade)
{
    m_ui->setupUi(this);
    this->setWindowTitle(tr("AIO Firmware Upgrader %1.%2").arg(VER_MAJOR).arg(VER_MINOR));
    m_ui->progressBar->setValue(100);
    m_ui->progressBar->setFormat(tr("AIO Firmware Upgrader %1.%2").arg(VER_MAJOR).arg(VER_MINOR));

    //m_console->setEnabled(false);

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionOpenFile->setEnabled(true);
    m_ui->actionUpgrade->setEnabled(false);
    m_ui->actionVerify->setEnabled(false);
    m_ui->actionDisconnect->setEnabled(false);

    m_ui->statusBar->addWidget(m_status);

    initActionsConnections();

    qRegisterMetaType<Settings>("Settings&");
    qRegisterMetaType<QSerialPort::SerialPortError>("QSerialPort::SerialPortError");

}

MainWindow::~MainWindow()
{
    delete m_settings;
    delete m_ui;
}

void MainWindow::openSerialPort()
{
    Settings p = m_settings->settings();
    //m_console->setEnabled(true);
    //m_console->setLocalEchoEnabled(p.localEchoEnabled);

    m_ui->actionConnect->setEnabled(false);
    m_ui->actionUpgrade->setEnabled(false);
    m_ui->actionVerify->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(true);

    if(m_mcuUpgrade->m_intelToBin->fileOpen)
        m_ui->actionUpgrade->setEnabled(true);

    showStatusMessage(tr("Connected to %1 : %2, %3, %4, %5, %6")
                      .arg(p.name).arg(p.stringBaudRate).arg(p.stringDataBits)
                      .arg(p.stringParity).arg(p.stringStopBits).arg(p.stringFlowControl));
}

void MainWindow::openSerialError(QString error)
{
    QMessageBox::critical(this, tr("Error"), error);
    showStatusMessage(tr("Open error"));
}

void MainWindow::closeSerialPort()
{
    //m_console->setEnabled(false);
    m_ui->actionConnect->setEnabled(true);
    m_ui->actionUpgrade->setEnabled(false);
    m_ui->actionVerify->setEnabled(false);
    m_ui->actionDisconnect->setEnabled(false);

    showStatusMessage(tr("Disconnected"));
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Simple Terminal"),
                       tr("The <b>Simple Terminal</b> example demonstrates how to "
                          "use the Qt Serial Port module in modern GUI applications "
                          "using Qt, with a menu bar, toolbars, and a status bar."));
}

void MainWindow::writeData(const QByteArray &data)
{
    m_mcuUpgrade->m_transferProtocol->m_serial->m_port->write(data);
}

void MainWindow::readData(const QByteArray &data)
{
    //m_console->putData(data.toHex());
}

void MainWindow::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QMessageBox::critical(this, tr("Critical Error"), m_mcuUpgrade->m_transferProtocol->m_serial->m_port->errorString());
        closeSerialPort();
    }
}

void MainWindow::initActionsConnections()
{
    connect(m_ui->actionConnect, &QAction::triggered, m_settings, &SettingsDialog::show);
    connect(m_ui->actionDisconnect, &QAction::triggered, this->m_mcuUpgrade->m_transferProtocol->m_serial, &SerialPort::closeSerialPort);
    connect(m_ui->actionDisconnect, &QAction::triggered, this, &MainWindow::closeSerialPort);
    connect(m_ui->actionOpenFile, &QAction::triggered, this, &MainWindow::openFile);
    connect(m_ui->actionUpgrade, &QAction::triggered, m_mcuUpgrade, &McuUpgrade::upgrade);
    connect(m_ui->actionVerify, &QAction::triggered, m_mcuUpgrade, &McuUpgrade::readFwVersion);

    connect(m_settings, &SettingsDialog::openPort, this->m_mcuUpgrade->m_transferProtocol->m_serial, &SerialPort::openSerialPort);
    connect(this->m_mcuUpgrade->m_transferProtocol->m_serial, &SerialPort::openSerialPortSucess, this, &MainWindow::openSerialPort);

    connect(m_mcuUpgrade->m_transferProtocol->m_serial, &SerialPort::openSerialPortError, this, &MainWindow::openSerialError);
    connect(m_mcuUpgrade->m_transferProtocol->m_serial, &SerialPort::handleError, this, &MainWindow::handleError);
    connect(m_mcuUpgrade, &McuUpgrade::updateProgress, this, &MainWindow::prograssBarUpdate);
    connect(m_mcuUpgrade, &McuUpgrade::showString, this, &MainWindow::prograssBarString);
    connect(m_mcuUpgrade, &McuUpgrade::popMessage, this,  &MainWindow::popMessage);

    connect(m_mcuUpgrade, &McuUpgrade::getPortSetting, m_settings, &SettingsDialog::settings);
    connect(m_mcuUpgrade, &McuUpgrade::changeBaudrate, m_settings, &SettingsDialog::ChangeBaudrate);
    connect(m_mcuUpgrade, &McuUpgrade::enableButtons, this, &MainWindow::enableButtons);
}

void MainWindow::enableButtons(bool enable)
{
    m_ui->actionOpenFile->setEnabled(enable);
    m_ui->actionUpgrade->setEnabled(enable);
    m_ui->actionVerify->setEnabled(enable);
    m_ui->actionDisconnect->setEnabled(enable);
}

void MainWindow::showStatusMessage(const QString &message)
{
    m_status->setText(message);
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName();

    if (!fileName.isEmpty())
    {
        m_mcuUpgrade->loadFile(fileName);

        if(m_mcuUpgrade->m_transferProtocol->m_serial->m_port->isOpen())
            m_ui->actionUpgrade->setEnabled(true);
    }
}

void MainWindow::prograssBarUpdate(int min, int max, int value)
{
    m_ui->progressBar->setFormat("%p%");
    m_ui->progressBar->setMinimum(min);
    m_ui->progressBar->setMaximum(max);
    m_ui->progressBar->setValue(value);
}

void MainWindow::prograssBarString(QString string)
{
    m_ui->progressBar->setFormat(string);
}

void MainWindow::popMessage(QString popMessage)
{
    QMessageBox::critical(this, tr("Critical Error"), popMessage);
}
