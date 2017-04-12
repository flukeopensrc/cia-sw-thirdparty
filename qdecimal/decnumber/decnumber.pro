#
#
#
include(../common.pri)

QT -= gui
TEMPLATE = lib
# decnumber library should always be static
# regardless Qdecimal is static or dynamic
CONFIG += staticlib
DEPENDPATH += .
TARGET = decnumber

CONFIG -= debug_and_release

unix {
    QMAKE_CXXFLAGS_DEBUG += -Wno-unused-value
    QMAKE_CFLAGS_DEBUG += -Wno-unused-value
    QMAKE_CXXFLAGS_RELEASE += -Wno-unused-value
    QMAKE_CFLAGS_RELEASE += -Wno-unused-value
}

win32-g++ {
    QMAKE_CXXFLAGS_DEBUG += -Wno-unused-value
    QMAKE_CFLAGS_DEBUG += -Wno-unused-value
    QMAKE_CXXFLAGS_RELEASE += -Wno-unused-value
    QMAKE_CFLAGS_RELEASE += -Wno-unused-value
}

win32:CONFIG(release, debug|release): DESTDIR = ../lib/release
else:win32:CONFIG(debug, debug|release): DESTDIR = ../lib/debug


# Input
HEADERS += decContext.h \
           decDouble.h \
           decDPD.h \
           decimal128.h \
           decimal32.h \
           decimal64.h \
           decNumber.h \
           decNumberLocal.h \
           decPacked.h \
           decQuad.h \
           decSingle.h \
           decCommon.c \
           decBasic.c \
           Port_stdint.h

SOURCES += decBasic.c \
           decCommon.c \
           decContext.c \
           decDouble.c \
           decimal128.c \
           decimal32.c \
           decimal64.c \
           decNumber.c \
           decPacked.c \
           decQuad.c \
           decSingle.c
