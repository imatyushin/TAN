cmake_minimum_required(VERSION 3.10)

# The cmake-policies(7) manual explains that the OLD behaviors of all
# policies are deprecated and that a policy should be set to OLD only under
# specific short-term circumstances.  Projects should be ported to the NEW
# behavior and not rely on setting a policy to OLD.

# VERSION not allowed unless CMP0048 is set to NEW
if(POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

# name
project(TAN DESCRIPTION "TAN")

get_filename_component(ABSOLUTE_PATH "${PROJECT_SOURCE_DIR}/../../../" ABSOLUTE)
set(TAN_ROOT ${ABSOLUTE_PATH})
message("TAN_ROOT: ${TAN_ROOT}")

#examples
add_subdirectory(../../samples/proj/cmake/GPUUtilities cmake-GPUUtilities-bin)
add_subdirectory(../../samples/proj/cmake/Ambisonic1stOrderDecoder cmake-Ambisonic1stOrderDecoder-bin)
add_subdirectory(../../samples/proj/cmake/Ambisonic1stOrderEncoder cmake-Ambisonic1stOrderEncoder-bin)
add_subdirectory(../../samples/proj/cmake/Ambisonic2ndOrderEncoder cmake-Ambisonic2ndOrderEncoder-bin)
add_subdirectory(../../samples/proj/cmake/AmbisonicVRDecoder cmake-AmbisonicVRDecoder-bin)
add_subdirectory(../../samples/proj/cmake/CLKernelPreprocessor cmake-CLKernelPreprocessor-bin)
add_subdirectory(../../samples/proj/cmake/FileToHeader cmake-FileToHeader-bin)
add_subdirectory(../../samples/proj/cmake/TrueAudioVR cmake-TrueAudioVR-bin)
add_subdirectory(../../samples/proj/cmake/RoomAcousticQT cmake-RoomAcousticQT-bin)

add_subdirectory(../../samples/proj/cmake/ReverbMixer cmake-ReverbMixer-bin)

if(WIN32)
  #add_subdirectory(../../samples/proj/cmake/OculusRoomTAN cmake-OculusRoomTAN-bin)
  add_subdirectory(../../samples/proj/cmake/RoomAcoustics cmake-RoomAcoustics-bin)
endif()

#libraries
add_subdirectory(../../tanlibrary/proj/cmake/amf cmake-amf-bin)
add_subdirectory(../../tanlibrary/proj/cmake/Graal cmake-Graal-bin)
add_subdirectory(../../tanlibrary/proj/cmake/clFFT cmake-clFFT-bin)
add_subdirectory(../../tanlibrary/proj/cmake/TAN cmake-TAN-bin)

#tests
add_subdirectory(../../tests/proj/cmake/TALibDopplerTest cmake-TALibDopplerTest-bin)
add_subdirectory(../../tests/proj/cmake/TALibTestConvolution cmake-TALibTestConvolution-bin)
add_subdirectory(../../tests/proj/cmake/TALibTestDynamicChannelConvolution cmake-TALibTestDynamicChannelConvolution-bin)
#add_subdirectory(../../tests/proj/cmake/TALibVRTest cmake-TALibVRTest-bin)
#add_subdirectory(../../tests/proj/cmake/TanDeviceResourcesTest cmake-TanDeviceResourcesTest-bin)
