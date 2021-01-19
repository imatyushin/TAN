cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

get_property(OpenCL_FOUND GLOBAL PROPERTY OpenCL_FOUND)
get_property(OpenCL_INCLUDE_DIR GLOBAL PROPERTY OpenCL_INCLUDE_DIR)
get_property(OpenCL_LIBRARY GLOBAL PROPERTY OpenCL_LIBRARY)

if(NOT OpenCL_FOUND)
  message("")
  message(SEND_ERROR "Error: OpenCL not configured")
  return()
endif()

message("")
message("OpenCL found, the following pathes will be used for ${CMAKE_CURRENT_SOURCE_DIR}:")
message("OpenCL include dir: ${OpenCL_INCLUDE_DIR}")
message("OpenCL linking library: ${OpenCL_LIBRARY}")
message("")

include_directories(${OpenCL_INCLUDE_DIR})
link_directories(${OpenCL_LIBRARY})
ADD_DEFINITIONS(-DCL_TARGET_OPENCL_VERSION=120)

if(DEFINE_AMD_OPENCL_EXTENSION)
  ADD_DEFINITIONS(-DDEFINE_AMD_OPENCL_EXTENSION)
endif()