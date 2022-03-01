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

target_link_libraries(jsonrpccppclient
    jsonrpccppcommon
)

set_target_properties(jsonrpccppclient
    PROPERTIES
    SOVERSION 0.0.0
    OUTPUT_NAME jsonrpccpp-client
)

install(TARGETS jsonrpccppclient
    LIBRARY
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT runtime
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        COMPONENT development
)
