#!/bin/sh

echo "Generating library makefiles..."
echo
nios2-lib-generate-makefile --lib-name jsonrpccpp \
    --src-dir client \
    --src-dir common \
    --src-dir server \
    --inc-dir . \
    --inc-dir .. \
    --lib-dir ../../build/nios2/lib/jsonrpccpp \
    --use-lib-dir ../../build/nios2/lib/jsoncpp \
    --public-inc-dir ../ \
    --set CXXFLAGS -std=c++11 \
    --verbose

echo
echo "Running make..."
echo
cd ../../build/nios2/lib/jsonrpccpp && make
