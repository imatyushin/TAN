cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

get_property(AMF_HOME GLOBAL PROPERTY AMF_HOME)
if(NOT AMF_HOME)
  message("")
  message(SEND_ERROR "Error: AMF_HOME are not set for ${CMAKE_CURRENT_SOURCE_DIR}!")
  return()
endif()

get_property(AMF_NAME GLOBAL PROPERTY AMF_NAME)
if(NOT AMF_NAME)
  message("")
  message(SEND_ERROR "Error: AMF_NAME not configured in ${CMAKE_CURRENT_SOURCE_DIR}!")
  return()
endif()

if(NOT AMF_CORE_STATIC)
  get_property(AMF_LOADER_NAME GLOBAL PROPERTY AMF_LOADER_NAME)
  if(NOT AMF_LOADER_NAME)
    message("")
    message(SEND_ERROR "Error: AMF_LOADER_NAME not configured in ${CMAKE_CURRENT_SOURCE_DIR}!")
    return()
  endif()
endif()

message("AMF configured successfully")