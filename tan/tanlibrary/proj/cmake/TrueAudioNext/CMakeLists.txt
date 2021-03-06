cmake_minimum_required(VERSION 3.11)

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

####################################################################################
#OpenMP integration, part 1
####################################################################################
message("")
message("Start OpenMP search...")

find_package(OpenMP)

if(OpenMP_FOUND AND OMP_INCLUDE_DIR AND OpenMP_CXX_LIBRARIES)
  set(OMP_ENABLED 1)
else()
  find_path(OMP_INCLUDE_DIR NAMES omp.h)
  find_library(OpenMP_CXX_LIBRARIES NAMES omp)

  string(FIND "${OMP_INCLUDE_DIR}" "-NOTFOUND" notFoundPosition)

  if(NOT(notFoundPosition EQUAL -1))
    set(OMP_INCLUDE_DIR "")
  endif()

  if(OMP_INCLUDE_DIR AND OpenMP_CXX_LIBRARIES)
    set(OMP_ENABLED 1)
  endif()
endif()

if(OMP_ENABLED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${OpenMP_CXX_FLAGS}")

  message("OMP_INCLUDE_DIR: ${OMP_INCLUDE_DIR}")
  message("OpenMP_CXX_LIBRARIES: ${OpenMP_CXX_LIBRARIES}")
  message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
  message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  message("CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")

  include_directories(${OMP_INCLUDE_DIR})
  ADD_DEFINITIONS(-DOMP_ENABLED)
else()
  message("NOTE: OpenMP will not be supported!")
endif()
###########################################################################

include(${TAN_ROOT}/utils/cmake/test_OpenCL.cmake)
include(${TAN_ROOT}/utils/cmake/test_amf.cmake)

#cl kernel preprocessor
include(${TAN_ROOT}/utils/cmake/CLKernelPreprocessor.cmake)

# name
project(TrueAudioNext VERSION 1.0.0 DESCRIPTION "TrueAudioNext")

include_directories(${AMF_HOME}/amf)
include_directories(${TAN_HEADERS})
include_directories(${TAN_ROOT}/tan/tanlibrary/src/clFFT-master/src/include)
include_directories(${TAN_ROOT}/tan/tanlibrary/src/Graal)
include_directories(${TAN_ROOT}/utils/common)

if(OpenMP_FOUND)
  include_directories(${OMP_INCLUDE_DIR})
endif()

#
ADD_DEFINITIONS(-DLEGACY_COPYING=1)
ADD_DEFINITIONS(-DCONVOLUTION_EXPORTS)
ADD_DEFINITIONS(-DTRUEAUDIONEXT_EXPORTS)
ADD_DEFINITIONS(-DTAN_SDK_EXPORTS)
#ADD_DEFINITIONS(-D_M_X64) is it needed?
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)
ADD_DEFINITIONS(-DNMC_VECTORIZE_TARGET=AVX2)

# sources
set(
  SOURCE_LIB

  ${TAN_ROOT}/utils/common/OCLHelper.cpp

  ${TAN_ROOT}/utils/common/StringUtility.cpp
  ${TAN_ROOT}/utils/common/FileUtility.cpp
  ${TAN_ROOT}/utils/common/cpucaps.cpp

  ../../../src/TrueAudioNext/converter/ConverterImpl.cpp
  ../../../src/TrueAudioNext/convolution/ConvolutionImpl.cpp
  ../../../src/TrueAudioNext/core/TANContextImpl.cpp
  ../../../src/TrueAudioNext/core/TANTraceAndDebug.cpp
  ../../../src/TrueAudioNext/fft/FFTImpl.cpp
  ../../../src/TrueAudioNext/filter/FilterImpl.cpp
  ../../../src/TrueAudioNext/IIRfilter/IIRfilterImpl.cpp
  ../../../src/TrueAudioNext/math/MathImpl.cpp
  ../../../src/TrueAudioNext/mixer/MixerImpl.cpp
  )

if(WIN32)
  list(APPEND SOURCE_LIB ../../../src/TrueAudioNext/core/windows/dll.cpp)
else()
  list(APPEND SOURCE_LIB ../../../src/TrueAudioNext/core/linux/dll.cpp)
endif()

set(
  HEADER_LIB

  ${TAN_HEADERS}/TrueAudioNext.h

  ${TAN_ROOT}/utils/common/OCLHelper.h
  ${TAN_ROOT}/utils/common/FileUtility.h

  ../../../src/TrueAudioNext/converter/ConverterImpl.h
  #../../../src/TrueAudioNext/convolution/CLKernel_ConvolutionTD.h
  ../../../src/TrueAudioNext/convolution/ConvolutionImpl.h
  ../../../src/TrueAudioNext/core/TANContextImpl.h
  ../../../src/TrueAudioNext/core/TANTraceAndDebug.h
  ../../../src/TrueAudioNext/fft/FFTImpl.h
  ../../../src/TrueAudioNext/filter/FilterImpl.h
  ../../../src/TrueAudioNext/IIRfilter/IIRfilterImpl.h
  ../../../src/TrueAudioNext/math/MathImpl.h
  ../../../src/TrueAudioNext/mixer/MixerImpl.h
  ../../../src/TrueAudioNext/resource.h
  )

# cl kernels for compilation
set(
  Tan_CL_Directories
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/convolution"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/convolution"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/convolution"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/mixer"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/math"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/math"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/math"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/math"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/converter"
  "${TAN_ROOT}/tan/tanlibrary/src/TrueAudioNext/IIRfilter"
  )

if(ENABLE_METAL)
  set(
    Tan_CL_Files
    "Crossfading.metal"
    "TimeDomainConvolution.metal"
    "SimpleCopy.metal"
    "Mixer.metal"
    "VectorComplexSum.metal"
    "VectorComplexMultiply.metal"
    "VectorComplexDivision.metal"
    "VectorComplexMultiplyAccumulate.metal"
    "Converter.metal"
    "IIRfilter.metal"
    )
else()
  set(
    Tan_CL_Files
    "Crossfading.cl"
    "TimeDomainConvolution.cl"
    "SimpleCopy.cl"
    "Mixer.cl"
    "VectorComplexSum.cl"
    "VectorComplexMultiply.cl"
    "VectorComplexDivision.cl"
    "VectorComplexMultiplyAccumulate.cl"
    "Converter.cl"
    "IIRfilter.cl"
  )
endif()

if(ENABLE_METAL)
  set(
    Tan_CL_Output
    "MetalKernel_Crossfading.h"
    "MetalKernel_TimeDomainConvolution.h"
    "MetalKernel_SimpleCopy.h"
    "MetalKernel_Mixer.h"
    "MetalKernel_VectorComplexSum.h"
    "MetalKernel_VectorComplexMultiply.h"
    "MetalKernel_VectorComplexDivision.h"
    "MetalKernel_VectorComplexMultiplyAccumulate.h"
    "MetalKernel_Converter.h"
    "MetalKernel_IIRfilter.h"
  )
else()
  set(
    Tan_CL_Output
    "CLKernel_Crossfading.h"
    "CLKernel_TimeDomainConvolution.h"
    "CLKernel_SimpleCopy.h"
    "CLKernel_Mixer.h"
    "CLKernel_VectorComplexSum.h"
    "CLKernel_VectorComplexMultiply.h"
    "CLKernel_VectorComplexDivision.h"
    "CLKernel_VectorComplexMultiplyAccumulate.h"
    "CLKernel_Converter.h"
    "CLKernel_IIRfilter.h"
    )
endif()

markGenerated(
  Tan_CL_Directories
  Tan_CL_Output
  OutHeaders
  )

#append generated headers
list(LENGTH OutHeaders OutHeadersCount)
math(EXPR OutHeaders_MaxIndex ${OutHeadersCount}-1)

foreach(FileIndex RANGE ${OutHeaders_MaxIndex})
  list(GET OutHeaders ${FileIndex} OutFile)
  list(APPEND HEADER_LIB ${OutFile})
endforeach()

# create library
add_library(
  TrueAudioNext
  SHARED
  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

#add_dependencies(TrueAudioNext thirdparty_fftw)
#add_dependencies(TrueAudioNext Graal)
#add_dependencies(TrueAudioNext clFFT-master)

generateCLKernel(
  TrueAudioNext
  Tan_CL_Directories
  Tan_CL_Files
  Tan_CL_Output
  )

if(FFTW_DIR)
  set(CacheFileName "FFTW_TAN_WISDOM.cache")
  set(MaxFFTPower "20")
  set(ForceRebuild "0")

  add_custom_command(
    TARGET TrueAudioNext
    POST_BUILD
    COMMAND
    FFTWCache ${CacheFileName} ${MaxFFTPower} ${ForceRebuild}
    COMMENT
    "Building FFTW cache file ${CacheFileName} for max ${MaxFFTPower} power..."
    VERBATIM
    )
endif()

# linking
target_link_libraries(TrueAudioNext ${OpenCL_LIBRARY})

if(AMF_CORE_STATIC)
  target_link_libraries(TrueAudioNext ${AMF_NAME})
else()
  target_link_libraries(TrueAudioNext ${AMF_LOADER_NAME})
endif()

target_link_libraries(TrueAudioNext Graal)
target_link_libraries(TrueAudioNext clFFT-master)

####################################################################################
#OpenMP integration, part 2
####################################################################################
if(OMP_ENABLED)
  message("TrueAudioNext will be linked with ${OpenMP_CXX_LIBRARIES}")
  target_link_libraries(TrueAudioNext ${OpenMP_CXX_LIBRARIES})
endif()

if(FFTW_DIR)
  target_link_libraries(TrueAudioNext fftw3f)
endif()

# Declare public API of your library.
# This API will be installed for third-party application.
# It is a good practice to isolate it in your project tree (like placing it include/ directory).
# Notice that, private headers should not been installed and it is strongly suggested to place them with sources files.
set_target_properties(
  TrueAudioNext
  PROPERTIES
  PUBLIC_HEADER
  ${TAN_HEADERS}/TrueAudioNext.h
  )

if(NOT WIN32)
  target_compile_options(TrueAudioNext PUBLIC -mavx2)
  target_compile_options(TrueAudioNext PUBLIC -mfma)
  target_compile_options(TrueAudioNext PUBLIC -msse4.2)

  if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    target_compile_options(TrueAudioNext PUBLIC -g)
  endif()

  if(NOT APPLE)
    target_compile_options(TrueAudioNext PUBLIC -Wpsabi)
  endif()
endif()