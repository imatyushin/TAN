cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

get_property(AMF_HOME GLOBAL PROPERTY AMF_HOME)

if(NOT AMF_NAME)
  if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8")
    set_property(GLOBAL PROPERTY AMF_NAME amfrt64)
  else()
    set_property(GLOBAL PROPERTY AMF_NAME amfrt32)
  endif()
  get_property(AMF_NAME GLOBAL PROPERTY AMF_NAME)
endif()

#static library to load AMF
if(NOT AMF_CORE_STATIC)
  set_property(GLOBAL PROPERTY AMF_LOADER_NAME ${AMF_NAME}_loader)
  get_property(AMF_LOADER_NAME GLOBAL PROPERTY AMF_LOADER_NAME)
endif()

if(NOT AMF_HOME)
  if(AMF_OPEN_DIR)
    message("OpenAMF will be used")
    message("OpenAMF path: " ${AMF_OPEN_DIR})

    get_filename_component(AMF_HOME ${AMF_OPEN_DIR} REALPATH)

    message("AMF_HOME will be set to " ${AMF_HOME})
    set_property(GLOBAL PROPERTY AMF_HOME ${AMF_HOME})



    add_subdirectory(${AMF_HOME}/amf/public/proj/cmake cmake-open-amf-bin)
  else()
    message("Proprietary AMF will be used")
    message("Proprietary AMF path: " ${TAN_ROOT}/amf)

    set_property(GLOBAL PROPERTY AMF_HOME ${TAN_ROOT}/amf)
    set_property(GLOBAL PROPERTY AMF_NAME amf)

    #static library to load AMF
    if(NOT AMF_CORE_STATIC)
      set_property(GLOBAL PROPERTY AMF_LOADER_NAME amf_loader)
      get_property(AMF_LOADER_NAME GLOBAL PROPERTY AMF_LOADER_NAME)
    endif()

    add_subdirectory(${TAN_ROOT}/tan/tanlibrary/proj/cmake/amf cmake-amf-bin)
  endif()
endif()