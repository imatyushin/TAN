#/bin/sh
clear
rm -rf ./mac-gnumake-amf-metal
mkdir mac-gnumake-amf-metal
cd mac-gnumake-amf-metal

cmake .. -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" -DOpenCL_INCLUDE_DIR="../../../thirdparty/OpenCL-Headers" -DPortAudio_DIR="../../../thirdparty/portaudio" -DFFTW_DIR_="../../../thirdparty/fftw" -DAMF_OPEN_DIR="../../../amfOpen" -DMETALFFT_DIR="../../../thirdparty/MetalFFT" -DDEFINE_AMD_OPENCL_EXTENSION=1 -DTAN_NO_OPENCL=1 -DAMF_CORE_STATIC=1 -DENABLE_METAL=1

cd ..