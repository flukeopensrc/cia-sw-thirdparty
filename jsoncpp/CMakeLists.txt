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

project(jsoncpp CXX)

file(GLOB JSONCPP_FILES
    "*.h"
    "*.cpp"
)

add_library(jsoncpp STATIC ${JSONCPP_FILES})

target_include_directories(jsoncpp PRIVATE
    ${CIA_DIR}/thirdparty
)

set_target_properties(jsoncpp
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    OUTPUT_NAME jsoncpp
)
