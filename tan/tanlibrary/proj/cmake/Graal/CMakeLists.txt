﻿cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

include(${TAN_ROOT}/utils/cmake/test_OpenCL.cmake)
include(${TAN_ROOT}/utils/cmake/test_amf.cmake)

#cl kernel preprocessor
include(${TAN_ROOT}/utils/cmake/CLKernelPreprocessor.cmake)

include_directories(${AMF_HOME}/amf)
include_directories(${TAN_HEADERS})
include_directories(${TAN_ROOT}/tan/tanlibrary/src/clFFT-master/src/include)
include_directories(${TAN_ROOT}/utils/common)

ADD_DEFINITIONS(-DTAN_SDK_EXPORTS)
ADD_DEFINITIONS(-DCLFFT_EXPORTS)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

# library name
project(Graal)

# sources
set(
  SOURCE_LIB

  ${TAN_ROOT}/utils/common/OCLHelper.cpp

  ../../../src/Graal/amdFHT.cpp
)

#if(TAN_NO_OPENCL)
#  list(APPEND SOURCE_LIB ../../../src/Graal/GraalConvAMF.cpp)
#else()
  list(APPEND SOURCE_LIB ../../../src/Graal/GraalConv.cpp)
  list(APPEND SOURCE_LIB ../../../src/Graal/GraalConvOCL.cpp)
  list(APPEND SOURCE_LIB ../../../src/Graal/GraalConv_clFFT.cpp)
#endif()

set(
  HEADER_LIB

  ${TAN_ROOT}/utils/common/OCLHelper.h

  ../../../src/Graal/amdFHT.h
  ../../../src/Graal/GraalInternal.h
  )

#if(TAN_NO_OPENCL)
#  list(APPEND HEADER_LIB ../../../src/Graal/GraalConvAMF.hpp)
#else()
  list(APPEND HEADER_LIB ../../../src/Graal/GraalConv.hpp)
  list(APPEND HEADER_LIB ../../../src/Graal/GraalConvOCL.hpp)
  list(APPEND HEADER_LIB ../../../src/Graal/GraalConv_clFFT.hpp)
#endif()

# cl kernels compilation
set(
  Graal_CL_Directories
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/converter"
  "${TAN_ROOT}/tan/tanlibrary/src/Graal"
  "${TAN_ROOT}/tan/tanlibrary/src/Graal"
  "${TAN_ROOT}/tan/tanlibrary/src/Graal"
  "${TAN_ROOT}/tan/tanlibrary/src/Graal"
  )
set(
  Graal_CL_Files
  "Converter.cl"
  "GraalUtil.cl"
  "GraalFHT.cl"
  "amdFFT_conv_kernels.cl"
  "amdFIR_kernels.cl"
  )
set(
  Graal_CL_Output
  "CLKernel_Converter.h"
  "OclKernels/CLKernel_GraalUtil.h"
  "OclKernels/CLKernel_GraalFHT.h"
  "OclKernels/CLKernel_amdFFT_conv_kernels.h"
  "OclKernels/amdFIR_kernels.h"
  )

markGenerated(
  Graal_CL_Directories
  Graal_CL_Output
  OutHeaders
  )

#append generated headers
list(LENGTH OutHeaders OutHeadersCount)
math(EXPR OutHeaders_MaxIndex ${OutHeadersCount}-1)

foreach(FileIndex RANGE ${OutHeaders_MaxIndex})
  list(GET OutHeaders ${FileIndex} OutFile)
  list(APPEND HEADER_LIB ${OutFile})
endforeach()

# declare static library creation
# that embeds OpenCL kernels
add_library(
  Graal
  STATIC
  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

generateCLKernel(
  Graal
  Graal_CL_Directories
  Graal_CL_Files
  Graal_CL_Output
  )

add_dependencies(Graal CLKernelPreprocessor)

set_property(TARGET Graal PROPERTY POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
  target_compile_options(Graal PUBLIC -mavx2)
  target_compile_options(Graal PUBLIC -mfma)
  target_compile_options(Graal PUBLIC -msse4.2)

  if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    target_compile_options(Graal PUBLIC -g)
  endif()

  if(NOT APPLE)
    target_compile_options(Graal PUBLIC -Wpsabi)
  endif()
endif()