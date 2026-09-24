QT += core gui network qml quick concurrent widgets

TEMPLATE = app
TARGET = qtbonjour

# Qt 6.8 LTS is the floor (AB#141677): anything deprecated up to it fails to build.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060800

# Bonjour SDK (dns_sd.h, dnssd64.lib) for Windows, from Conan.
include($$PWD/conaninstall.pri)

win32 {
LIBS += -lws2_32
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

