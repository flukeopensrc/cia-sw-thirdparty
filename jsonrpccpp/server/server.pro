#*****************************************************************************
#    Copyright (c) 2016 Fluke Corporation, Inc. All rights reserved.
#*****************************************************************************
#
#   Repository URL:    <URL to Containing Repository>
#   Authored By:       <Original Author>
#   Origin:            <List of projects this file has been part of>
#
#   One-line summary of file purpose.
#
#*****************************************************************************
#   Use of the software source code and warranty disclaimers are
#   identified in the Software Agreement associated herewith.
#*****************************************************************************

# This project builds a library.
TEMPLATE = lib

# Make target created for this project.
TARGET = jsonrpccpp-server

# Where the library goes. This can be overridden.
#!defined(DESTDIR) {
#    DESTDIR = ../../../QBuild/lib
#}

# Include files *************************************************************
include(../../shared/common.pri)

# Project configuration *****************************************************

# If this contains no QOBJECTs or other Qt code, remove it from QT and CONFIG.
QT -= gui
QT -= core
CONFIG -= qt

# Make this a static library
CONFIG += static

# The compiler on line is a spaz. So I am turning warnings off for now.
#CONFIG += warn_on
CONFIG += warn_off

# The qmake on my linux box is old. Otherwise, I could add c++11 to CONFIG like this:
CONFIG += c++11
# But instead I do this:
#QMAKE_CXXFLAGS += -std=c++11

# Where generated intermediate files go.
#OBJECTS_DIR = obj
#MOC_DIR = qt
#RCC_DIR = qt
#UI_DIR = qt

# Where compiler should look for include files.
# By convention, all includes should include the subsystem path.
# The dependency path should mirror the include path, so if a header changes,
# the appropriate files recompile.
INCLUDEPATH += .. ../../
DEPENDPATH += ..

# Tell qmake where to clean things.
#QMAKE_CLEAN = $$DESTDIR/*

#* Source and header files ***************************************************

# Source files.
#exists(*.cpp) {
#    SOURCES += *.cpp
#}

# Header files.
#exists(*.h) {
#    HEADERS += *.h
#}
