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

  set_property(GLOBAL PROPERTY FFTW_SOURCES_PATH ${Path_To_FFTW})
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
      #BINARY_DIR        cmake-fftw-bin
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
  #add_subdirectory(${Path_To_FFTW} cmake-fftw-bin EXCLUDE_FROM_ALL)
  add_subdirectory(${Path_To_FFTW} ${CMAKE_BUILD_TYPE})

  message("")
  message("FFTW library name: ${fftw3_lib}")
  message("")

  ADD_DEFINITIONS(-DUSE_FFTW)
  include_directories(${Path_To_FFTW})
endif()