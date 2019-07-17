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

include_directories(../../../../common)

include_directories(../../../../../amf)
include_directories(../../../../../tan)

include_directories(../../../src/ReverbMixer)
include_directories(../../../src/ReverbMixer/UI)
include_directories(../../../src/ReverbMixer/QTObject)

ADD_DEFINITIONS(-D_CONSOLE)
ADD_DEFINITIONS(-D_LIB)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

# sources
set(
  SOURCE_EXE

  ../../../src/common/GpuUtils.cpp

  ../../../src/common/WASAPIPlayer.cpp
  ../../../../common/wav.cpp

  ../../../src/ReverbMixer/ReverbMixer.cpp
  ../../../src/ReverbMixer/ReverbProcessor.cpp

  ../../../src/ReverbMixer/QTObject/QTReverbMixerMainWindow.cpp
  ../../../src/ReverbMixer/QTObject/QTSaveResponse.cpp
  )

set(
  HEADER_EXE

  ../../../src/common/WASAPIUtils.h

  ../../../../common/wav.h

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

target_link_libraries(ReverbMixer amf)
target_link_libraries(ReverbMixer TrueAudioNext)
target_link_libraries(ReverbMixer GPUUtilities)
target_link_libraries(ReverbMixer Qt5::Widgets)