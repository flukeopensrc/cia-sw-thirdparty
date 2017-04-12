#
#
#
TEMPLATE = subdirs
CONFIG += staticlib
CONFIG += ordered
CONFIG -= debug_and_release
CONFIG( debug, debug|release ) {
    CONFIG -= release
}
else {
    CONFIG -= debug
    CONFIG += release
}

SUBDIRS = decnumber src
