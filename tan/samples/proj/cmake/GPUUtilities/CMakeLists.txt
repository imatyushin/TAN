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

find_package(OpenCL REQUIRED)
if(NOT OpenCL_FOUND)
	message(SEND_ERROR "Failed to find OpenCL")
	return()
else()
	include_directories(${OpenCL_INCLUDE_DIRS})
  link_directories(${OpenCL_LIBRARY})
endif()

# name
project(GPUUtilities DESCRIPTION "GPUUtilities")

#
#if(COMPILER_MSVC)
  ADD_DEFINITIONS(-D_USRDLL)
  ADD_DEFINITIONS(-DGPUUTILITIES_EXPORTS)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

# sources
set(
  SOURCE_LIB
  ../../../../../amf/public/common/AMFFactory.cpp
  ../../../../../amf/public/common/AMFSTL.cpp
  ../../../../../amf/public/common/PropertyStorageExImpl.cpp
  ../../../../../amf/public/common/Thread.cpp
  ../../../../../amf/public/common/TraceAdapter.cpp
  ../../../../../amf/public/common/Windows/ThreadWindows.cpp
  ../../../src/ADL/ADLquery.cpp
  ../../../src/GPUUtilities/dllmain.cpp
  ../../../src/GPUUtilities/GPUUtilities.cpp
  )

set(
  HEADER_LIB
  ../../../../../amf/public/common/AMFSTL.h
  ../../../../../amf/public/common/InterfaceImpl.h
  ../../../../../amf/public/common/ObservableImpl.h
  ../../../../../amf/public/common/PropertyStorageExImpl.h
  ../../../../../amf/public/common/PropertyStorageImpl.h
  ../../../../../amf/public/common/TraceAdapter.h
  ../../../../../amf/public/include/core/AudioBuffer.h
  ../../../../../amf/public/include/core/Buffer.h
  ../../../../../amf/public/include/core/Compute.h
  ../../../../../amf/public/include/core/ComputeFactory.h
  ../../../../../amf/public/include/core/Context.h
  ../../../../../amf/public/include/core/Data.h
  ../../../../../amf/public/include/core/Debug.h
  ../../../../../amf/public/include/core/Factory.h
  ../../../../../amf/public/include/core/Interface.h
  ../../../../../amf/public/include/core/Plane.h
  ../../../../../amf/public/include/core/Platform.h
  ../../../../../amf/public/include/core/PropertyStorage.h
  ../../../../../amf/public/include/core/PropertyStorageEx.h
  ../../../../../amf/public/include/core/Result.h
  ../../../../../amf/public/include/core/Surface.h
  ../../../../../amf/public/include/core/Trace.h
  ../../../../../amf/public/include/core/Variant.h
  ../../../../../amf/public/include/core/Version.h
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
target_link_libraries(GPUUtilities ${OpenCL_LIBRARY})