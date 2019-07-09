#/bin/sh

mkdir linux
cd linux
cmake .. -DCMAKE_PREFIX_PATH=/opt/Qt/5.6.3/gcc_64 -DOpenCL_INCLUDE_DIR=/opt/rocm/opencl/include -DOpenCL_LIBRARY=/opt/rocm/opencl/lib/x86_64/libOpenCL.so -DPortAudio_INCLUDE_DIRS=../../../../../thirdparty/portaudio/include -DPortAudio_LIBRARY=~/TAN/thirdparty/portaudio/lib/.libs/libportaudio.so
cd ..
