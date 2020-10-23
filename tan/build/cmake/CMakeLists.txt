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
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -D_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
  message("Build type was not set, Debug will be forced")
endif()

if(RTQ_ENABLED)
  if(WIN32)
    message("Realtime queue support will be enabled")
    ADD_DEFINITIONS(-DRTQ_ENABLED)
  else()
    message(WARNING "Warning: realtime queues are only supported for Windows platform")
    #ADD_DEFINITIONS(-DRTQ_ENABLED)
  endif()
endif()

# name
if(TAN_NO_OPENCL)
  if(USE_METAL)
    project(TAN-AMF-METAL DESCRIPTION "TAN-AMF-METAL")
  else()
    project(TAN-AMF-CL DESCRIPTION "TAN-AMF-CL")
  endif()
else()
  project(TAN-CL DESCRIPTION "TAN-CL")
endif()

get_filename_component(ABSOLUTE_PATH "${PROJECT_SOURCE_DIR}/../../../" ABSOLUTE)

set(TAN_ROOT ${ABSOLUTE_PATH})
set(TAN_HEADERS ${TAN_ROOT}/tan/tanlibrary/include)

include(${TAN_ROOT}/utils/cmake/test_TAN.cmake)
message("TAN_ROOT: ${TAN_ROOT}")

#setup opencl
include(setup_OpenCL.cmake)

if(NOT OpenCL_FOUND)
  message(SEND_ERROR "Error: Could not setup OpenCL")
  return()
endif()

if(TAN_NO_OPENCL)
  ADD_DEFINITIONS(-DTAN_NO_OPENCL)
endif()

if(AMF_CORE_STATIC)
  message("static AMF build will be used")
  ADD_DEFINITIONS(-DAMF_CORE_STATIC)
else()
  message("shared AMF build will be used")
endif()

if((APPLE OR XCODE) AND USE_METAL)
  SET(METAL_SUPPORT 1)
  ADD_DEFINITIONS(-DUSE_METAL)
  ADD_DEFINITIONS(-DMETAL_SUPPORT)
  message("Metal support will be enabled for OpenAMF")

  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} \
    -framework AppKit \
    -framework Metal \
    -framework MetalKit"
    )
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} \
    -framework AppKit \
    -framework Metal \
    -framework MetalKit"
    )
endif()

#choose amf library to use
include(setup_amf.cmake)
include(${TAN_ROOT}/utils/cmake/test_amf.cmake)

#utilities
add_subdirectory(${TAN_ROOT}/utils/proj/cmake/CLKernelPreprocessor cmake-CLKernelPreprocessor-bin)
add_subdirectory(${TAN_ROOT}/utils/proj/cmake/FileToHeader cmake-FileToHeader-bin)

#libraries
add_subdirectory(${TAN_ROOT}/tan/tanlibrary/proj/cmake/Graal cmake-Graal-bin)
add_subdirectory(${TAN_ROOT}/tan/tanlibrary/proj/cmake/clFFT cmake-clFFT-bin)
add_subdirectory(${TAN_ROOT}/tan/tanlibrary/proj/cmake/TrueAudioNext cmake-TrueAudioNext-bin)

#examples
add_subdirectory(../../samples/proj/cmake/GPUUtilities cmake-GPUUtilities-bin)
#add_subdirectory(../../samples/proj/cmake/Ambisonic1stOrderDecoder cmake-Ambisonic1stOrderDecoder-bin)
#add_subdirectory(../../samples/proj/cmake/Ambisonic1stOrderEncoder cmake-Ambisonic1stOrderEncoder-bin)
#add_subdirectory(../../samples/proj/cmake/Ambisonic2ndOrderEncoder cmake-Ambisonic2ndOrderEncoder-bin)
#add_subdirectory(../../samples/proj/cmake/AmbisonicVRDecoder cmake-AmbisonicVRDecoder-bin)
add_subdirectory(../../samples/proj/cmake/TrueAudioVR cmake-TrueAudioVR-bin)
add_subdirectory(../../samples/proj/cmake/RoomAcousticQT cmake-RoomAcousticQT-bin)
#add_subdirectory(../../samples/proj/cmake/ReverbMixer cmake-ReverbMixer-bin)
if(WIN32)
  #not implemented yet: add_subdirectory(../../samples/proj/cmake/OculusRoomTAN cmake-OculusRoomTAN-bin)
  #migration to new Audio3D class not yet done: add_subdirectory(../../samples/proj/cmake/RoomAcoustics cmake-RoomAcoustics-bin)
endif()

#tests
#add_subdirectory(../../tests/proj/cmake/TALibDopplerTest cmake-TALibDopplerTest-bin)
#add_subdirectory(../../tests/proj/cmake/TALibTestConvolution cmake-TALibTestConvolution-bin)
#add_subdirectory(../../tests/proj/cmake/TALibTestDynamicChannelConvolution cmake-TALibTestDynamicChannelConvolution-bin)
#add_subdirectory(../../tests/proj/cmake/TALibVRTest cmake-TALibVRTest-bin)
#add_subdirectory(../../tests/proj/cmake/TanDeviceResourcesTest cmake-TanDeviceResourcesTest-bin)