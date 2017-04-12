#
#
#
include(../common.pri)

QT -= gui
TEMPLATE = lib

# Pick if the library will be static or dynamic:
CONFIG += staticlib
# or dynamic (don't forget to define QDECIMAL_SHARED
#CONFIG += shared
#DEFINES += QDECIMAL_SHARED=2
# 1=import, client app, 2=export, source shared library (here)


TARGET = qdecimal
DEPENDPATH += .
# To include decnumber headers
INCLUDEPATH += ../decnumber

# Input
HEADERS += QDecContext.hh \
           QDecDouble.hh  \
           QDecPacked.hh  \
           QDecNumber.hh  \
           QDecSingle.hh  \
           QDecQuad.hh 

SOURCES += QDecContext.cc \
           QDecDouble.cc  \
           QDecPacked.cc  \
           QDecNumber.cc  \
           QDecSingle.cc  \
           QDecQuad.cc 

Q_OS_WIN32:CONFIG(release, debug|release): DESTDIR = ../lib/release
else:Q_OS_WIN32:CONFIG(debug, debug|release): DESTDIR = ../lib/debug

Q_OS_WIN32:CONFIG(release, debug|release): LIBS += ../lib/release -ldecnumber
else:Q_OS_WIN32:CONFIG(debug, debug|release): LIBS += ../lib/debug -ldecnumber

unix:!macx: LIBS += -L$$OUT_PWD/../decnumber/ -ldecnumber

INCLUDEPATH += $$PWD/../decnumber
DEPENDPATH += $$PWD/../decnumber

unix:!macx: PRE_TARGETDEPS += $$OUT_PWD/../decnumber/libdecnumber.a
