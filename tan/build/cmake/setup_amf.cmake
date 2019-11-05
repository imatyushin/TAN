cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

get_property(AMF_HOME GLOBAL PROPERTY AMF_HOME)

if(NOT AMF_HOME)
  if(AMF_OPEN)
    message("Open AMF will be used")
    message("Open AMF path: " ${AMF_OPEN})
    
    get_filename_component(amfPath ${AMF_OPEN} REALPATH)
    message("AMF_HOME will be set to " ${amfPath})

    set_property(GLOBAL PROPERTY AMF_HOME ${amfPath})
    set_property(GLOBAL PROPERTY AMF_NAME open_amf)

    #include(${AMF_OPEN}/public/proj/cmake/open_amf.cmake)
    #add_subdirectory(${AMF_OPEN}/public/proj/cmake cmake-open-amf-bin)

    add_subdirectory(./../../../amfOpen/amf/public/proj/cmake cmake-open-amf-bin)
  else()
    message("Proprietary AMF will be used")
    message("Proprietary AMF path: " ${TAN_ROOT}/amf)

    set_property(GLOBAL PROPERTY AMF_HOME ${TAN_ROOT}/amf)
    set_property(GLOBAL PROPERTY AMF_NAME amf)

    #include(${AMF_OPEN}/public/proj/cmake/open_amf.cmake)
    add_subdirectory(${TAN_ROOT}/tan/tanlibrary/proj/cmake/amf cmake-amf-bin)
  endif()
endif()