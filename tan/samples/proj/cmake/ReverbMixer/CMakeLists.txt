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
	message(SEND_ERROR "Failed to find OpenCL")
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
project(ReverbMixer DESCRIPTION "ReverbMixer")

include_directories(${Qt5Widgets_INCLUDE_DIRS})
link_directories(${Qt5Widgets_LIBRARY})

include_directories(../../../../../amf)
include_directories(../../../../../tan)

include_directories(../../../src/ReverbMixer)
include_directories(../../../src/ReverbMixer/UI)
include_directories(../../../src/ReverbMixer/QTObject)

#
#if(COMPILER_MSVC)
  add_compile_definitions(_CONSOLE)
  add_compile_definitions(_LIB)
  ADD_DEFINITIONS(-DUNICODE)
  ADD_DEFINITIONS(-D_UNICODE)
#endif()

# sources
set(
  SOURCE_EXE
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

  ../../../src/common/WASAPIUtils.cpp
  ../../../src/common/wav.cpp
  ../../../src/ReverbMixer/ReverbMixer.cpp
  ../../../src/ReverbMixer/ReverbProcessor.cpp

  ../../../src/ReverbMixer/QTObject/QTReverbMixerMainWindow.cpp
  ../../../src/ReverbMixer/QTObject/QTSaveResponse.cpp
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

  ../../../src/common/WASAPIUtils.h
  ../../../src/common/wav.h

  ../../../src/ReverbMixer/ReverbProcessor.h
  #../../../src/ReverbMixer/stdafx.h
  )

set(
  UI_FILES
  ../../../src/ReverbMixer/UI/ReverbMixer.ui
  ../../../src/ReverbMixer/UI/QD_SaveResponse.ui
  )

set(
  MOC_SOURCE_FILES
  ../../../src/ReverbMixer/QTObject/QTSaveResponse.h
  ../../../src/ReverbMixer/QTObject/QTReverbMixerMainWindow.h
  )

# preprocess QT files
qt5_wrap_ui(UI_HEADERS ${UI_FILES})
qt5_wrap_cpp(MOC_HEADERS ${MOC_SOURCE_FILES})

# create binary
add_executable(
  ReverbMixer
  ${SOURCE_EXE}
  ${HEADER_EXE}
  ${UI_HEADERS}
  ${MOC_HEADERS}
  )

target_link_libraries(ReverbMixer TrueAudioNext)
target_link_libraries(ReverbMixer GPUUtilities)
target_link_libraries(ReverbMixer Qt5::Widgets)