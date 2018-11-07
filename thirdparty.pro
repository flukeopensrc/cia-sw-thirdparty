#*****************************************************************************
#    Copyright (c) 2016 Fluke Corporation, Inc. All rights reserved.
#*****************************************************************************
#
#   Repository URL:    git.sesg.fluke.com:fcal/cia/sw/src.git
#   Authored By:       Dave Bartley
#   Origin:            FCAL Common Instrument Architecture
#
#   Project file for the value namespace library.
#
#*****************************************************************************
#   Use of the software source code and warranty disclaimers are
#   identified in the Software Agreement associated herewith.
#*****************************************************************************

# This project invokes multiple other builds
TEMPLATE = subdirs

# These are the subdirectories to run make when making the whole system.
SUBDIRS = \
	jsoncpp \
	jsonrpccpp \
	usbtmc

# Set Makefile for object files of this build based on which qmake is used.
# And for non-CDA builds, run the unit tests.
linux-oe-g++ {
    message("This a CDA target build.")
}
else {
    message("This a host build.")
}
