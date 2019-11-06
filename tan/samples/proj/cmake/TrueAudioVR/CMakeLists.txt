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
include(${TAN_ROOT}/utils/cmake/test_amf.cmake)

# cl kernel to hex header
include(${TAN_ROOT}/utils/cmake/FileToHeader.cmake)

# name
project(TrueAudioVR DESCRIPTION "TrueAudioVR")

#
ADD_DEFINITIONS(-DTAN_SDK_EXPORTS)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)
ADD_DEFINITIONS(-DTAN_SDK_EXPORTS)

include_directories(${AMF_HOME}/amf)
include_directories(../../../..)
include_directories(${TAN_ROOT}/utils/common)

# sources
set(
  SOURCE_LIB
  ../../../src/TrueAudioVR/stdafx.cpp
  ../../../src/TrueAudioVR/TrueAudioVR.cpp
  ${TAN_ROOT}/utils/common/cpucaps.cpp
  )

if(WIN32)
  list(APPEND SOURCE_LIB ../../../src/TrueAudioVR/dllmain.cpp)
else()
endif()

set(
  HEADER_LIB

  ../../../src/TrueAudioVR/Header.h
  ../../../src/TrueAudioVR/resource.h
  ../../../src/TrueAudioVR/resource1.h
  ../../../src/TrueAudioVR/stdafx.h
  ../../../src/TrueAudioVR/targetver.h
  ../../../src/TrueAudioVR/TrueAudioVR.h

  ${TAN_ROOT}/utils/common/cpucaps.h
  )

# create library
add_library(
  TrueAudioVR
  SHARED
  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

# cl kernels compilation
set(
  CL_Directories
  "${TAN_ROOT}/tan/samples/src/TrueAudioVR/Kernels"
  "${TAN_ROOT}/tan/samples/src/TrueAudioVR/Kernels"
  )
set(
  CL_Files
  "Fill.cl"
  "GenerateRoomResponse.cl"
  )
set(
  CL_Output
  "Fill"
  "GenerateRoomResponse"
  )

generateCLKernelHeader(
  TrueAudioVR
  CL_Directories
  CL_Files
  CL_Output
  )

# linking
target_link_libraries(TrueAudioVR ${OpenCL_LIBRARY})
target_link_libraries(TrueAudioVR ${AMF_NAME})

if(NOT WIN32)
  target_compile_options(TrueAudioVR PUBLIC -mavx2)
  target_compile_options(TrueAudioVR PUBLIC -mfma)
  target_compile_options(TrueAudioVR PUBLIC -msse4.2)
  target_compile_options(TrueAudioVR PUBLIC -g) #todo: to Debug only
  
  if(NOT APPLE)
    target_compile_options(TrueAudioVR PUBLIC -Wpsabi)
  endif()
endif()