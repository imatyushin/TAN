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

if(0)
find_package(OpenMP)
if(OpenMP_FOUND)
  message("OpenMP package found... ${OpenMP_C} ${OpenMP_CXX}")
endif()

if(NOT OMP_INCLUDE_DIR OR NOT OpenMP_LIBRARY)
  find_path(OMP_INCLUDE_DIR NAMES omp.h)
  find_library(OpenMP_LIBRARY NAMES omp)
endif()

if(OMP_INCLUDE_DIR AND OpenMP_LIBRARY)
  set(OpenMP_FOUND 1)
  ADD_DEFINITIONS(-DOMP_ENABLED)
  message("OMP_INCLUDE_DIR: ${OMP_INCLUDE_DIR}")
  message("OpenMP_LIBRARY: ${OpenMP_LIBRARY}")

  if(OpenMP_C OR OpenMP_CXX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} ${OpenMP_C}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C}")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${OpenMP_CXX}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX}")
  else()
    message("OpenMP flags not set, try to set them manually...")

    if(NOT WIN32)
      if(APPLE OR XCODE)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Xclang -fopenmp -lomp")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Xclang -fopenmp -lomp")
        #set(CMAKE_SHARED_LINKER_FLAGS " -lomp")
      else()
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fopenmp")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")
      endif()
    endif()

    message("CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
    message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  endif()
endif()
endif()

####################################################################################
message("")
message("Start OpenMP search...")
find_package(OpenMP)

if(OpenMP_FOUND AND OMP_INCLUDE_DIR AND OpenMP_CXX_LIBRARIES)
  ADD_DEFINITIONS(-DOMP_ENABLED)
else()
  find_path(OMP_INCLUDE_DIR NAMES omp.h)
  find_library(OpenMP_CXX_LIBRARIES NAMES omp)

  string(FIND "${OMP_INCLUDE_DIR}" "-NOTFOUND" notFoundPosition)

  if(NOT(notFoundPosition EQUAL -1))
    set(OMP_INCLUDE_DIR "")
  endif()

  if(OpenMP_CXX_LIBRARIES)
    set(OpenMP_FOUND 1)
  endif()
endif()

if(OpenMP_FOUND)
  message("OMP_INCLUDE_DIR: ${OMP_INCLUDE_DIR}")
  message("OpenMP_CXX_LIBRARIES: ${OpenMP_CXX_LIBRARIES}")
  message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
  message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  message("CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${OpenMP_CXX_FLAGS}")

  include_directories(${OMP_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Error: OpenMP not found!")
endif()

#set(OpenMP_CXX_LIB_NAMES "omp")
#set(CMAKE_EXE_LINKER_FLAGS "omp")

#set(CMAKE_C_COMPILER clang-omp CACHE STRING "C compiler" FORCE)
#set(CMAKE_CXX_COMPILER clang-omp++ CACHE STRING "C++ compiler" FORCE)

#find_package(OpenMP REQUIRED)

#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")

message("CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}")
message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")

#OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp -lomp -I/usr/local/opt/libomp/include"
#OpenMP_CXX_LIB_NAMES "omp"
#OpenMP_omp_LIBRARY /usr/local/opt/libomp/lib/libomp.dylib
####################################################################################
#   /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/c++  -Xclang -fopenmp -g -D_DEBUG -Xclang -fopenmp -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk -mmacosx-version-min=10.15 -dynamiclib -Wl,-headerpad_max_install_names -o libTrueAudioNext.dylib -install_name @rpath/libTrueAudioNext.dylib CMakeFiles/TrueAudioNext.dir/Users/user/TAN/utils/common/OCLHelper.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/utils/common/StringUtility.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/utils/common/FileUtility.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/utils/common/cpucaps.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/converter/ConverterImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/convolution/ConvolutionImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/core/TANContextImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/core/TANTraceAndDebug.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/fft/FFTImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/filter/FilterImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/IIRfilter/IIRfilterImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/math/MathImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/mixer/MixerImpl.cpp.o CMakeFiles/TrueAudioNext.dir/Users/user/TAN/tan/tanlibrary/src/TrueAudioNext/core/linux/dll.cpp.o   -L/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk/System/Library/Frameworks/OpenCL.framework  -Wl,-rpath,/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX11.1.sdk/System/Library/Frameworks/OpenCL.framework -Wl,-rpath,/Users/user/TAN/tan/build/cmake/mac-gnumake-cl/cmake-TrueAudioNext-bin/cmake-fftw-bin -framework OpenCL ../cmake-open-amf-bin/libamfrt64.a ../cmake-Graal-bin/libGraal.a ../cmake-clFFT-bin/libclFFT-master.a cmake-fftw-bin/libfftw3f.3.dylib -lm
if(FFTW_DIR)
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

  #INCLUDE(ExternalProject)
  #ExternalProject_Add(
  #  thirdparty_fftw
  #  SOURCE_DIR          ${Path_To_FFTW}
  #  #GIT_REPOSITORY     ""
  #  PREFIX              thirdparty_fftw
  #  DOWNLOAD_DIR        ${TAN_ROOT}/downloads
  #  URL                 http://fftw.org/fftw-3.3.9.tar.gz
  #  URL_MD5             50145bb68a8510b5d77605f11cadf8dc
  #  #UPDATE_COMMAND      ""
  #  #CONFIGURE_COMMAND   ${Path_To_FFTW}/configure
  #  #BUILD_IN_SOURCE     0
  #  #BUILD_COMMAND       make
  #  #INSTALL_COMMAND     "" #sudo make install
  #  CMAKE_ARGS          "${CMAKE_ARGS};-DENABLE_FLOAT=1"
  #  )

  INCLUDE(FetchContent) #requires cmake 3.11
  FetchContent_Declare(
    fftw_sources
    URL                 http://fftw.org/fftw-3.3.9.tar.gz
    URL_MD5             50145bb68a8510b5d77605f11cadf8dc
  )

  FetchContent_GetProperties(fftw_sources)
  if(NOT fftw_sources_POPULATED)
    FetchContent_Populate(
      fftw_sources
      DOWNLOAD_DIR      ${TAN_ROOT}/downloads
      URL               http://fftw.org/fftw-3.3.9.tar.gz
      URL_MD5           50145bb68a8510b5d77605f11cadf8dc
      SOURCE_DIR        ${Path_To_FFTW}
      BINARY_DIR        cmake-fftw-bin
      )
  endif()

  #option(BUILD_SHARED_LIBS "" OFF)
  option(BUILD_TESTS "" OFF)
  #option(ENABLE_OPENMP "" ON)
  #option(ENABLE_THREADS "" ON)
  option(ENABLE_FLOAT "" ON)
  option(ENABLE_SSE "" ON)
  option(ENABLE_SSE2 "" ON)
  option(ENABLE_AVX "" ON)
  option(ENABLE_AVX2 "" ON)
  option(DISABLE_FORTRAN "" ON)
  add_subdirectory(${Path_To_FFTW} cmake-fftw-bin EXCLUDE_FROM_ALL)
  add_subdirectory(${TAN_ROOT}/utils/proj/cmake/FFTWCache cmake-FFTWCache-bin EXCLUDE_FROM_ALL)

  message("")
  message("FFTW library name: ${fftw3_lib}")
  message("")

  ADD_DEFINITIONS(-DUSE_FFTW)
  include_directories(${Path_To_FFTW})
endif()

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
#if(FFTW_DIR)
#  add_dependencies(TrueAudioNext FFTWCache)
#endif()

generateCLKernel(
  TrueAudioNext
  Tan_CL_Directories
  Tan_CL_Files
  Tan_CL_Output
  )

set(CacheFileName "FFTW_TAN_WISDOM.cache")
set(MaxFFTPower "20")
set(ForceRebuild "0")

if(FFTW_DIR)
  set(FFTW_BIN_DIR "${TrueAudioNext_BINARY_DIR}/cmake-fftw-bin/$<CONFIGURATION>")
  message("FFTW_BIN_DIR: " ${FFTW_BIN_DIR})

  if(WIN32)
    add_custom_command(
      TARGET TrueAudioNext
      POST_BUILD

      COMMAND ${CMAKE_COMMAND} -E copy_directory "${${AMF_NAME}_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>
      )
  endif()

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

if(OpenMP_FOUND)
  #target_link_libraries(TrueAudioNext ${OpenMP_LIBRARY})
endif()

if(FFTW_DIR)
  target_link_libraries(TrueAudioNext fftw3f)
endif()

target_link_libraries(TrueAudioNext ${OpenMP_CXX_LIBRARIES})

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