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

file(GLOB JSONCPP_PUBLIC_HEADERS
    "json/*.h"
)

file(GLOB JSONCPP_FILES
    "*.h"
    "*.cpp"
)

add_library(jsoncpp ${JSONCPP_FILES})

target_include_directories(jsoncpp PRIVATE
    ${CIA_DIR}/thirdparty
)

set_target_properties(jsoncpp
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    LIBRARY_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    RUNTIME_OUTPUT_DIRECTORY "${CIA_BIN_DIR}/thirdparty"
    SOVERSION 0.0.0
    OUTPUT_NAME jsoncpp
    PUBLIC_HEADER "${JSONCPP_PUBLIC_HEADERS}"
)

install(TARGETS jsoncpp
    LIBRARY 
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE
        DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    PUBLIC_HEADER
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/json/"
)
