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
    "server/*.h"
    "server/*.cpp"
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_library(jsonrpccpp STATIC ${JSONRPCCPP_FILES})
