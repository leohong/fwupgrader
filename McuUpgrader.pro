QT += widgets serialport
requires(qtConfig(combobox))

TARGET = McuUpgrader
TEMPLATE = app

SOURCES += \
    TransferProtocol.cpp \
    inteltobin.cpp \
    main.cpp \
    mainwindow.cpp \
    mcuUpgrade.cpp \
    serialport.cpp \
    settingsdialog.cpp \
    console.cpp

HEADERS += \
    Release.h \
    TransferProtocol.h \
    commonTypeDef.h \
    inteltobin.h \
    mainwindow.h \
    mcuUpgrade.h \
    serialport.h \
    settingsdialog.h \
    console.h

FORMS += \
    mainwindow.ui \
    settingsdialog.ui

RESOURCES += \
    McuUpgrader.qrc

target.path = D:/ProjectCode/McuUpgrader/trunk
INSTALLS += target

DISTFILES += \
    config.json

RC_FILE +=\
    exe_ico.rc
