#*****************************************************************************
#    Copyright (c) 2016 Fluke Corporation, Inc. All rights reserved.
#*****************************************************************************
#
#   Repository URL:    <URL to Containing Repository>
#   Authored By:       Dave Bartley
#   Origin:            FCAL Common Instrument Architecture
#
#   Standard qmake include for module builds.
#
#*****************************************************************************
#   Use of the software source code and warranty disclaimers are
#   identified in the Software Agreement associated herewith.
#*****************************************************************************

# Set subdirectory for object files of this build based on which qmake is used.
linux-oe-g++ {
    TARGET_SUBDIR = cda-arm
}
else {
    TARGET_SUBDIR = host
}

# Path to the build directory relative to the src builds.
isEmpty(BUILDPATH) {
    BUILDPATH = ../../../build
}

# Where third party include files are.
THIRDPARTYPATH = ../../../thirdparty

# Where the built library goes.
DESTDIR = $$OUT_PWD/$$BUILDPATH/$$TARGET_SUBDIR/lib

# Where the object files that constitute the built library go.
OBJECTS_DIR = $$OUT_PWD/$$BUILDPATH/$$TARGET_SUBDIR/$$TARGET

# Where Qt-generated files go.
MOC_DIR = qt
RCC_DIR = qt
UI_DIR = qt

# Where compiler should look for include files.
# By convention, all includes should include the subsystem path.
# The dependency path should mirror the include path, so if a header changes,
# the appropriate files recompile.
#
#INCLUDEPATH += .. $$THIRDPARTYPATH
#DEPENDPATH += .. $$THIRDPARTYPATH

# Add decnumber, assuming Scalar continues to use it.
#INCLUDEPATH += $$THIRDPARTYPATH/qdecimal/decnumber
#DEPENDPATH += $$THIRDPARTYPATH/qdecimal/decnumber

# Make this a static library.
#!contains(QT,static) {
#    CONFIG += static
#}

# Turn on warnings, but unused parameters are OK with me sometimes.
CONFIG += warn_on
QMAKE_CXXFLAGS += -Wno-unused-parameter

# The qmake on my host machine is old. Otherwise, I could add c++11 to CONFIG like this:
CONFIG += c++11
# But instead I do this:
#QMAKE_CXXFLAGS += -std=c++11

# Harvest source files from the directory.
exists($$OUT_PWD/*.cpp) {
    SOURCES += $$OUT_PWD/*.cpp
}

# Header files from the directory.
exists($$OUT_PWD/*.h) {
    HEADERS += $$OUT_PWD/*.h
}

# Harvest Qt forms from the directory.
#exists($$OUT_PWD/*.ui) {
#    FORMS += $$OUT_PWD/*.ui
#}

# Harvest Qt resources from the directory.
#exists($$OUT_PWD/*.rcc) {
#    RESOURCES += $$OUT_PWD/*.rcc
#}
