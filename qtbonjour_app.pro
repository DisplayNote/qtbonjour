QT += core gui qml quick concurrent
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
TEMPLATE = app
TARGET = qtbonjour

win32 {
INCLUDEPATH += "C:\Program Files\Bonjour SDK\Include"
LIBS += -L"C:\Program Files\Bonjour SDK\Lib\Win32" -ldnssd -lws2_32
QMAKE_LFLAGS_WINDOWS += /NODEFAULTLIB:libcmt.lib
}

linux {
LIBS += -ldns_sd
}

SOURCES += \
    main.cpp \
    bonjourrecord.cpp \
    bonjourservicebrowser.cpp \
    bonjourserviceregister.cpp \
    bonjourserviceresolver.cpp

    
HEADERS += \
    bonjourrecord.h \
    bonjourservicebrowser.h \
    bonjourserviceregister.h \
    bonjourserviceresolver.h

