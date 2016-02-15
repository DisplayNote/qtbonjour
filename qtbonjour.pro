TEMPLATE = lib
CONFIG += staticlib
TARGET = qtbonjour


INCLUDEPATH += "C:\Program Files\Bonjour SDK\Include"
LIBS += -LC:\Program Files\Bonjour SDK\Lib\Win32
LIBS += -ldnssd

SOURCES += \
    bonjourservicebrowser.cpp \
    bonjourserviceregister.cpp \
    bonjourserviceresolver.cpp

    
HEADERS += \
    bonjourrecord.h \
    bonjourservicebrowser.h \
    bonjourserviceregister.h \
    bonjourserviceresolver.h

