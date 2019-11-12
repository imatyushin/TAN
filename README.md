# TrueAudio Next (TAN) SDK

AMD TrueAudio Next is a software development kit for GPU accelerated and multi-core audio signal processing. TrueAudio Next provides pre-optimized library functions for computationally expensive algorithms such as time-varying audio convolution, FFT/FHT, and audio-oriented vector math. Sample applications and examples are included to facilitate integration into audio applications. The SDK also provides GPU utilities functions that support AMD GPU Resource Reservation, a technology that allows audio to share resources on the GPU with graphics while minimizing impact to quality-of-service.

<div>
  <a href="https://github.com/GPUOpen-LibrariesAndSDKs/TAN/releases/latest/"><img src="http://gpuopen-librariesandsdks.github.io/media/latest-release-button.svg" alt="Latest release" title="Latest release"></a>
</div>

### Prerequisites
* Windows&reg; 7 (SP1 with the [Platform Update](https://msdn.microsoft.com/en-us/library/windows/desktop/jj863687.aspx)), Windows&reg; 8.1, or Windows&reg; 10, Linux or MacOS X
* CMake with version 3.0.11 or above
* Any CMake and C++11 compatible C/C++ compiler
* OpenCL 1.2 compatible drivers for related platform
* QT SDK, version 5.6.3 is the default tested version

### Getting Started
* Create solution and project files by CMake project in `tan/build/cmake` directory.
* To run CMake you could use prepared shell or bat scripts for related platform and compiler/ide variants placed in `tan/build/cmake` directory.
* Additional documentation can be found in the `tan` directory.

### Supported CMake defines
The following CMake defines are currently supported, please use them to generate solution by CMake:
* -DCMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64" <- path to QT binary folder
* -DOpenCL_INCLUDE_DIR="../../../../thirdparty/OpenCL-Headers" <- path to OpenCL headers
* -DPortAudio_DIR="../../../../../thirdparty/portaudio" <- path to PortAudio folder, if needed
* -DDEFINE_AMD_OPENCL_EXTENSION=1 <- will declare absend AMD's OpenCL extentions internally
* -DAMF_OPEN_DIR="../../../amfOpen" <- path to opensource AMF implementation to use instead of proprietary implementation, if needed
* -DTAN_AMF=1 <- will use TAN/AMF implementation and headers instead of TAN/OpenCL

### Attribution
* AMD, the AMD Arrow logo, Radeon, LiquidVR and combinations thereof are either registered trademarks or trademarks of Advanced Micro Devices, Inc. in the United States and/or other countries.
* Microsoft, Visual Studio, and Windows are either registered trademarks or trademarks of Microsoft Corporation in the United States and/or other countries.
