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
    "client/connectors/filedescriptorclient.h"
    "client/connectors/filedescriptorclient.cpp"
    "client/connectors/tcpsocketclient.h"
    "client/connectors/tcpsocketclient.cpp"
    "client/connectors/unixdomainsocketclient.h"
    "client/connectors/unixdomainsocketclient.cpp"
    "client/connectors/linuxtcpsocketclient.h"
    "client/connectors/linuxtcpsocketclient.cpp"
    "client/connectors/tcpsocketclientprivate.h"
    "client/connectors/tcpsocketclient.h"
    "client/connectors/tcpsocketclient.cpp"
    "server/*.h"
    "server/*.cpp"   
    "server/connectors/filedescriptorserver.cpp"
    "server/connectors/filedescriptorserver.h"
    "server/connectors/linuxtcpsocketserver.cpp"
    "server/connectors/linuxtcpsocketserver.h"
    "server/connectors/tcpsocketserver.cpp"
    "server/connectors/tcpsocketserver.h"
    "server/connectors/unixdomainsocketserver.cpp"
    "server/connectors/unixdomainsocketserver.h"
)

list(FILTER JSONRPCCPP_FILES EXCLUDE REGEX ".*windows.*$")
list(FILTER JSONRPCCPP_FILES EXCLUDE REGEX ".*httpserver.*$")

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
