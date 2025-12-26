cmake_minimum_required(VERSION 3.16)
project(beamline_worker_caf)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# CAF (C++ Actor Framework)
find_library(CAF_CORE_LIB caf_core REQUIRED)
find_library(CAF_IO_LIB caf_io REQUIRED)
find_library(CAF_OPENSSL_LIB caf_openssl)

find_path(CAF_INCLUDE_DIR caf/actor_system.hpp REQUIRED)

include_directories(${CAF_INCLUDE_DIR})

# Optional packages for blocks
pkg_check_modules(CURL libcurl)
pkg_check_modules(SQLITE sqlite3)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CAF_INCLUDE_DIRS}
)

if(CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
endif()

if(SQLITE_FOUND)
    include_directories(${SQLITE_INCLUDE_DIRS})
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/worker_actor.cpp
    src/block_executor.cpp
    src/scheduler.cpp
    src/sandbox.cpp
    src/observability.cpp
    src/blocks/http_block.cpp
    src/blocks/fs_block.cpp
    src/blocks/sql_block.cpp
    src/blocks/human_block.cpp
)

# Create executable
add_executable(beamline_worker ${SOURCES})

# Link libraries
target_link_libraries(beamline_worker
    ${CAF_CORE_LIB}
    ${CAF_IO_LIB}
    ${CMAKE_THREAD_LIBS_INIT}
)

if(CAF_OPENSSL_LIB)
    target_link_libraries(beamline_worker ${CAF_OPENSSL_LIB})
endif()

if(CURL_FOUND)
    target_link_libraries(beamline_worker ${CURL_LIBRARIES})
endif()

if(SQLITE_FOUND)
    target_link_libraries(beamline_worker ${SQLITE_LIBRARIES})
endif()

# Enhanced compiler flags for better code quality
target_compile_options(beamline_worker PRIVATE
    -Wall -Wextra -Werror
    -Wpedantic -Wmissing-declarations
    -Wshadow -Wpointer-arith -Wcast-align
    -Wwrite-strings -Wconversion
    -Wunreachable-code -Waggregate-return
    -Wstrict-overflow=5
)

# Coverage support
option(ENABLE_COVERAGE "Enable code coverage reporting" OFF)
if(ENABLE_COVERAGE)
    target_compile_options(beamline_worker PRIVATE --coverage)
    target_link_libraries(beamline_worker --coverage)
endif()

# Debug builds get additional flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(beamline_worker PRIVATE
        -g -O0 -DDEBUG
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
endif()

# Installation
install(TARGETS beamline_worker
    RUNTIME DESTINATION bin
)

# Tests
enable_testing()
add_subdirectory(tests)