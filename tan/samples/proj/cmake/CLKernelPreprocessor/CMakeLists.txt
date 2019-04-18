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

# name
project(CLKernelPreprocessor DESCRIPTION "CLKernelPreprocessor")

#
#if(COMPILER_MSVC)
  add_compile_definitions(_CONSOLE)
  add_compile_definitions(_LIB)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

# sources
set(
  SOURCE_EXE
  ../../../src/CLKernelPreprocessor/CLKernelPreprocessor.cpp
  ../../../src/CLKernelPreprocessor/stdafx.cpp
  )

set(
  HEADER_EXE
  ../../../src/CLKernelPreprocessor/stdafx.h
  ../../../src/CLKernelPreprocessor/targetver.h
  )

# create binary
add_executable(
  CLKernelPreprocessor
  ${SOURCE_EXE}
  ${HEADER_EXE}
  )

#if ( MSVC )
#  #etc for the other available configuration types (MinSizeRel, RelWithDebInfo)
#  set_target_properties(CLKernelPreprocessor PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
#  set_target_properties(CLKernelPreprocessor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_BINARY_DIR})
#  set_target_properties(CLKernelPreprocessor PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_BINARY_DIR})
#endif ( MSVC )