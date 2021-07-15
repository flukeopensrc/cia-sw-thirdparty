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

file(GLOB JSONRPCCPP_TOPLEVEL_HEADERS
    "*.h"
)
file(GLOB JSONRPCCPP_COMMON_HEADERS
    "common/*.h"
)
file(GLOB JSONRPCCPP_CLIENT_HEADERS
    "client/*.h"
)
file(GLOB JSONRPCCPP_SERVER_HEADERS
    "server/*.h"
)
file(GLOB JSONRPCCPP_FILES
    "common/*.h"
    "common/*.cpp"
    "client/*.h"
    "client/*.cpp"  
    "client/connectors/linuxtcpsocketclient.h"
    "client/connectors/linuxtcpsocketclient.cpp"
    "server/*.h"
    "server/*.cpp"   
    "server/connectors/linuxtcpsocketserver.h"
    "server/connectors/linuxtcpsocketserver.cpp"
)

add_library(jsonrpccpp ${JSONRPCCPP_FILES})

target_include_directories(jsonrpccpp PRIVATE
    ${CIA_DIR}/thirdparty
)

set_target_properties(jsonrpccpp
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    SOVERSION 0.0.0
    OUTPUT_NAME jsonrpccpp
)

install(TARGETS jsonrpccpp
    LIBRARY 
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
install(FILES
    ${JSONRPCCPP_TOPLEVEL_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/jsonrpccpp/"
)
install(FILES
    ${JSONRPCCPP_COMMON_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/jsonrpccpp/common"
)
install(FILES
    ${JSONRPCCPP_CLIENT_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/jsonrpccpp/client"
)
install(FILES
    ${JSONRPCCPP_SERVER_HEADERS}
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/jsonrpccpp/server"
)
