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

project(jsonrpccppserver CXX)

file(GLOB JSONRPCCPP_SERVER_FILES
    "*.h"
    "*.cpp"
    "connectors/linuxtcpsocketserver.h"
    "connectors/linuxtcpsocketserver.cpp"
)

add_library(jsonrpccppserver ${JSONRPCCPP_SERVER_FILES})

target_include_directories(jsonrpccppserver PRIVATE
    ${CIA_DIR}/thirdparty
)

target_link_libraries(jsonrpccppserver
    jsonrpccppcommon
    pthread
)

set_target_properties(jsonrpccppserver
    PROPERTIES
    SOVERSION 0.0.0
    OUTPUT_NAME jsonrpccpp-server
)

install(TARGETS jsonrpccppserver
    LIBRARY
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
