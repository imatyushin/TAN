CLS
SETLOCAL

RMDIR /S /Q vs2017-cl
MKDIR vs2017-cl
CD vs2017-cl

SET CMAKE_PREFIX_PATH=c:\Qt\Qt5.6.3\5.6.3\msvc2015_64
cmake .. -G "Visual Studio 15 2017" -A x64 -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="C:\Program Files (x86)\OCL_SDK_Light\lib\x86_64\OpenCL.lib" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DAMF_CORE_SHARED=1 -DDEFINE_AMD_OPENCL_EXTENSION=1
