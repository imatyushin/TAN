cmake_minimum_required(VERSION 3.0)

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

include_directories(${Qt5Widgets_INCLUDE_DIRS})
link_directories(${Qt5Widgets_LIBRARY})

include_directories(../../../../../amf)
include_directories(../../../../../tan)
include_directories(../../../src/common)

include_directories(../../../src/RoomAcousticQT/QTObject)

#
#if(COMPILER_MSVC)
  #add_compile_definitions(RTQ_ENABLED)
  #WIN32
  #WIN64
  #QT_DLL
  #QT_CORE_LIB
  #QT_GUI_LIB
  #QT_WIDGETS_LIB
  #ADD_DEFINITIONS(-DUNICODE)
  #ADD_DEFINITIONS(-D_UNICODE)
  #set(CMAKE_MFC_FLAG 1)
  #add_definitions(-D_AFXDLL)
  #add_definitions(-D_DLL)
#endif()

# sources
set(
  SOURCE_EXE
  ../../../src/RoomAcousticQT/Resources/Resource.rc

  ../../../../../AMF/public/common/AMFFactory.cpp
  ../../../../../AMF/public/common/AMFSTL.cpp
  ../../../../../AMF/public/common/DataStreamFactory.cpp
  ../../../../../AMF/public/common/DataStreamFile.cpp
  ../../../../../AMF/public/common/DataStreamMemory.cpp
  ../../../../../AMF/public/common/IOCapsImpl.cpp
  ../../../../../AMF/public/common/PropertyStorageExImpl.cpp
  ../../../../../AMF/public/common/Thread.cpp
  ../../../../../AMF/public/common/TraceAdapter.cpp
  ../../../../../AMF/public/common/Windows/ThreadWindows.cpp

  ../../../src/common/fifo.cpp
  ../../../src/common/GpuUtils.cpp
  ../../../src/common/SimpleVRaudio.cpp
  ../../../src/common/utilities.c
  ../../../src/common/WASAPIUtils.cpp
  ../../../src/common/wav.cpp
  ../../../src/RoomAcousticQT/FileUtility.cpp
  ../../../src/RoomAcousticQT/main.cpp
  ../../../src/RoomAcousticQT/QTObject/AdvanceGraphicView.cpp
  ../../../src/RoomAcousticQT/QTObject/QTExportResponse.cpp
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticConfig.cpp
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticGraphic.cpp
  ../../../src/RoomAcousticQT/RegisterBrowser.cpp
  ../../../src/RoomAcousticQT/RoomAcoustic.cpp
  )

set(
  HEADER_EXE
  ../../../../../AMF/public/common/AMFFactory.h
  ../../../../../AMF/public/common/AMFSTL.h
  ../../../../../AMF/public/common/ByteArray.h
  ../../../../../AMF/public/common/DataStream.h
  ../../../../../AMF/public/common/DataStreamFile.h
  ../../../../../AMF/public/common/DataStreamMemory.h
  ../../../../../AMF/public/common/InterfaceImpl.h
  ../../../../../AMF/public/common/IOCapsImpl.h
  ../../../../../AMF/public/common/ObservableImpl.h
  ../../../../../AMF/public/common/PropertyStorageExImpl.h
  ../../../../../AMF/public/common/PropertyStorageImpl.h
  ../../../../../AMF/public/common/Thread.h
  ../../../../../AMF/public/common/TraceAdapter.h
  ../../../../../AMF/public/include/core/AudioBuffer.h
  ../../../../../AMF/public/include/core/Buffer.h
  ../../../../../AMF/public/include/core/Compute.h
  ../../../../../AMF/public/include/core/ComputeFactory.h
  ../../../../../AMF/public/include/core/Context.h
  ../../../../../AMF/public/include/core/Data.h
  ../../../../../AMF/public/include/core/Debug.h
  ../../../../../AMF/public/include/core/Factory.h
  ../../../../../AMF/public/include/core/Interface.h
  ../../../../../AMF/public/include/core/Plane.h
  ../../../../../AMF/public/include/core/Platform.h
  ../../../../../AMF/public/include/core/PropertyStorage.h
  ../../../../../AMF/public/include/core/PropertyStorageEx.h
  ../../../../../AMF/public/include/core/Result.h
  ../../../../../AMF/public/include/core/Surface.h
  ../../../../../AMF/public/include/core/Trace.h
  ../../../../../AMF/public/include/core/Variant.h
  ../../../../../AMF/public/include/core/Version.h
  ../../../src/common/fifo.h
  ../../../src/common/GpuUtils.h
  ../../../src/common/maxlimits.h
  ../../../src/common/SimpleVRaudio.h
  ../../../src/common/utilities.h
  ../../../src/common/WasAPI.h
  ../../../src/common/WASAPIUtils.h
  ../../../src/common/wav.h
  ../../../src/RoomAcousticQT/FileUtility.h
  ../../../src/RoomAcousticQT/QTObject/AdvanceGraphicView.h
  ../../../src/RoomAcousticQT/RegisterBrowser.h
  ../../../src/RoomAcousticQT/Resources/resource.h
  ../../../src/RoomAcousticQT/RoomAcoustic.h
)

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
target_link_libraries(RoomAcousticsQT ${OpenCL_LIBRARY})
target_link_libraries(RoomAcousticsQT Qt5::Widgets)

target_link_libraries(RoomAcousticsQT "version.lib")
target_link_libraries(RoomAcousticsQT "shlwapi.lib")