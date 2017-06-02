#!/bin/sh

echo "Generating library makefiles..."
echo
nios2-lib-generate-makefile --lib-name jsoncpp \
    --src-dir . \
    --inc-dir . \
    --lib-dir ../../build/nios2/lib/jsoncpp \
    --set CXXFLAGS -std=c++11 -D_GLIBCXX_USE_C99 \
    --verbose

echo
echo "Running make..."
echo
cd ../../build/nios2/lib/jsoncpp && make
