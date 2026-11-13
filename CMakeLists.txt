# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.21)

project(QtQuickAutofill VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(QTQUICKAUTOFILL_BUILD_EXAMPLE "Build the example application" ${PROJECT_IS_TOP_LEVEL})
option(QTQUICKAUTOFILL_BUILD_TESTS "Build the unit tests (desktop only)" ${PROJECT_IS_TOP_LEVEL})

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.5)

# Standalone build: put all QML modules (library, example) under one import path so that qmllint finds them
if(PROJECT_IS_TOP_LEVEL AND NOT QT_QML_OUTPUT_DIRECTORY)
    set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")
endif()

include(cmake/QtQuickAutofillAndroid.cmake)

add_subdirectory(src)

if(QTQUICKAUTOFILL_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(QTQUICKAUTOFILL_BUILD_TESTS AND NOT ANDROID AND NOT IOS)
    enable_testing()
    add_subdirectory(tests)
endif()
