CLS
SETLOCAL

RMDIR /S /Q vs2019-amf
MKDIR vs2019-amf
CD vs2019-amf

SET CMAKE_PREFIX_PATH=c:\Qt\Qt5.9.9\5.9.9\msvc2017_64
cmake .. -G "Visual Studio 16 2019" -A x64 -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="C:\Program Files (x86)\OCL_SDK_Light\lib\x86_64\OpenCL.lib" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DAMF_CORE_SHARED=1 -DDEFINE_AMD_OPENCL_EXTENSION=1 -DTAN_NO_OPENCL=1
