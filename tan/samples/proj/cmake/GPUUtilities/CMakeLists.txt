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
project(GPUUtilities DESCRIPTION "GPUUtilities")

#
#if(COMPILER_MSVC)
  add_compile_definitions(_USRDLL)
  add_compile_definitions(GPUUTILITIES_EXPORTS)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

# sources
set(
  SOURCE_LIB
  ../../../../../AMF/public/common/AMFFactory.cpp
  ../../../../../AMF/public/common/AMFSTL.cpp
  ../../../../../AMF/public/common/PropertyStorageExImpl.cpp
  ../../../../../AMF/public/common/Thread.cpp
  ../../../../../AMF/public/common/TraceAdapter.cpp
  ../../../../../AMF/public/common/Windows/ThreadWindows.cpp
  ../../../src/ADL/ADLquery.cpp
  ../../../src/GPUUtilities/dllmain.cpp
  ../../../src/GPUUtilities/GPUUtilities.cpp
  )

set(
  HEADER_LIB
  ../../../../../AMF/public/common/AMFSTL.h
  ../../../../../AMF/public/common/InterfaceImpl.h
  ../../../../../AMF/public/common/ObservableImpl.h
  ../../../../../AMF/public/common/PropertyStorageExImpl.h
  ../../../../../AMF/public/common/PropertyStorageImpl.h
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