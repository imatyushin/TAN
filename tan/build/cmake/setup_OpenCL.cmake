cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

get_property(OpenCL_FOUND GLOBAL PROPERTY OpenCL_FOUND)

if(NOT OpenCL_FOUND)
  message("")
  message("OpenCL_INCLUDE_DIR is set to " ${OpenCL_INCLUDE_DIR})

  #test the ${OpenCL_INCLUDE_DIR} is not an absolute path
  if(NOT IS_ABSOLUTE ${OpenCL_INCLUDE_DIR})
    #test this cmake is included from other project
    if(NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
      get_filename_component(Path_To_OpenCL "${CMAKE_SOURCE_DIR}/${OpenCL_INCLUDE_DIR}" ABSOLUTE)
    #assume that the project is built from itself
    else()
      get_filename_component(Path_To_OpenCL "${CMAKE_CURRENT_SOURCE_DIR}/${OpenCL_INCLUDE_DIR}" ABSOLUTE)
    endif()
  else()
    get_filename_component(Path_To_OpenCL ${OpenCL_INCLUDE_DIR} ABSOLUTE)
  endif()

  message("OpenCL_INCLUDE_DIR expanded: " ${Path_To_OpenCL})
  message("search OpenCL...")

  #search
  find_package(OpenCL REQUIRED)

  if(NOT OpenCL_FOUND)
    message("")
    message(SEND_ERROR "Error: Failed to find OpenCL!")
    return()
  else()
    message("OpenCL found: " ${OpenCL_FOUND})
    message("OpenCL_INCLUDE_DIRS: " ${Path_To_OpenCL})
    message("OpenCL_LIBRARIES: " ${OpenCL_LIBRARIES})
    message("OpenCL_VERSION_STRING: " ${OpenCL_VERSION_STRING})
    message("OpenCL_VERSION_MAJOR: " ${OpenCL_VERSION_MAJOR})
    message("OpenCL_VERSION_MINOR: " ${OpenCL_VERSION_MINOR})

    message("OpenCL_INCLUDE_DIR: ${Path_To_OpenCL}")
    message("OpenCL_LIBRARY: ${OpenCL_LIBRARY}")

    set_property(GLOBAL PROPERTY OpenCL_FOUND 1)
    set_property(GLOBAL PROPERTY OpenCL_INCLUDE_DIR ${Path_To_OpenCL})
    set_property(GLOBAL PROPERTY OpenCL_LIBRARY ${OpenCL_LIBRARY})
  endif()
endif()

get_property(OpenCL_INCLUDE_DIR GLOBAL PROPERTY OpenCL_INCLUDE_DIR)
get_property(OpenCL_LIBRARY GLOBAL PROPERTY OpenCL_LIBRARY)

#message("")
#message("OpenCL found, the following pathes will be used for ${CMAKE_CURRENT_SOURCE_DIR}:")
#message("OpenCL include dir: ${OpenCL_INCLUDE_DIR}")
#message("OpenCL linking library: ${OpenCL_LIBRARY}")
#message("")

include_directories(${OpenCL_INCLUDE_DIR})
link_directories(${OpenCL_LIBRARY})

ADD_DEFINITIONS(-DCL_TARGET_OPENCL_VERSION=120)

if(DEFINE_AMD_OPENCL_EXTENSION)
  ADD_DEFINITIONS(-DDEFINE_AMD_OPENCL_EXTENSION)
endif()