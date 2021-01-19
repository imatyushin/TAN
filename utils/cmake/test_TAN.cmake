cmake_minimum_required(VERSION 3.10)

enable_language(CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

if(NOT TAN_ROOT)
  message("")
  message(SEND_ERROR "Error: TAN_ROOT are not set for ${CMAKE_CURRENT_SOURCE_DIR}!")
  return()
endif()