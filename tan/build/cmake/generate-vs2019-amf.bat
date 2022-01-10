CLS
SETLOCAL

RMDIR /S /Q vs2019-amf
MKDIR vs2019-amf
CD vs2019-amf

SET CMAKE_PREFIX_PATH=c:\Qt\Qt6\6.2.2\msvc2019_64
cmake .. -G "Visual Studio 16 2019" -A x64 -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="C:\Program Files (x86)\OCL_SDK_Light\lib\x86_64\OpenCL.lib" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DTAN_NO_OPENCL=1 -DRTQ_ENABLED=1
