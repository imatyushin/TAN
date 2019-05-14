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

include_directories(../../../../../amf)
include_directories(../../../../..)

# library name
project(clFFT-master)

include_directories(../../../src/clFFT-master/src/include)

#if(COMPILER_MSVC)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

# sources
set(
  SOURCE_LIB
  ../../../src/clFFT-master/src/library/accessors.cpp
  ../../../src/clFFT-master/src/library/action.transpose.cpp
  ../../../src/clFFT-master/src/library/dllmain.cpp
  ../../../src/clFFT-master/src/library/enqueue.cpp
  ../../../src/clFFT-master/src/library/fft_binary_lookup.cpp

  ../../../src/clFFT-master/src/library/generator.copy.cpp
  ../../../src/clFFT-master/src/library/generator.stockham.cpp
  ../../../src/clFFT-master/src/library/generator.transpose.cpp

  ../../../src/clFFT-master/src/library/generator.transpose.gcn.cpp
  ../../../src/clFFT-master/src/library/lifetime.cpp
  ../../../src/clFFT-master/src/library/md5sum.c

  ../../../src/clFFT-master/src/library/plan.cpp
  ../../../src/clFFT-master/src/library/repo.cpp
  ../../../src/clFFT-master/src/library/stdafx.cpp

  ../../../src/clFFT-master/src/library/transform.cpp
)

set(
  HEADER_LIB
  ../../../src/clFFT-master/src/library/action.h
  ../../../src/clFFT-master/src/library/action.transpose.h
  ../../../src/clFFT-master/src/library/fft_binary_lookup.h
  ../../../src/clFFT-master/src/library/generator.h
  ../../../src/clFFT-master/src/library/generator.stockham.h
  ../../../src/clFFT-master/src/library/generator.transpose.gcn.h
  ../../../src/clFFT-master/src/library/generator.transpose.h
  ../../../src/clFFT-master/src/library/lock.h
  ../../../src/clFFT-master/src/library/mainpage.h
  ../../../src/clFFT-master/src/library/md5sum.h
  ../../../src/clFFT-master/src/library/plan.h
  ../../../src/clFFT-master/src/library/private.h
  ../../../src/clFFT-master/src/library/repo.h

  ../../../src/clFFT-master/src/library/clFFT.pc.in
  )


# declare static library creation
add_library(
  clFFT-master
  STATIC
  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

set_property(TARGET clFFT-master PROPERTY POSITION_INDEPENDENT_CODE ON)

if(!WIN32)
target_compile_options(clFFT-master PUBLIC -mavx2)
target_compile_options(clFFT-master PUBLIC -mfma)
target_compile_options(clFFT-master PUBLIC -msse4.2)
target_compile_options(clFFT-master PUBLIC -Wpsabi)
endif()