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

project(usbtmc CXX)

file(GLOB USBTMC_CIA_FILES
    "*.h"
    "*.c"
)

add_library(usbtmc STATIC ${USBTMC_CIA_FILES})

target_include_directories(usbtmc PRIVATE
#    ${CIA_DIR}/src
#    ${CIA_DIR}/src/ogsrc
#    ${CIA_DIR}/src/shared
#    ${CIA_DIR}/src/thridparty
)

set_target_properties(usbtmc PROPERTIES LINKER_LANGUAGE CXX)

set_target_properties(usbtmc
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/og"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/og"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/og"
    OUTPUT_NAME usbtmc
)
