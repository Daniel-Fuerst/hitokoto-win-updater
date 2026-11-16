# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/version/UpdaterVersion.cmake")

project(hitokoto-win-updater
    VERSION ${UPDATER_VERSION_CORE}
    DESCRIPTION "Windows updater for hitokoto"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(UPDATER_BUILD_QT "Build Qt platform and CLI targets (off = core library and its tests only)" ON)
option(UPDATER_BUILD_TESTS "Build unit and integration tests" ON)
option(UPDATER_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(UPDATER_SANITIZE "" CACHE STRING "Comma-separated sanitizers, e.g. address,undefined")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(UpdaterCompilerOptions)
include(UpdaterDependencies)

add_subdirectory(src/core)

if(UPDATER_BUILD_QT)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Core Network)
    qt_standard_project_setup()
    add_subdirectory(src/platform)
    add_subdirectory(src/cli)
endif()

if(UPDATER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
