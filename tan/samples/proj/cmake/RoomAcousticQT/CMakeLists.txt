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

include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

include(${TAN_ROOT}/utils/cmake/test_OpenCL.cmake)
include(${TAN_ROOT}/utils/cmake/test_amf.cmake)

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

if(PortAudio_DIR)
  message("")
  message("PortAudio_DIR: " ${PortAudio_DIR})

  #test the ${PortAudio_DIR} is not an absolute path
  if(NOT IS_ABSOLUTE ${PortAudio_DIR})
    #test this cmake is included from other project
    if(NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
      get_filename_component(Path_To_PortAudio "${CMAKE_SOURCE_DIR}/${PortAudio_DIR}" ABSOLUTE)
    #assume that the project is built from itself
    else()
      get_filename_component(Path_To_PortAudio "${CMAKE_CURRENT_SOURCE_DIR}/${PortAudio_DIR}" ABSOLUTE)
    endif()
  else()
    get_filename_component(Path_To_PortAudio ${PortAudio_DIR} ABSOLUTE)
  endif()

  message("PortAudio_DIR expanded: " ${Path_To_PortAudio})

  INCLUDE(ExternalProject)
  #ExternalProject_Add(
  #  thirdparty_portaudio
  #  SOURCE_DIR          ${Path_To_PortAudio}
  #  #GIT_REPOSITORY     https://git.assembla.com/portaudio.git
  #  PREFIX              thirdparty_portaudio
  #  DOWNLOAD_DIR        ${TAN_ROOT}/downloads
  #  URL                 http://files.portaudio.com/archives/pa_stable_v190600_20161030.tgz
  #  URL_MD5             4df8224e047529ca9ad42f0521bf81a8
  #  #UPDATE_COMMAND     ""
  #  #CONFIGURE_COMMAND  ${Path_To_PortAudio}/configure
  #  #CONFIGURE_COMMAND  "" #cmake ${Path_To_PortAudio} -G "Unix Makefiles"
  #  #BUILD_IN_SOURCE    0
  #  #BUILD_COMMAND      make
  #  #BUILD_COMMAND      ""
  #  #INSTALL_COMMAND    "" #sudo make install
  #  )

  INCLUDE(FetchContent) #requires cmake 3.11
  FetchContent_Declare(
    portaudio_sources
    #DOWNLOAD_DIR        ${TAN_ROOT}/downloads
    URL                 http://files.portaudio.com/archives/pa_stable_v190600_20161030.tgz
    URL_MD5             4df8224e047529ca9ad42f0521bf81a8
    )

  FetchContent_GetProperties(portaudio_sources)
  if(NOT portaudio_sources_POPULATED)
    FetchContent_Populate(
      portaudio_sources
      #QUIET "0"
      DOWNLOAD_DIR        ${TAN_ROOT}/downloads
      URL                 http://files.portaudio.com/archives/pa_stable_v190600_20161030.tgz
      URL_MD5             4df8224e047529ca9ad42f0521bf81a8
      SOURCE_DIR          ${Path_To_PortAudio}
      BINARY_DIR          cmake-portaudio-bin
      )
  endif()

  add_subdirectory(${Path_To_PortAudio} cmake-portaudio-bin EXCLUDE_FROM_ALL)

  ADD_DEFINITIONS(-DENABLE_PORTAUDIO)
  include_directories(${Path_To_PortAudio}/include)
endif()

include_directories(${Qt5Widgets_INCLUDE_DIRS})
link_directories(${Qt5Widgets_LIBRARY})

include_directories(${AMF_HOME}/amf)
include_directories(${TAN_HEADERS})

include_directories(${TAN_ROOT}/utils/common)
include_directories(${TAN_ROOT}/tan/samples/src/common)
include_directories(${TAN_ROOT}/tan/samples/src/GPUUtilities)
include_directories(${TAN_ROOT}/tan/samples/src/TrueAudioVR)

include_directories(../../../src/RoomAcousticQT/QTObject)

# sources
set(
  SOURCE_EXE
  ../../../src/RoomAcousticQT/Resources/Resource.rc

  ../../../src/common/GpuUtils.cpp
  ${TAN_ROOT}/utils/common/FileUtility.cpp
  ${TAN_ROOT}/utils/common/StringUtility.cpp
  ${TAN_ROOT}/utils/common/RegisterBrowser.cpp
  ${TAN_ROOT}/utils/common/wav.cpp
  ${TAN_ROOT}/utils/common/fifo.cpp
  ${TAN_ROOT}/utils/common/cpucaps.cpp

  ../../../src/RoomAcousticQT/main.cpp
  ../../../src/RoomAcousticQT/QTObject/AdvanceGraphicView.cpp
  ../../../src/RoomAcousticQT/QTObject/QTExportResponse.cpp
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticConfig.cpp
  ../../../src/RoomAcousticQT/QTObject/QTRoomAcousticGraphic.cpp
  ../../../src/RoomAcousticQT/RoomAcousticQT.cpp
  )

if(TAN_NO_OPENCL)
  list(APPEND SOURCE_EXE ../../../src/common/Audio3DAMF.cpp)
else()
  list(APPEND SOURCE_EXE ../../../src/common/Audio3DOpenCL.cpp)
endif()

if(WIN32)
  list(APPEND SOURCE_EXE ../../../src/common/WASAPIPlayer.cpp)
else()
if(NOT APPLE)
  list(APPEND SOURCE_EXE ../../../src/common/AlsaPlayer.cpp)
endif()

endif()

set(
  HEADER_EXE

  ${TAN_ROOT}/utils/common/fifo.h

  ../../../src/common/GpuUtils.h
  ../../../src/common/maxlimits.h
  ../../../src/common/WasAPI.h
  ../../../src/common/IWavPlayer.h
  ../../../src/common/IAudio3D.h

  ${TAN_ROOT}/utils/common/FileUtility.h
  ${TAN_ROOT}/utils/common/StringUtility.h
  ${TAN_ROOT}/utils/common/RegisterBrowser.h
  ${TAN_ROOT}/utils/common/wav.h
  ${TAN_ROOT}/utils/common/cpucaps.h

  ../../../src/RoomAcousticQT/QTObject/AdvanceGraphicView.h
  ../../../src/RoomAcousticQT/Resources/resource.h
  ../../../src/RoomAcousticQT/RoomAcousticQT.h
)

if(TAN_NO_OPENCL)
  list(APPEND HEADER_EXE ../../../src/common/Audio3DAMF.h)
else()
  list(APPEND HEADER_EXE ../../../src/common/Audio3DOpenCL.h)
endif()

if(WIN32)
  list(APPEND HEADER_EXE ../../../src/common/WASAPIPlayer.h)
else()
  if(NOT APPLE)
    list(APPEND HEADER_EXE ../../../src/common/AlsaPlayer.h)
  endif()
endif()

if(PortAudio_DIR)
  list(APPEND SOURCE_EXE ../../../src/common/PortPlayer.cpp)
  list(APPEND HEADER_EXE ../../../src/common/PortPlayer.h)
  message("PortAudio support injected to project")
endif()

set(
  UI_FILES
  ../../../src/RoomAcousticQT/UI/RoomAcousticConfig.ui
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

if(AMF_CORE_STATIC)
  target_link_libraries(RoomAcousticsQT ${AMF_NAME})
else()
  target_link_libraries(RoomAcousticsQT ${AMF_LOADER_NAME})
endif()

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

  target_link_libraries(RoomAcousticsQT "dl")

  if(NOT APPLE)
    target_link_libraries(RoomAcousticsQT "asound")
  endif()
endif()

if(PortAudio_DIR)
  target_link_libraries(RoomAcousticsQT portaudio)
  #target_link_libraries(RoomAcousticsQT portaudio-static)
endif()

if(NOT WIN32)
  target_compile_options(RoomAcousticsQT PUBLIC -mavx2)
  target_compile_options(RoomAcousticsQT PUBLIC -mfma)
  target_compile_options(RoomAcousticsQT PUBLIC -msse4.2)

  if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    target_compile_options(RoomAcousticsQT PUBLIC -g)
  endif()

  if(NOT APPLE)
    target_compile_options(RoomAcousticsQT PUBLIC -Wpsabi)
  endif()
endif()

#copy runtime dll to the binary dir
if(WIN32)
  if(NOT AMF_CORE_STATIC)
    add_custom_command(
      TARGET RoomAcousticsQT
      POST_BUILD

      COMMAND ${CMAKE_COMMAND} -E copy_directory "${${AMF_NAME}_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>
      )
  endif()

  add_custom_command(
    TARGET RoomAcousticsQT
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${${AMF_NAME}_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TrueAudioNext_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${TrueAudioVR_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${GPUUtilities_BINARY_DIR}/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${RoomAcousticsQT_BINARY_DIR}/cmake-portaudio-bin/$<CONFIGURATION>" $<TARGET_FILE_DIR:RoomAcousticsQT>

    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Core> $<TARGET_FILE_DIR:RoomAcousticsQT>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Gui> $<TARGET_FILE_DIR:RoomAcousticsQT>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:Qt5::Widgets> $<TARGET_FILE_DIR:RoomAcousticsQT>
    )
endif()