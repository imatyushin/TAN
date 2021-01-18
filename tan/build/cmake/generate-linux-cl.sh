#/bin/sh

rm -rf ./linux
mkdir linux
cd linux

cmake .. -DCMAKE_PREFIX_PATH="/opt/Qt5.6.3/5.6.3/gcc_64" -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="/opt/rocm/opencl/lib/x86_64/libOpenCL.so" -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DDEFINE_AMD_OPENCL_EXTENSION=1

cd ..