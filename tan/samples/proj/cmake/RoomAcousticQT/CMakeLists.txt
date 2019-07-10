cmake_minimum_required(VERSION 3.10)

# The cmake-policies(7) manual explains that the OLD behaviors of all
# policies are deprecated and that a policy should be set to OLD only under
# specific short-term circumstances.  Projects should be ported to the NEW
# behavior and not rely on setting a policy to OLD.

# VERSION not allowed unless CMP0048 is set to NEW
if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif (POLICY CMP0048)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SKIP_RULE_DEPENDENCY TRUE)

enable_language(CXX)

find_package(OpenCL REQUIRED)
if(NOT OpenCL_FOUND)
  message(FATAL_ERROR "Failed to find OpenCL")
  return()
else()
  include_directories(${OpenCL_INCLUDE_DIRS})
  link_directories(${OpenCL_LIBRARY})
endif()

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
#set(CMAKE_AUTOMOC ON)
# Create code from a list of Qt designer ui files
#set(CMAKE_AUTOUIC ON)

# Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)
if(NOT Qt5Widgets_FOUND)
  message(SEND_ERROR "Failed to find Qt5Widgets")
  return()
endif()

# name
project(RoomAcousticsQT DESCRIPTION "RoomAcousticsQT")

if(PortAudio_INCLUDE_DIRS)
  message("PortAudio_INCLUDE_DIRS: " ${PortAudio_INCLUDE_DIRS})
  message("PortAudio_LIBRARY: " ${PortAudio_LIBRARY})

  include_directories(${PortAudio_INCLUDE_DIRS})

  ADD_DEFINITIONS(-DENABLE_PORTAUDIO)
endif()

include_directories(${Qt5Widgets_INCLUDE_DIRS})
link_directories(${Qt5Widgets_LIBRARY})

include_directories(../../../../../amf)
include_directories(../../../../../tan)
include_directories(../../../src/common)
include_directories(../../../../common)

include_directories(../../../src/RoomAcousticQT/QTObject)

ADD_DEFINITIONS(-DRTQ_ENABLED)
ADD_DEFINITIONS(-D_DEBUG)

# sources
set(
  SOURCE_EXE
  ../../../src/RoomAcousticQT/Resources/Resource.rc

  ../../../../../amf/public/common/AMFFactory.cpp
  ../../../../../amf/public/common/AMFSTL.cpp
  ../../../../../amf/public/common/DataStreamFactory.cpp
  ../../../../../amf/public/common/DataStreamFile.cpp
  ../../../../../amf/public/common/DataStreamMemory.cpp
  ../../../../../amf/public/common/IOCapsImpl.cpp
  ../../../../../amf/public/common/PropertyStorageExImpl.cpp
  ../../../../../amf/public/common/Thread.cpp
  ../../../../../amf/public/common/TraceAdapter.cpp
  ../../../../../amf/public/common/Windows/ThreadWindows.cpp

  ../../../src/common/common.cpp
  ../../../src/common/GpuUtils.cpp
  ../../../src/common/SimpleVRaudio.cpp
  ../../../src/common/utilities.c

  ../../../../common/FileUtility.cpp
  ../../../../common/StringUtility.cpp
  ../../../../common/RegisterBrowser.cpp
  ../../../../common/wav.cpp
  ../../../../common/fifo.cpp
  ../../../../common/cpucaps.cpp

  ../../../src/RoomAcousticQT/main.cpp
  ../../../src/RoomAcousticQT/QTObject/AdvanceGraphicView.cpp
  ../../../src/RoomAcousticQT/QTObject/QTExportResponse.cpp
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticConfig.cpp
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticGraphic.cpp
  ../../../src/RoomAcousticQT/RoomAcousticQT.cpp
  )

if(WIN32)
  list(APPEND SOURCE_EXE ../../../src/common/WASAPIPlayer.cpp)
else()
  list(APPEND SOURCE_EXE ../../../src/common/PulsePlayer.cpp)
  list(APPEND SOURCE_EXE ../../../src/common/AlsaPlayer.cpp)
endif()

set(
  HEADER_EXE
  ../../../../../amf/public/common/AMFFactory.h
  ../../../../../amf/public/common/AMFSTL.h
  ../../../../../amf/public/common/ByteArray.h
  ../../../../../amf/public/common/DataStream.h
  ../../../../../amf/public/common/DataStreamFile.h
  ../../../../../amf/public/common/DataStreamMemory.h
  ../../../../../amf/public/common/InterfaceImpl.h
  ../../../../../amf/public/common/IOCapsImpl.h
  ../../../../../amf/public/common/ObservableImpl.h
  ../../../../../amf/public/common/PropertyStorageExImpl.h
  ../../../../../amf/public/common/PropertyStorageImpl.h
  ../../../../../amf/public/common/Thread.h
  ../../../../../amf/public/common/TraceAdapter.h
  ../../../../../amf/public/include/core/AudioBuffer.h
  ../../../../../amf/public/include/core/Buffer.h
  ../../../../../amf/public/include/core/Compute.h
  ../../../../../amf/public/include/core/ComputeFactory.h
  ../../../../../amf/public/include/core/Context.h
  ../../../../../amf/public/include/core/Data.h
  ../../../../../amf/public/include/core/Debug.h
  ../../../../../amf/public/include/core/Factory.h
  ../../../../../amf/public/include/core/Interface.h
  ../../../../../amf/public/include/core/Plane.h
  ../../../../../amf/public/include/core/Platform.h
  ../../../../../amf/public/include/core/PropertyStorage.h
  ../../../../../amf/public/include/core/PropertyStorageEx.h
  ../../../../../amf/public/include/core/Result.h
  ../../../../../amf/public/include/core/Surface.h
  ../../../../../amf/public/include/core/Trace.h
  ../../../../../amf/public/include/core/Variant.h
  ../../../../../amf/public/include/core/Version.h

  ../../../../common/fifo.h

  ../../../src/common/common.h
  ../../../src/common/GpuUtils.h
  ../../../src/common/maxlimits.h
  ../../../src/common/SimpleVRaudio.h
  ../../../src/common/utilities.h
  ../../../src/common/WasAPI.h
  ../../../src/common/IWavPlayer.h

  ../../../../common/FileUtility.h
  ../../../../common/StringUtility.h
  ../../../../common/RegisterBrowser.h
  ../../../../common/wav.h
  ../../../../common/cpucaps.h

  ../../../src/RoomAcousticQT/QTObject/AdvanceGraphicView.h
  ../../../src/RoomAcousticQT/Resources/resource.h
  ../../../src/RoomAcousticQT/RoomAcousticQT.h
)

if(WIN32)
  list(APPEND HEADER_EXE ../../../src/common/WASAPIPlayer.h)
else()
  list(APPEND HEADER_EXE ../../../src/common/PulsePlayer.h)
  list(APPEND HEADER_EXE ../../../src/common/AlsaPlayer.h)
endif()

if(PortAudio_INCLUDE_DIRS)
  list(APPEND SOURCE_EXE ../../../src/common/PortPlayer.cpp)
  list(APPEND HEADER_EXE ../../../src/common/PortPlayer.h)
endif()

set(
  UI_FILES
  ../../../src/RoomAcousticQT/UI/RoomAcousticConfig.ui
  ../../../src/RoomAcousticQT/UI/RoomAcousticConfig_NoRTQ.ui
  ../../../src/RoomAcousticQT/UI/ExportResponse.ui
  )

set(
  MOC_SOURCE_FILES
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticConfig.h
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticGraphic.h
  ../../../src/RoomAcousticQT/QTObject/QTExportResponse.h
  )

set(
  QRC_FILES
  ../../../src/RoomAcousticQT/roomaccousticnew.qrc
  )

# preprocess QT files
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_wrap_cpp(MOC_HEADERS ${MOC_SOURCE_FILES})
qt5_add_resources(RCC_SOURCES ${QRC_FILES})

# create binary
add_executable(
  RoomAcousticsQT
  ${SOURCE_EXE}
  ${HEADER_EXE}
  ${UI_HEADERS}
  ${MOC_HEADERS}
  ${RCC_SOURCES}
  )

target_link_libraries(RoomAcousticsQT TrueAudioNext)
target_link_libraries(RoomAcousticsQT TrueAudioVR)
target_link_libraries(RoomAcousticsQT GPUUtilities)
target_link_libraries(RoomAcousticsQT ${OpenCL_LIBRARY})
target_link_libraries(RoomAcousticsQT Qt5::Widgets)

if(WIN32)

target_link_libraries(RoomAcousticsQT "version.lib")
target_link_libraries(RoomAcousticsQT "shlwapi.lib")

else()

find_package(Threads REQUIRED)
target_link_libraries(RoomAcousticsQT ${CMAKE_THREAD_LIBS_INIT})

target_link_libraries(RoomAcousticsQT "asound")
target_link_libraries(RoomAcousticsQT "portaudio")

endif()

if(PortAudio_INCLUDE_DIRS)
  target_link_libraries(RoomAcousticsQT ${PortAudio_LIBRARY})
endif()