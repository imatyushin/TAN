# TrueAudio Next (TAN) SDK

AMD TrueAudio Next is a software development kit for GPU accelerated and multi-core audio signal processing. TrueAudio Next provides pre-optimized library functions for computationally expensive algorithms such as time-varying audio convolution, FFT/FHT, and audio-oriented vector math. Sample applications and examples are included to facilitate integration into audio applications. The SDK also provides GPU utilities functions that support AMD GPU Resource Reservation, a technology that allows audio to share resources on the GPU with graphics while minimizing impact to quality-of-service.

<div>
  <a href="https://github.com/GPUOpen-LibrariesAndSDKs/TAN/releases/latest/"><img src="http://gpuopen-librariesandsdks.github.io/media/latest-release-button.svg" alt="Latest release" title="Latest release"></a>
</div>

### Supported OS
* Windows&reg; 7 (SP1 with the [Platform Update](https://msdn.microsoft.com/en-us/library/windows/desktop/jj863687.aspx)), Windows&reg; 8.1, or Windows&reg; 10,
* Linux (Tested on Ubutu 16.04, 18.04)
* MacOS X (Tested on 10.14 Mohave and 10.15 Catalina)

### Prerequisites
* CMake with version 3.0.11 or above
* Any CMake and C++11 compatible C/C++ compiler
* OpenCL 1.2 compatible videocard
* Metal 2.0 compatible videocard for running via Metal
* QT SDK, tested versions are from Qt 5.6.3 to QT 6

### HOW TO BUILD
1. Install QT distribution

2. Install OpenCL runtime:
  - [For Windows] Install OpenCL lib files, for example OpenCL Lite SDK from https://github.com/GPUOpen-LibrariesAndSDKs/OCL-SDK/releases
  - [For Linux] Install ROCm platform
  - [For MacOS X] OpenCL library files usualy available in OS and should be found by CMake, no manual installation is needed

3. Get TAN sources:
  git clone https://github.com/imatyushin/TAN.git [FolderName]

4. Go to clonned TAN sources:
  cd [FolderName]

5. Get third-party gits via command:
  git submodule update --init

6. Go to TAN's CMake folder:
  cd tan/build/cmake

7. Run neccessary generator script to prepare solution for your operation system and preffered IDE. Generators are the specially prepared shell scripts (bat or sh) which will create a related folder where cmake will be runned to prepare project's solution files. Shell scripts will set neccessary defines with supported options for cmake. The list of the supported options:

  - CMake's [CMAKE_PREFIX_PATH] variable is used to set path to demanded QT distribution, for example [CMAKE_PREFIX_PATH="/Applications/Qt5.6.3/5.6.3/clang_64"]

  - [-DOpenCL_INCLUDE_DIR] is used to set path to demanded OpenCL headers, the default value is ["../../../thirdparty/OpenCL-Headers"] where git's submodule will be cloned automatically via [git submodule update --init] command

  - [-DPortAudio_DIR] is used to set path to PortAudio sources, the default value is ["../../../thirdparty/portaudio"] where portaudio library will be automatically downloaded and extracted via CMake build process

  - [-DFFTW_DIR] is used to set path to FFTW distribution, the default value is ["../../../thirdparty/fftw"] where FFTW will be automatically downloaded and built via CMake build process

  - [-DMETALFFT_DIR] is used to set path for METAL_FFT library, the default value is ["../../../thirdparty/MetalFFT"] where METAL_FFTW will be cloned automatically via [git submodule update --init] command

  - [-DAMF_OPEN_DIR] is used to set path to AMF library, the default value is ["../../../amfOpen"] where OpenAMF library will be cloned automatically via [git submodule update --init] command

  - [-DDEFINE_AMD_OPENCL_EXTENSION] is used to define AMD specific OpenCL extensions in TAN headers, instead of getting a special AMD version of OpenCL headers, the default value is [1]

  - [-DTAN_NO_OPENCL] is an option to configure what type of configuration use to build AMF or OpenCL. AMF configuration used to allow cross-platform working via new AMF interfaces of TAN libraries. OpeCL is an old variant of TAN, working via plain OpenCL operations. [-DTAN_NO_OPENCL] is set to [1] in all AMF helper generators

  - [-DAMF_CORE_STATIC] is an option to control how AMF library will be built - statically or dinamically

  - [-DENABLE_METAL] is an option to control Apple Metal support on Mac OS

8. You could run CMake generation manually, prepare own generators scripts or use already prepared:

  - [generate-vs20xx-amf.bat] - generates VS.20xx solution for AMF type of configuration for Win32, all options compatible with Win32 is set
  - [genetate-vs20xx-cl.bat] - generates VS.20xx solution for OpenCL variant of configuration for Win32, all options compatible with Win32 is set
  - [generate-linux-amf.bat] - generates makefiles on Linux, for AMF type of configuration, all options compatible with Linux is set
  - [generate-linux-cl.bat] - generates makefiles on Linux, for OpenCL variant of configuration, all options compatible with Linux is set
  - [generate-xcode-cl.sh] - generates XCode solution, uses raw OpenCL implementation, uses OpenCL for computing, FFTW library will be used for FFT calclulation via CPU
  - [generate-xcode-cl-nofftw.sh] - generates XCode solution, uses raw OpenCL implementation, uses OpenCL for computing, reference software FFT calclulation will be used when run via CPU
  - [generate-xcode-amf-cl.sh] - generates XCode solution, uses AMF type of configuration, uses OpenCL for computing, FFTW library will be used for FFT calclulation via CPU
  - [generate-xcode-amf-cl-nofftw.sh] - generates XCode solution, uses AMF type of configuration, uses OpenCL for computing, reference software FFT calclulation will be used when run via CPU
  - [generate-xcode-amf-metal.sh] - generates XCode solution, uses AMF type of configuration, uses Metal runtime, FFTW library will be used for FFT calclulation via CPU
  - [generate-xcode-amf-metal-nofftw.sh] - generates XCode solution, uses AMF type of configuration, uses Metal runtime, no FFTW library will be used)
  - [generate-mac-cl.sh] - generates Makefile, uses raw OpenCL implementation, uses OpenCL for computing, FFTW library will be used for FFT calclulation via CPU
  - [generate-mac-cl-nofftw.sh] - generates Makefile, uses raw OpenCL implementation, uses OpenCL for computing, reference software FFT calclulation will be used when run via CPU
  - [generate-mac-amf-cl.sh] - generates Makefile, uses AMF type of configuration, uses OpenCL for computing, FFTW library will be used for FFT calclulation via CPU
  - [generate-mac-amf-cl-nofftw.sh] - generates Makefile, uses AMF type of configuration, uses OpenCL for computing, reference software FFT calclulation will be used when run via CPU
  - [generate-mac-amf-metal.sh] - generates Makefile, uses AMF type of configuration, uses Metal runtime, FFTW library will be used for FFT calclulation via CPU
  - [generate-mac-amf-metal-nofftw.sh] - generates Makefile, uses AMF type of configuration, uses Metal runtime, no FFTW library will be used)

  Please not that .sh files could not have an executable property after cloning git on Unixes. In that case yould run command [chmod +x ./*.sh]

9. After CMake will generates neccessary projects or Makefiles you could open generated project file in the related IDE (Visual Studio/XCode/...) or run build process for Makefile. To run demanded build process for Makefiles the are several already prepared runners existing in tan/build/cmake directory:

  - [build-linux-amf.sh] - build Makefiles created for OpenCL via AMF configuration on Linux
  - [build-linux-cl.sh] - build Makefiles created for OpenCL configuration on Linux
  - [build-mac-amf-cl.sh] - build Makefiles created for OpenCL via AMF configuration on Mac
  - [build-mac-amf-metal.sh] - build Makefiles created for OpenCL via AMF configuration on Mac
  - [build-mac-cl.sh] - build Makefiles created for OpenCL via AMF configuration on Mac
  - [build-vs20xx-msbuild-debug-amf.sh] - build VS solution in console via MSBuild, for debug configuration, for AMF variant of TAN
  - [build-vs20xx-msbuild-debug-cl.sh] - build VS solution in console via MSBuild, for debug configuration, for OpenCL variant of TAN
  - [build-vs20xx-msbuild-release-amf.sh] - build VS solution in console via MSBuild, for release configuration, for AMF variant of TAN
  - [build-vs20xx-msbuild-release-cl.sh] - build VS solution in console via MSBuild, for release configuration, for OpenCL variant of TAN

  If you builing TAN with FFTW support enabled, building process will generates FFTW cache file if it is not found in [TEMP FILES]/AMD/TAN/FFTW_TAN_WISDOM.cache. If this file was removed TAN will rebuild this file after PLAY button will be pressed. Rebuilding of the FFTW cache will take a long time.

10. CMake project for TAN will collect all demanded dependencies to the binary folders of the built applications. After building process will be completed you could run RoomAcousticQT example via RUN comman in used IDE or manually, usually RoomAcousticQT binaries is placed in tan/build/cmake/[generation type]/cmake-RoomAcousticQT-bin folder on Unixes and in the project output folder on Windows

### KNOWN ISSUES
- Graal2 library currently not ported to AMF and its usage is temporary disabled in TAN

- Operation systems periodically removing files in [Temp] folder, and FFTW cache file will be lost. TAN will rebuild it after next run. This cache file should be moved to [AppData] folder in future.

- MetalFFTW integration still buggy, will be fixed on future.

### Attribution
* AMD, the AMD Arrow logo, Radeon, LiquidVR and combinations thereof are either registered trademarks or trademarks of Advanced Micro Devices, Inc. in the United States and/or other countries.
* Microsoft, Visual Studio, and Windows are either registered trademarks or trademarks of Microsoft Corporation in the United States and/or other countries.
