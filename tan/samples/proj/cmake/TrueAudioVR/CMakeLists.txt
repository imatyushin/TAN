cmake_minimum_required(VERSION 3.0)

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

find_package(OpenCL REQUIRED)
if(NOT OpenCL_FOUND)
	message(SEND_ERROR "Failed to find OpenCL")
	return()
else()
	include_directories(${OpenCL_INCLUDE_DIRS})
  link_directories(${OpenCL_LIBRARY})
endif()

# cl kernel preprocessor
include(../../../../samples/proj/cmake/CLKernelPreprocessor/CLKernelPreprocessor.cmake)

# name
project(TrueAudioVR DESCRIPTION "TrueAudioVR")

#
add_compile_definitions(TAN_SDK_EXPORTS)

#if(COMPILER_MSVC)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

include_directories(../../../../../amf)
include_directories(../../../..)

# sources
set(
  SOURCE_LIB
  ../../../../../AMF/public/common/Windows/ThreadWindows.cpp
  ../../../src/TrueAudioVR/dllmain.cpp
  ../../../src/TrueAudioVR/stdafx.cpp
  ../../../src/TrueAudioVR/TrueAudioVR.cpp
  )

set(
  HEADER_LIB
  ../../../../../AMF/public/common/TraceAdapter.h
  ../../../../../AMF/public/include/core/AudioBuffer.h
  ../../../../../AMF/public/include/core/Buffer.h
  ../../../../../AMF/public/include/core/Compute.h
  ../../../../../AMF/public/include/core/ComputeFactory.h
  ../../../../../AMF/public/include/core/Context.h
  ../../../../../AMF/public/include/core/Data.h
  ../../../../../AMF/public/include/core/Debug.h
  ../../../../../AMF/public/include/core/Factory.h
  ../../../../../AMF/public/include/core/Interface.h
  ../../../../../AMF/public/include/core/Plane.h
  ../../../../../AMF/public/include/core/Platform.h
  ../../../../../AMF/public/include/core/PropertyStorage.h
  ../../../../../AMF/public/include/core/PropertyStorageEx.h
  ../../../../../AMF/public/include/core/Result.h
  ../../../../../AMF/public/include/core/Surface.h
  ../../../../../AMF/public/include/core/Trace.h
  ../../../../../AMF/public/include/core/Variant.h
  ../../../../../AMF/public/include/core/Version.h
  ../../../src/TrueAudioVR/cpucaps.h
  ../../../src/TrueAudioVR/Header.h
  ../../../src/TrueAudioVR/resource.h
  ../../../src/TrueAudioVR/resource1.h
  ../../../src/TrueAudioVR/stdafx.h
  ../../../src/TrueAudioVR/targetver.h
  ../../../src/TrueAudioVR/TrueAudioVR.h
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