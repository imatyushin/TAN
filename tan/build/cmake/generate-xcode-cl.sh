#/bin/sh
clear

rm -rf ./mac-xcode-cl
mkdir mac-xcode-cl
cd mac-xcode-cl

cmake -G "Xcode" .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DTAN_NO_OPENCL=0 -DAMF_CORE_STATIC=1
#cmake -G "Xcode" .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../thirdparty/portaudio" -DAMF_OPEN_DIR="../../../amfOpen" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DTAN_NO_OPENCL=0 -DAMF_CORE_STATIC=1

cd ..