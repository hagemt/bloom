cmake_minimum_required(VERSION 2.8.0 FATAL_ERROR)
project(Bloom C)

# Force a build type to be declared
set(CMAKE_BUILD_TYPES "(None|Debug|Release|RelWithDebInfo|MinSizeRel)")
if(NOT CMAKE_BUILD_TYPE MATCHES "${CMAKE_BUILD_TYPES}")
  set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "${CMAKE_BUILD_TYPES}" FORCE)
endif(NOT CMAKE_BUILD_TYPE MATCHES "${CMAKE_BUILD_TYPES}")
# Specify some default directories
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}/lib")

# Version info
set(BLOOM_VERSION_MAJOR 0)
set(BLOOM_VERSION_MINOR 1)
set(BLOOM_VERSION_STRING "v${BLOOM_VERSION_MAJOR}.${BLOOM_VERSION_MINOR}")

# Build options
option(BUILD_MONITOR "Build the monitoring daemon" OFF)

find_library(LIBALGO calg c-algorithm REQUIRED)
find_library(LIBHASH crypto ssl openssl REQUIRED)
find_library(LIBMATH m math REQUIRED)

add_executable(bloom bloom.c)
target_link_libraries(bloom ${LIBALGO} ${LIBHASH} ${LIBMATH})

if(BUILD_MONITOR)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(GTK_PKG gtk+-2.0)
  include_directories(${GTK_PKG_INCLUDE_DIRS})
  link_directories(${GTK_PKG_LIBRARY_DIRS})
  # Require libnotify
  find_library(LIBNOTIFY notify REQUIRED)
  # Daemon target
  add_executable(bloomd monitor.c)
  target_link_libraries(bloomd ${GTK_PKG_LIBRARIES} ${LIBNOTIFY})
endif(BUILD_MONITOR)

include(CTest)
if(BUILD_TESTING)
  add_test(no_arg_bloom "${EXECUTABLE_OUTPUT_PATH}/bloom")
  add_test(simple_bloom "${EXECUTABLE_OUTPUT_PATH}/bloom" "${CMAKE_SOURCE_DIR}/bloom_test")
  if(BUILD_MONITOR)
    add_test(no_arg_bloomd "${EXECUTABLE_OUTPUT_PATH}/bloomd")
  endif(BUILD_MONITOR)
  if(UNIX)
    find_program(MEMORYCHECK_COMMAND valgrind REQUIRED)
  endif(UNIX)
endif(BUILD_TESTING)
