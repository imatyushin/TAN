cmake_minimum_required(VERSION 3.10)

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

#cl-helper
#SET(clHelper_DIR ../../../../../OpenCLHelper/clHelper)
# this builds the clHelper library (optional, if you want
# to manually access the globally generated symbols)
#ADD_SUBDIRECTORY(${clHelper_DIR} cmake-clHelper-bin)
# specify include paths for #include's in opencl files:
#OPENCL_INCLUDE_DIRECTORIES(${OpenCL_INCLUDE_DIRS})

# compile some opencl kernels. This properly preprocessor-exapands
# and test-compiles the given .cl files (once to asm, once to llvm),
# and puts the preprocessor-expanded code (as a char[] array)
# into dedicated .c files (that can be accessed through the
# implicit EMBEDDED_OPENCL_KERNELS variable
#COMPILE_OPENCL(
#  ../../../src/Graal/amdFFT_conv_kernels.cl
#  ../../../src/Graal/amdFIR_kernels.cl
#  ../../../src/Graal/GraalFHT.cl
#  ../../../src/Graal/GraalUtil.cl
#  )

include_directories(../../../../../amf)
include_directories(../../../..)

ADD_DEFINITIONS(-DTAN_SDK_EXPORTS)
ADD_DEFINITIONS(-DCLFFT_EXPORTS)
#if(COMPILER_MSVC)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

# library name
project(Graal)

# sources
set(
  SOURCE_LIB
  ../../../src/common/OCLHelper.cpp
  ../../../src/Graal/amdFHT.cpp
  ../../../src/Graal/GraalConv.cpp
  ../../../src/Graal/GraalConvOCL.cpp
  ../../../src/Graal/GraalConv_clFFT.cpp
)

set(
  HEADER_LIB
  ../../../src/common/OCLHelper.h
  ../../../src/Graal/amdFHT.h
  ../../../src/Graal/GraalConv.hpp
  ../../../src/Graal/GraalConvOCL.hpp
  ../../../src/Graal/GraalConv_clFFT.hpp
  ../../../src/Graal/GraalInternal.h
  )

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