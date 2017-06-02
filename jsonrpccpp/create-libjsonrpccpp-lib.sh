#*****************************************************************************
#    Copyright (c) 2017 Fluke Corporation, Inc. All rights reserved.
#*****************************************************************************
#
#   Repository URL:    git.sesg.fluke.com:/fcal/cia/sw/thirdparty.git
#   Authored By:       Trevor Vannoy
#   Origin:            FCAL Common Instrument Architecture
#
#   Build script for the jsonrpccpp library.
#
#*****************************************************************************
#   Use of the software source code and warranty disclaimers are
#   identified in the Software Agreement associated herewith.
#*****************************************************************************
#!/bin/sh

LIBNAME=jsonrpccpp

BUILDDIR="../../build/nios2/$LIBNAME"

#* Source paths **************************************************************
CLIENTDIR=client
COMMONDIR=common
SERVERDIR=server

#* Include paths *************************************************************
INCDIR=../

#* Misc config ***************************************************************
#-D_GLIBCXX_USE_C99 ensures std::stoul and some others are defined.
CXXFLAGS="-std=c++11"

#* Build the library *************************************************************
echo "Generating library makefiles..."
echo

nios2-lib-generate-makefile \
    --lib-name $LIBNAME \
    --lib-dir $BUILDDIR \
    --src-dir $CLIENTDIR \
    --src-dir $COMMONDIR \
    --src-dir $SERVERDIR \
    --inc-dir $INCDIR \
    --set CXXFLAGS $CXXFLAGS \
    --verbose

echo
echo "Running make..."
echo
cd $BUILDDIR
make

echo
echo "Moving the library to the lib directory..."
echo
mv "lib$LIBNAME.a" "../lib/lib$LIBNAME.a"

echo "All done!"
