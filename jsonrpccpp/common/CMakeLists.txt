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

project(jsonrpccppcommon CXX)

file(GLOB JSONRPCCPP_COMMON_FILES
    "*.h"
    "*.cpp"
)

add_library(jsonrpccppcommon ${JSONRPCCPP_COMMON_FILES})

target_include_directories(jsonrpccppcommon PRIVATE
    ${CIA_DIR}/thirdparty
)

target_link_libraries(jsonrpccppcommon
    jsoncpp
)

set_target_properties(jsonrpccppcommon
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    SOVERSION 0.0.0
    OUTPUT_NAME jsonrpccpp-common
)

install(TARGETS jsonrpccppcommon
    LIBRARY 
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)
