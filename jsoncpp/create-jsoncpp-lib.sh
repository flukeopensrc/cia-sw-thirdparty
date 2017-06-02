#*****************************************************************************
#    Copyright (c) 2017 Fluke Corporation, Inc. All rights reserved.
#*****************************************************************************
#
#   Repository URL:    git.sesg.fluke.com:/fcal/cia/sw/thirdparty.git
#   Authored By:       Trevor Vannoy
#   Origin:            FCAL Common Instrument Architecture
#
#   Build script for the jsoncpp library.
#
#*****************************************************************************
#   Use of the software source code and warranty disclaimers are
#   identified in the Software Agreement associated herewith.
#*****************************************************************************
#!/bin/sh

LIBNAME=jsoncpp

LIBDIR=../../build/nios2/lib/

BUILDDIR="../../build/nios2/$LIBNAME"

#* Source paths **************************************************************
SRCDIR=.

#* Include paths *************************************************************
INCDIR=.

#* Misc config ***************************************************************
#-D_GLIBCXX_USE_C99 ensures std::stoul and some others are defined.
CXXFLAGS="-std=c++11 -D_GLIBCXX_USE_C99"

#* Build the app *************************************************************
echo "Generating library makefiles..."
echo

nios2-lib-generate-makefile \
    --lib-name $LIBNAME \
    --lib-dir $BUILDDIR \
    --src-dir $SRCDIR \
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
