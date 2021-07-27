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

project(jsonrpccppclient CXX)

file(GLOB JSONRPCCPP_CLIENT_FILES
    "*.h"
    "*.cpp"
    "connectors/linuxtcpsocketclient.h"
    "connectors/linuxtcpsocketclient.cpp"
)

add_library(jsonrpccppclient ${JSONRPCCPP_CLIENT_FILES})

target_include_directories(jsonrpccppclient PRIVATE
    ${CIA_DIR}/thirdparty
)

set_target_properties(jsonrpccppclient
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    SOVERSION 0.0.0
    OUTPUT_NAME jsonrpccpp-client
)

install(TARGETS jsonrpccppclient
    LIBRARY 
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
