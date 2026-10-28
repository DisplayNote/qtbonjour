# GoogleTest suite for the qtbonjour library (roadmap 2.23).
#
# Links the static library built from ../qtbonjour.pro in the same build
# directory, so build that first. CI does it through the tests_coverage stage's
# libraryProjectFile. See docs/testing.md.

TEMPLATE = app
TARGET = qtbonjourtests

QT -= gui
QT += testlib
CONFIG += c++17 console
CONFIG -= app_bundle

DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060800

include(conan.pri)

INCLUDEPATH += $$PWD/.. $$PWD/fakes

LIBS += -L$$OUT_PWD -lqtbonjour
PRE_TARGETDEPS += $$OUT_PWD/qtbonjour.lib

# The fake dns_sd hands out real (never used) sockets for QSocketNotifier.
LIBS += -lws2_32

HEADERS += \
    $$PWD/fakes/fake_dns_sd.h

SOURCES += \
    $$PWD/fakes/fake_dns_sd.cpp \
    $$PWD/support/qtapplication.cpp \
    $$PWD/tst_bonjourrecord.cpp \
    $$PWD/tst_bonjourservicebrowser.cpp \
    $$PWD/tst_bonjourserviceresolver.cpp \
    $$PWD/tst_bonjourserviceregister.cpp
