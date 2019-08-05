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

#set(CMAKE_C_STANDARD 11)
#set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

find_package(OpenCL REQUIRED)
if(NOT OpenCL_FOUND)
	message(SEND_ERROR "Failed to find OpenCL")
	return()
else()
  message("OpenCL found, headers path: " ${OpenCL_INCLUDE_DIRS} ", runtime library: " ${OpenCL_LIBRARY})
	include_directories(${OpenCL_INCLUDE_DIRS})
  link_directories(${OpenCL_LIBRARY})
endif()

# name
project(GPUUtilities DESCRIPTION "GPUUtilities")

#
ADD_DEFINITIONS(-D_USRDLL)
ADD_DEFINITIONS(-DGPUUTILITIES_EXPORTS)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)
ADD_DEFINITIONS(-DTAN_SDK_EXPORTS)

if(NOT WIN32)
ADD_DEFINITIONS(-DLINUX)
endif()

# sources
set(
  SOURCE_LIB

  ../../../src/ADL/ADLquery.cpp
  ../../../src/GPUUtilities/GPUUtilities.cpp
  )

if(WIN32)
  list(APPEND SOURCE_LIB ../../../src/GPUUtilities/dllmain.cpp)
endif()

set(
  HEADER_LIB

  ../../../src/ADL/ADLQuery.h
  ../../../src/GPUUtilities/GpuUtilities.h
  ../../../src/GPUUtilities/stdafx.h
  ../../../src/GPUUtilities/targetver.h
  )

# create library
add_library(
  GPUUtilities
  SHARED
  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

# linking
target_link_libraries(GPUUtilities amf)
target_link_libraries(GPUUtilities ${OpenCL_LIBRARY})

target_compile_options(GPUUtilities PUBLIC -g) #todo: to Debug only