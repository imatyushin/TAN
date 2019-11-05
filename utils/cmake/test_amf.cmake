cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

get_property(AMF_HOME GLOBAL PROPERTY AMF_HOME)

if(NOT AMF_HOME)
  message("")
  message(SEND_ERROR "Error: AMF not configured!")
  return()
endif()

message("AMF configured")