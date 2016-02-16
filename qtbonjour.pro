TEMPLATE = lib
#CONFIG += staticlib
TARGET = qtbonjour

win32 {
INCLUDEPATH += "C:\Program Files\Bonjour SDK\Include"
LIBS += -LC:\Program Files\Bonjour SDK\Lib\Win32 -ldnssd
}

linux {
LIBS += -ldns_sd
}


SOURCES += \
    bonjourservicebrowser.cpp \
    bonjourserviceregister.cpp \
    bonjourserviceresolver.cpp

    
HEADERS += \
    bonjourrecord.h \
    bonjourservicebrowser.h \
    bonjourserviceregister.h \
    bonjourserviceresolver.h

