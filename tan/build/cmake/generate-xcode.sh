#/bin/sh

rm -rf ./mac
mkdir mac
cd mac
cmake -G "Xcode" .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="/System/Library/Frameworks/OpenCL.framework" -DPortAudio_DIR="../../../../../thirdparty/portaudio" -DDEFINE_AMD_OPENCL_EXTENSION=1
cd ..