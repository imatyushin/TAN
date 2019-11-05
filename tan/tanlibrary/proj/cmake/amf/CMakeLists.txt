cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

# detects demanded name (previosly set in setup_amf.cmake)
get_property(AMF_NAME GLOBAL PROPERTY AMF_NAME)

if(NOT AMF_NAME)
  #set_property(GLOBAL PROPERTY AMF_NAME amf)
  message(FATAL_ERROR "Error: proprietary AMF project name not set!")
endif()

message("Proprietary AMF project name: " ${AMF_NAME})

# library name
project(${AMF_NAME})
include_directories(${TAN_ROOT}/amf)

if(AMF_LIBRARY_NAME)
  message("AMF library name set to: " ${AMF_LIBRARY_NAME})
  message("this way does not work with some lexems, use environment variable AMF_DLL_NAME instead")

  ADD_DEFINITIONS(-DAMF_LIBRARY_NAME=${AMF_LIBRARY_NAME})
else()
  message("AMF will be built with default shared library name, use environment variable AMF_DLL_NAME to override it")
endif()

#ADD_DEFINITIONS(-DAMF_CORE_STATIC)

# sources
set(
  SOURCE_LIB

  ${TAN_ROOT}/amf/public/common/AMFFactory.cpp
  ${TAN_ROOT}/amf/public/common/AMFSTL.cpp
  ${TAN_ROOT}/amf/public/common/PropertyStorageExImpl.cpp
  ${TAN_ROOT}/amf/public/common/Thread.cpp
  ${TAN_ROOT}/amf/public/common/TraceAdapter.cpp
  ${TAN_ROOT}/amf/public/common/DataStreamFactory.cpp
  ${TAN_ROOT}/amf/public/common/DataStreamFile.cpp
  ${TAN_ROOT}/amf/public/common/DataStreamMemory.cpp
  ${TAN_ROOT}/amf/public/common/IOCapsImpl.cpp
  ${TAN_ROOT}/amf/public/common/PropertyStorageExImpl.cpp
)

if(WIN32)
  list(APPEND SOURCE_LIB ${TAN_ROOT}/amf/public/common/Windows/ThreadWindows.cpp)
else()
  list(APPEND SOURCE_LIB ${TAN_ROOT}/amf/public/common/Linux/ThreadLinux.cpp)
endif()

set(
  HEADER_LIB

    ../../../common/AMFMath.h
    ../../../common/AMFSTL.h
    ../../../common/ByteArray.h
    ../../../common/CurrentTime.h
    ../../../common/CurrentTimeImpl.h
    ../../../common/DataStream.h
    ../../../common/DataStreamFile.h
    ../../../common/DataStreamMemory.h
    ../../../common/IOCapsImpl.h
    ../../../common/InterfaceImpl.h
    ../../../common/ObservableImpl.h
    ../../../common/PropertyStorageExImpl.h
    ../../../common/PropertyStorageImpl.h
    ../../../common/Thread.h
    ../../../common/TraceAdapter.h

    ../../../include/components/Ambisonic2SRenderer.h
    ../../../include/components/AudioCapture.h
    ../../../include/components/ColorSpace.h
    ../../../include/components/Component.h
    ../../../include/components/ComponentCaps.h
    ../../../include/components/DisplayCapture.h
    ../../../include/components/FFMPEGAudioConverter.h
    ../../../include/components/FFMPEGAudioDecoder.h
    ../../../include/components/FFMPEGAudioEncoder.h
    ../../../include/components/FFMPEGComponents.h
    ../../../include/components/FFMPEGFileDemuxer.h
    ../../../include/components/FFMPEGFileMuxer.h
    ../../../include/components/FFMPEGVideoDecoder.h
    ../../../include/components/MediaSource.h
    ../../../include/components/VideoCapture.h
    ../../../include/components/VideoConverter.h
    ../../../include/components/VideoDecoderUVD.h
    ../../../include/components/VideoEncoderHEVC.h
    ../../../include/components/VideoEncoderVCE.h
    ../../../include/components/VideoStitch.h
    ../../../include/components/ZCamLiveStream.h

    ../../../include/core/AudioBuffer.h
    ../../../include/core/Buffer.h
    ../../../include/core/Compute.h
    ../../../include/core/ComputeFactory.h
    ../../../include/core/Context.h
    ../../../include/core/Data.h
    ../../../include/core/Debug.h
    ../../../include/core/Dump.h
    ../../../include/core/Factory.h
    ../../../include/core/Interface.h
    ../../../include/core/Plane.h
    ../../../include/core/Platform.h
    ../../../include/core/PropertyStorage.h
    ../../../include/core/PropertyStorageEx.h
    ../../../include/core/Result.h
    ../../../include/core/Surface.h
    ../../../include/core/Trace.h
    ../../../include/core/Variant.h
    ../../../include/core/Version.h

    ../../../src/core/BufferImpl.h
    ../../../src/core/ComputeOCL.h
    ../../../src/core/ContextImpl.h
    ../../../src/core/DataImpl.h
    ../../../src/core/DebugImpl.h
    ../../../src/core/Device.h
    ../../../src/core/DeviceHostImpl.h
    ../../../src/core/DeviceImpl.h
    ../../../src/core/DeviceOCLImpl.h
    ../../../src/core/FactoryImpl.h
    ../../../src/core/PlaneImpl.h
    ../../../src/core/ProgramsImpl.h
    ../../../src/core/SurfaceImpl.h
    ../../../src/core/TraceImpl.h
  )

# declare static library creation
add_library(
  ${AMF_NAME}
  STATIC
  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

set_property(TARGET ${AMF_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
  target_compile_options(${AMF_NAME} PUBLIC -mavx2)
  target_compile_options(${AMF_NAME} PUBLIC -mfma)
  target_compile_options(${AMF_NAME} PUBLIC -msse4.2)
  target_compile_options(${AMF_NAME} PUBLIC -g) #todo: to Debug only
  
  if(NOT APPLE)
    target_compile_options(${AMF_NAME} PUBLIC -Wpsabi)
  endif()
endif()