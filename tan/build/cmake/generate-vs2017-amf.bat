SETLOCAL

RMDIR /S /Q vs2017-amf
MKDIR vs2017-amf
CD vs2017-amf

SET CMAKE_PREFIX_PATH=c:\Qt\Qt5.6.3\5.6.3\msvc2015_64
REM cmake .. -G "Visual Studio 15 2017" -A x64 -DOpenCL_INCLUDE_DIR="C:\Program Files (x86)\OCL_SDK_Light\include" -DOpenCL_LIBRARY="C:\Program Files (x86)\OCL_SDK_Light\lib\x86_64\OpenCL.lib" -DPortAudio_DIR="../../../../../thirdparty/portaudio"
cmake .. -G "Visual Studio 15 2017" -A x64 -DOpenCL_INCLUDE_DIR="../../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="C:\Program Files (x86)\OCL_SDK_Light\lib\x86_64\OpenCL.lib" -DPortAudio_DIR="../../../../../thirdparty/portaudio" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DAMF_OPEN_DIR="../../../amfOpen" -DTAN_NO_OPENCL=1 -DAMF_CORE_STATIC=1
