TEMPLATE = lib

win32{
CONFIG += staticlib
}

TARGET = qtbonjour

# Qt 6.8 LTS is the floor (AB#141677): anything deprecated up to it fails to build.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060800

# Bonjour SDK (dns_sd.h) for Windows, from Conan.
include($$PWD/conaninstall.pri)

linux {
LIBS += -ldns_sd
}


SOURCES += \
    bonjourrecord.cpp \
    bonjourservicebrowser.cpp \
    bonjourserviceregister.cpp \
    bonjourserviceresolver.cpp

    
HEADERS += \
    bonjourrecord.h \
    bonjourservicebrowser.h \
    bonjourserviceregister.h \
    bonjourserviceresolver.h

## Install rules ##
isEmpty(LIBDIR) {
    LIBDIR = lib
}

BUILD_TYPE=Debug
CONFIG(release, debug|release) {
    BUILD_TYPE=Release
}

# Installation prefix and library directory
isEmpty(PREFIX) {
    linux:   PREFIX = install/Linux/$$BUILD_TYPE
    win32:   PREFIX = install/Windows/$$BUILD_TYPE
    android: PREFIX = install/Android/$$BUILD_TYPE
    ios:     PREFIX = install/iOS/$$BUILD_TYPE
    macx:    PREFIX = install/Macos/$$BUILD_TYPE
}

headers.files = $$HEADERS
headers.path = $$PREFIX/include

target.path = $$PREFIX/$$LIBDIR

INSTALLS += target headers
## ~ ##
