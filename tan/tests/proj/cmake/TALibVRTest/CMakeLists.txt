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

include(${TAN_ROOT}/utils/cmake/test_OpenCL.cmake)

# name
project(TALibVRTest DESCRIPTION "TALibVRTest")


include_directories(${AMF_HOME}/amf)
include_directories(${TAN_HEADERS})

include_directories(${TAN_ROOT}/utils/common)
include_directories(${TAN_ROOT}/tan/samples/src/GPUUtilities)
include_directories(${TAN_ROOT}/tan/samples/src/TrueAudioVR)
include_directories(${TAN_ROOT}/tan/samples/src/common)

ADD_DEFINITIONS(-D_CONSOLE)
ADD_DEFINITIONS(-D_LIB)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

# sources
set(
  SOURCE_EXE
  ${TAN_ROOT}/utils/common/wav.cpp
  ${TAN_ROOT}/utils/common/FileUtility.cpp

  ${TAN_ROOT}/tan/samples/src/common/GpuUtils.cpp

  ../../../src/TALibVRTest/stdafx.cpp
  ../../../src/TALibVRTest/TALibVRTest.cpp

  )

set(
  HEADER_EXE
  ${TAN_ROOT}/utils/common/wav.h
  ${TAN_ROOT}/utils/common/FileUtility.h

  ${TAN_ROOT}/tan/samples/src/common/GpuUtils.h

  ../../../src/TALibVRTest/stdafx.h
  ../../../src/TALibVRTest/targetver.h
  )

# create binary
add_executable(
  TALibVRTest
  ${SOURCE_EXE}
  ${HEADER_EXE}
  )

target_link_libraries(TALibVRTest TrueAudioNext)
target_link_libraries(TALibVRTest GPUUtilities)
target_link_libraries(TALibVRTest TrueAudioVR)
target_link_libraries(TALibVRTest ${OpenCL_LIBRARY})

add_custom_command(
  TARGET TALibVRTest
  POST_BUILD

  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TrueAudioNext_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:TALibVRTest>
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${TrueAudioVR_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:TALibVRTest>
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${GPUUtilities_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:TALibVRTest>

  VERBATIM
  )

if(FFTW_DIR)
    add_custom_command(
        TARGET TALibVRTest
        POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_directory "${fftw_BINARY_DIR}/$<IF:$<CONFIG:Debug>,Debug,Release>" $<TARGET_FILE_DIR:TALibVRTest>

        VERBATIM
        )
endif()
