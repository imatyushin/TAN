cmake_minimum_required(VERSION 3.10)

# The cmake-policies(7) manual explains that the OLD behaviors of all
# policies are deprecated and that a policy should be set to OLD only under
# specific short-term circumstances.  Projects should be ported to the NEW
# behavior and not rely on setting a policy to OLD.

# VERSION not allowed unless CMP0048 is set to NEW
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

ADD_DEFINITIONS(-DUSE_TANWAVCOMPARE)

# name
project(TANWavCompare DESCRIPTION "TANWavCompare - count the correlation between wav files.")

include_directories(../../../common)

# sources
set(
    SOURCE_EXE
    ../../../src/TANWavCompare/TANWavCompare.cpp

    ${TAN_ROOT}/utils/common/wav.cpp
    ${TAN_ROOT}/utils/common/FileUtility.cpp
)

set(
    HEADER_EXE
    ../../../src/TANWavCompare/TANWavCompare.h

    ${TAN_ROOT}/utils/common/wav.h
    ${TAN_ROOT}/utils/common/FileUtility.h
)

# create binary
add_executable(
    TANWavCompare
    ${SOURCE_EXE}
    ${HEADER_EXE}
)

if(AMF_CORE_STATIC)
  target_link_libraries(TANWavCompare ${AMF_NAME})
else()
  target_link_libraries(TANWavCompare ${AMF_LOADER_NAME})
endif()

#copy binaries to autotest unified directory
set(auto_test_path ${TAN_ROOT}/bin/$<PLATFORM_ID>/$<CONFIGURATION>)
add_custom_command(
    TARGET TANWavCompare
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory $<TARGET_FILE_DIR:TANWavCompare> "${auto_test_path}"
    VERBATIM
    )
