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

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_library(jsoncpp STATIC ${JSONCPP_FILES})

set_target_properties(jsoncpp
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_DIR}/build/thirdparty/${BUILD_SYSTEM_TYPE}"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_DIR}/build/thirdparty/${BUILD_SYSTEM_TYPE}"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_DIR}/build/thirdparty/${BUILD_SYSTEM_TYPE}"
    OUTPUT_NAME jsoncpp
)
