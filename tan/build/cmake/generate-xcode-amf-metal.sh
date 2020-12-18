#/bin/sh
clear

rm -rf ./mac-xcode-amf-metal
mkdir mac-xcode-amf-metal
cd mac-xcode-amf-metal

cmake -G "Xcode" .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../../../thirdparty/portaudio" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DAMF_OPEN_DIR="../../../amfOpen" -DTAN_NO_OPENCL=1 -DAMF_CORE_STATIC=1 -DENABLE_METAL=1

cd ..