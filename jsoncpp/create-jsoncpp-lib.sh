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

BUILDDIR="../../build/nios2/$LIBNAME"

#* Source paths **************************************************************
SRCDIR=.

#* Include paths *************************************************************
INCDIR=.

#* Misc config ***************************************************************
#-D_GLIBCXX_USE_C99 ensures std::stoul and some others are defined.
#-D_GLIBCXX_USE_CXX11_ABI=0 forces gcc to use the old std::string ABI,
# which isn't c++11 compliant... After upgrading to GCC 5, I couldn't get
# everything to link properly without using the old ABI; I suspect there is an
# IntelFPGA library somewhere, which I don't have control over, that uses the old ABI
CXXFLAGS="-std=c++11 -D_GLIBCXX_USE_C99 -D_GLIBCXX_USE_CXX11_ABI=0"

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
cd $BUILDDIR &&
make &&

echo
echo "Moving the library to the lib directory..."
echo
mkdir -p ../lib
mv "lib$LIBNAME.a" "../lib/lib$LIBNAME.a" &&

echo "All done!"
