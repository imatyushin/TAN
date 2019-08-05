#/bin/sh

mkdir mac
cd mac
cmake .. -DCMAKE_PREFIX_PATH="~/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../../thirdparty/OpenCL-Headers" -DOpenCL_LIBRARY="/System/Library/Frameworks/OpenCL.framework" -DPortAudio_INCLUDE_DIRS="../../../../thirdparty/portaudio/include" -DPortAudio_LIBRARY="~/TAN/thirdparty/portaudio/lib/.libs/libportaudio.dylib"
cd ..