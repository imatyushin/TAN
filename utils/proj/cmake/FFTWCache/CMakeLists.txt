cmake_minimum_required(VERSION 3.10)

# The cmake-policies(7) manual explains that the OLD behaviors of all
# policies are deprecated and that a policy should be set to OLD only under
# specific short-term circumstances.  Projects should be ported to the NEW
# behavior and not rely on setting a policy to OLD.

# VERSION not allowed unless CMP0048 is set to NEW
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

if(NOT FFTW_DIR)
  message(SEND_ERROR "Error: path to FFTW library is not set!")
  return()
endif()

message("")
message("FFTW_DIR: " ${FFTW_DIR})

#test the ${FFTW_DIR} is not an absolute path
if(NOT IS_ABSOLUTE ${FFTW_DIR})
  #test this cmake is included from other project
  if(NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    get_filename_component(Path_To_FFTW "${CMAKE_SOURCE_DIR}/${FFTW_DIR}" ABSOLUTE)
  #assume that the project is built from itself
  else()
    get_filename_component(Path_To_FFTW "${CMAKE_CURRENT_SOURCE_DIR}/${FFTW_DIR}" ABSOLUTE)
  endif()
else()
  get_filename_component(Path_To_FFTW ${FFTW_DIR} ABSOLUTE)
endif()

message("FFTW_DIR expanded: " ${Path_To_FFTW})

ADD_DEFINITIONS(-DUSE_FFTW)
ADD_DEFINITIONS(-D_CONSOLE)

# name
project(FFTWCache DESCRIPTION "FFTWCache")

include_directories(${Path_To_FFTW})
include_directories(../../../common)

# sources
set(
  SOURCE_EXE
  ../../../src/FFTWCache/FFTWCache.cpp
  ../../../src/FFTWCache/stdafx.cpp

  ${TAN_ROOT}/utils/common/FileUtility.cpp
  ${TAN_ROOT}/utils/common/StringUtility.cpp
  )

set(
  HEADER_EXE
  ../../../src/FFTWCache/stdafx.h
  ../../../src/FFTWCache/targetver.h

  ${TAN_ROOT}/utils/common/FileUtility.h
  ${TAN_ROOT}/utils/common/StringUtility.h
  )

# create binary
add_executable(
  FFTWCache
  ${SOURCE_EXE}
  ${HEADER_EXE}
  )

target_link_libraries(FFTWCache fftw3f)

add_custom_command(
  TARGET FFTWCache
  POST_BUILD

  COMMAND ${CMAKE_COMMAND} -E copy_directory "${fftw_BINARY_DIR}" $<TARGET_FILE_DIR:FFTWCache>
  )