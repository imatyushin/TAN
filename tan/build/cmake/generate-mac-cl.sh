#/bin/sh
clear
rm -rf ./mac-gnumake
mkdir mac-gnumake
cd mac-gnumake

cmake .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../../../thirdparty/portaudio" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DAMF_OPEN_DIR="../../../amfOpen" -DTAN_NO_OPENCL=0 -DAMF_CORE_STATIC=1

cd ..