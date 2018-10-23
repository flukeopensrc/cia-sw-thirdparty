#-------------------------------------------------
#
# Authored By:       Tektronix
# Adapted By:        B.J.Araujo
#
# Origin:            Tektronix
#
# Project build file for usbtmc library.
#
#-------------------------------------------------

TARGET = usbtmc
TEMPLATE = lib
CONFIG += staticlib
CONFIG += separate_debug_info
CONFIG -= debug_and_release
CONFIG( debug, debug|release ) {
    CONFIG -= release
    CONFIG += debug
    QMAKE_CXXFLAGS += -DDEBUG
}
else {
    CONFIG -= debug
    CONFIG += release
    QMAKE_CXXFLAGS += -DNDEBUG
}

CONFIG += c++11

include(usbtmc.pri)
