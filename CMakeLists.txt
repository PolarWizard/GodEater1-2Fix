# MIT License
#
# Copyright (c) 2025 Dominik Protasewicz
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

cmake_minimum_required(VERSION 3.28)

# Variables
set(PROJECT_NAME GodEater1-2Fix)

set(INSTALL_PATH_OK false)
if(DEFINED CMAKE_INSTALL_PREFIX AND NOT CMAKE_INSTALL_PREFIX STREQUAL "")
    if(NOT EXISTS ${CMAKE_INSTALL_PREFIX})
        message(FATAL_ERROR "Install path '${CMAKE_INSTALL_PREFIX}' does not exist")
    else()
        set(VALID_EXES "GER.exe" "GE2RB.exe")
        foreach(EXE ${VALID_EXES})
            if(EXISTS "${CMAKE_INSTALL_PREFIX}/${EXE}")
                set(INSTALL_PATH_OK true)
                message(STATUS "Path: '${CMAKE_INSTALL_PREFIX}' yields valid game exe: '${EXE}'")
            endif()
        endforeach()

        if(NOT INSTALL_PATH_OK)
            message(WARNING
                "Provided install path is unexpected: '${CMAKE_INSTALL_PREFIX}'\n"
                "The path does not contain any of the following valid game exe's: ${VALID_EXES}\n"
                "Will use anyway during installation... but are you sure this is correct?"
            )
        endif()
    endif()
else()
    set(CMAKE_INSTALL_PREFIX "" CACHE PATH "Path to game folder" FORCE)
    message(WARNING
        "Install path not specified! Using 'cmake --install' will NOT work!\n"
        "If this is a mistake reconfigure using: '-DCMAKE_INSTALL_PREFIX=<path-to-game-folder>'\n"
    )
endif()

# Force all MSVC Runtimes to be linked statically into DLL
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)

# Set the project name and version
project(${PROJECT_NAME} VERSION 1.0)
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Define the common output directory
set(OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Configure debug and release directories separately
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${OUTPUT_DIRECTORY}/Debug)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${OUTPUT_DIRECTORY}/Release)

# Include dependencies via FetchContent
include(cmake/Dependencies.cmake)

# Add DLL
set(DLL_FILES src/dllmain.cpp src/utils.cpp)
add_library(${PROJECT_NAME} SHARED ${DLL_FILES})

# Add /utf-8 flag for MSVC
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE "/utf-8")
endif()

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE inc)

# Link dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
    Zydis
    safetyhook
    spdlog::spdlog
    yaml-cpp
)

if(INSTALL_PATH_OK)
    install(CODE "
        execute_process(
            COMMAND
                powershell.exe
                    -ExecutionPolicy Bypass
                    -File \"${CMAKE_SOURCE_DIR}/install.ps1\" \"${CMAKE_INSTALL_PREFIX}\"
        )
    ")
endif()
