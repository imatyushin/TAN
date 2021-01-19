#/bin/sh
clear

rm -rf ./mac-xcode-amf-cl
mkdir mac-xcode-amf-cl
cd mac-xcode-amf-cl

cmake -G "Xcode" .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DTAN_NO_OPENCL=1 -DAMF_CORE_STATIC=1

cd ..