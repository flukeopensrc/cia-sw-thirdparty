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

project(jsonrpccpp CXX)

file(GLOB JSONRPCCPP_FILES
    "common/*.h"
    "common/*.cpp"
    "client/*.h"
    "client/*.cpp"
    "client/connectors/*.h"
    "client/connectors/*.cpp"
    "server/*.h"
    "server/*.cpp"
    "server/connectors/*.h"
    "server/connectors/*.cpp"
)

list(FILTER JSONRPCCPP_FILES EXCLUDE REGEX ".*windows.*$")

add_library(jsonrpccpp STATIC ${JSONRPCCPP_FILES})

target_include_directories(jsonrpccpp PRIVATE
    ${CIA_DIR}/thirdparty
)

set_target_properties(jsonrpccpp
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    OUTPUT_NAME jsonrpccpp
)
