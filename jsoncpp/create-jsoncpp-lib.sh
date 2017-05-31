#!/bin/sh

echo "Generating library makefiles..."
echo
nios2-lib-generate-makefile --lib-name jsoncpp \
    --src-dir . \
    --inc-dir . \
    --lib-dir ../../build/nios2/lib/jsoncpp \
    --public-inc-dir ../ \
    --public-inc-dir . \
    --verbose

echo
echo "Running make..."
echo
cd ../../build/nios2/lib/jsoncpp && make
