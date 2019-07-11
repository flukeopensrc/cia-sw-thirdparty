#-------------------------------------------------
#
# Copyright (c) 2019 Fluke Corporation, Inc. All rights reserved.
# Use of the software source code and warranty disclaimers are
# identified in the Software Agreement associated herewith.
#
# Repository URL:    git@git.sesg.fluke.com:fcal/CIA
# Authored By:       B.J.Araujo
# Origin:            CIA
#
# Project build file for the "CIA" project.
#
#-------------------------------------------------

project(ciausbtmc CXX)

file(GLOB USBTMC_CIA_FILES
    "*.h"
    "*.c"
)

add_library(ciausbtmc STATIC ${REMOTE_CIA_FILES})

set_target_properties(ciausbtmc
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/og"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/og"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/og"
    OUTPUT_NAME usbtmc
)

set_target_properties(ciausbtmc PROPERTIES LINKER_LANGUAGE CXX)
