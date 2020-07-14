cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

if(AMF_CORE_STATIC)
  message(FATAL_ERROR "Error: this loader is not a full proprietary AMF library! To use staticaly linked proprietary AMF library add an appropriate CMakeFile.txt!")
endif()

# detects demanded name (previosly set in setup_amf.cmake)
get_property(AMF_LOADER_NAME GLOBAL PROPERTY AMF_LOADER_NAME)

if(NOT AMF_LOADER_NAME)
  message(FATAL_ERROR "Error: name for the loader library (public headers) for proprietary AMF is not set!")
endif()

message("Loader for proprietary AMF: " ${AMF_LOADER_NAME})

# library name
project(${AMF_LOADER_NAME})

include_directories(${TAN_ROOT}/amf)

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
  ${AMF_LOADER_NAME}
  STATIC

  ${SOURCE_LIB}
  ${HEADER_LIB}
  )

set_property(TARGET ${AMF_LOADER_NAME} PROPERTY POSITION_INDEPENDENT_CODE ON)

if(NOT WIN32)
  target_compile_options(amf PUBLIC -mavx2)
  target_compile_options(amf PUBLIC -mfma)
  target_compile_options(amf PUBLIC -msse4.2)

  if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    target_compile_options(amf PUBLIC -g)
  endif()

  if(NOT APPLE)
    target_compile_options(${AMF_LOADER_NAME} PUBLIC -Wpsabi)
  endif()
endif()