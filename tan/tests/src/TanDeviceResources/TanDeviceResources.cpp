// TanDeviceResourcesTest.cpp : Defines the entry point for the console application.
//
//
// Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <stdio.h>
#include <string.h>

#include <CL/cl.h>
#include "GpuUtilities.h"
//#include "public/include/core/Context.h"
//#include "public/common/AMFFactory.h"

#ifdef _WIN32
  #include <Windows.h>
  #include <io.h>
#endif

GPUUTILITIES_EXPORT int GPUUTILITIES_CDECL_CALL  listTanDevicesAndCaps(TanDeviceCapabilities **deviceList, int *count);

int listTanDevicesAndCaps(TanDeviceCapabilities **deviceListPtr, int *listLength)
{
    int status = 0;

    ADLAdapterInfo ADLInfo[100];
    int nADLAdapters = ADLQueryAdapterInfo(ADLInfo, 100);

    TanDeviceCapabilities *deviceList = new TanDeviceCapabilities[100];

    /*
    * Have a look at the available platforms
    */

    cl_uint numPlatforms = 0;
    cl_platform_id platform = NULL;
    cl_platform_id* platforms = NULL;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS) {
        fprintf(stdout, "clGetPlatformIDs returned error: %d\n", status);
        return 0;
    }
    if (0 < numPlatforms)
    {
        platforms = new cl_platform_id[numPlatforms];
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if (status != CL_SUCCESS) {
            fprintf(stdout, "clGetPlatformIDs returned error: %d\n", status);
            delete[] platforms;
            return 0;
        }
    }


    // enumerate devices
    char driverVersion[100] = "\0";
    char *extensions[2048];

    // Retrieve device
    int totalDevices = 0;

    cl_device_type clDeviceTypes[] = { CL_DEVICE_TYPE_GPU, CL_DEVICE_TYPE_CPU };

    for (int dt = 0; dt < sizeof(clDeviceTypes) / sizeof(cl_device_type); dt++) {


        for (unsigned int nPlatform = 0; nPlatform < numPlatforms; nPlatform++) {
            platform = platforms[nPlatform];

            cl_uint numDevices = 0;
            clGetDeviceIDs(platform, clDeviceTypes[dt], 0, NULL, &numDevices);
            if (numDevices == 0)
            {
                continue;
            }
            cl_device_id* devices = new cl_device_id[numDevices];
            for (cl_uint i = 0; i < numDevices; i++){
                devices[i] = NULL;
            }
            status = clGetDeviceIDs(platform, clDeviceTypes[dt], numDevices, devices, &numDevices);
            if (status != CL_SUCCESS) {
                fprintf(stdout, "clGetDeviceIDs returned error: %d\n", status);
            }
            status = clGetDeviceInfo(devices[0], CL_DRIVER_VERSION, sizeof(driverVersion), driverVersion, NULL);
            if (status != CL_SUCCESS) {
                fprintf(stdout, "clGetDeviceInfo returned error: %d\n", status);
            }
            else {
                /*
                cl_device_id devId;
                cl_device_type devType;
                char *deviceName;
                char *deviceVendor;
                char *driverVersion;
                bool supportsTAN;
                bool hasAttachedDisplay;
                int maxClockFrequency;
                float ComputeUnitPerfFactor;
                int totalComputeUnits;
                int maxReservableComputeUnits;
                int reserveComputeUnitsGranularity;
                int maxMemory;
                */

                //fprintf(stdout, "Driver version: %s\n", driverVersion);
                //fprintf(stdout, "%d devices found:\n", numDevices);

                for (unsigned int n = 0; n < numDevices; n++) {
                    int k = totalDevices + n;
                    //device id:
                    deviceList[k].devId = devices[n];
                    // device type:
                    clGetDeviceInfo(devices[n], CL_DEVICE_TYPE, 100, &deviceList[k].devType, NULL);
                    //device name:
                    deviceList[k].deviceName = new char[100];
                    deviceList[k].deviceName[0] = '\0';
                    clGetDeviceInfo(devices[n], CL_DEVICE_NAME, 100, deviceList[k].deviceName, NULL);
                    //device vendor:
                    deviceList[k].deviceVendor = new char[100];
                    deviceList[k].deviceVendor[0] = '\0';
                    clGetDeviceInfo(devices[n], CL_DEVICE_VENDOR, 100, deviceList[k].deviceVendor, NULL);

                    //driver version:
                    deviceList[k].driverVersion = new char[100];
                    deviceList[k].driverVersion[0] = '\0';
                    clGetDeviceInfo(devices[n], CL_DRIVER_VERSION, 100, deviceList[k].driverVersion, NULL);

                    //supports TAN:
                    deviceList[k].supportsTAN = checkOpenCL2_XCompatibility(devices[n]);

                    //maxClockFrequency:
                    clGetDeviceInfo(devices[n], CL_DEVICE_MAX_CLOCK_FREQUENCY, 100, &deviceList[k].maxClockFrequency, NULL);
                    //ComputeUnitPerfFactor:
                    deviceList[k].ComputeUnitPerfFactor = 1.0;
                    //totalComputeUnits:
                    clGetDeviceInfo(devices[n], CL_DEVICE_MAX_COMPUTE_UNITS, 100, &deviceList[k].totalComputeUnits, NULL);

                    //maxReservableComputeUnits:
                    //deviceList[k].maxReservableComputeUnits = deviceList[k].totalComputeUnits / 5; // hack TODO get from AMF
                    cl_context_properties contextProps[3] =
                    {
                        CL_CONTEXT_PLATFORM,
                        (cl_context_properties)platform,
                        0
                    };

                    cl_int error = 0;
                    deviceList[k].maxReservableComputeUnits = 0;
                    cl_context context = clCreateContext(contextProps, 1, &deviceList[k].devId, NULL, NULL, &error);
                    cl_command_queue queue = clCreateCommandQueue(context, deviceList[k].devId, NULL, &error);
                    //printf("Queue created %llX\r\n", queue);
                    getAMFdeviceProperties(queue, &deviceList[k].maxReservableComputeUnits);


                    //reserveComputeUnitsGranularity:
                    deviceList[k].reserveComputeUnitsGranularity = 4; // hack TODO get from AMF
                    if (deviceList[k].reserveComputeUnitsGranularity > deviceList[k].maxReservableComputeUnits) {
                        deviceList[k].reserveComputeUnitsGranularity = deviceList[k].maxReservableComputeUnits;
                    }

                    //maxMemory:
                    //clGetDeviceInfo(devices[n], CL_DEVICE_MAX_MEM_ALLOC_SIZE, 100, &deviceList[k].maxMemory, NULL);
                    clGetDeviceInfo(devices[n], CL_DEVICE_GLOBAL_MEM_SIZE, 100, &deviceList[k].maxMemory, NULL);
                    deviceList[k].localMemSize = 0;
                    clGetDeviceInfo(devices[n], CL_DEVICE_LOCAL_MEM_SIZE, 100, &deviceList[k].localMemSize, NULL);

                    #define CL_DEVICE_TOPOLOGY_AMD 0x4037
                    struct cl_device_topology_amd
                    {
                        union hack1
                        {
                            unsigned char data[2048];

                        };

                        struct pcie_
                        {
                            uint32_t bus;
                            uint32_t device;
                            uint32_t function;
                        };
                        pcie_ pcie;
                    };

                    //hack extra stuff
                    cl_device_topology_amd pciBusInfo;
                    memset(&pciBusInfo, 0, sizeof(pciBusInfo));

                    status = clGetDeviceInfo(devices[n], CL_DEVICE_TOPOLOGY_AMD, sizeof(cl_device_topology_amd), &pciBusInfo, NULL);
                    //if (status == CL_SUCCESS){
                    //    fprintf(stdout, "   PCI bus: %d device: %d function: %d\n", pciBusInfo.pcie.bus, pciBusInfo.pcie.device, pciBusInfo.pcie.function);
                    //}
                    status = clGetDeviceInfo(devices[n], CL_DEVICE_EXTENSIONS, 2047, extensions, NULL);

                    //hasAttachedDisplay:
                    deviceList[k].hasAttachedDisplay = false;
                    for (int l = 0; l < nADLAdapters; l++){
                        if (ADLInfo[l].busNumber == pciBusInfo.pcie.bus &&
                            ADLInfo[l].deviceNumber == pciBusInfo.pcie.device &&
                            ADLInfo[l].functionNumber == pciBusInfo.pcie.function)
                        {
                                if(ADLInfo[l].active){
                                    deviceList[k].hasAttachedDisplay = true;
                                }
                                deviceList[k].memoryBandwidth = ADLInfo[l].memoryBandwidth;
                                //hack
                                //deviceList[k].totalComputeUnits = ADLInfo[l].numCUs;

                        }
                    }

                    //{
                    ////    // hack wglGetCurrentContext needs OpenGL32.lib
                    //    cl_device_id devices[32];
                    //    memset(devices, 0, sizeof(devices));
                    //    size_t size = 0;
                    //   cl_context_properties properties[] = {
                    // //       CL_GL_CONTEXT_KHR, (cl_context_properties)wglGetCurrentContext(), //glXGetCurrentContext(), // GLX Context
                    ////        CL_GLX_DISPLAY_KHR, (cl_context_properties)0, //glXGetCurrentDisplay(), // GLX Display
                    //        CL_CONTEXT_PLATFORM, (cl_context_properties)platform, // OpenCL platform
                    ////        0
                    //    };

                    //    typedef int(__cdecl *clGetGLContextInfoKHRType)(const cl_context_properties * /* properties */,
                    //        cl_gl_context_info            /* param_name */,
                    //        size_t                        /* param_value_size */,
                    //        void *                        /* param_value */,
                    //        size_t *                      /* param_value_size_ret */);
                    //    clGetGLContextInfoKHRType clGetGLContextInfoKHR = nullptr;

                    //    clGetGLContextInfoKHR = (clGetGLContextInfoKHRType)clGetExtensionFunctionAddress("clGetGLContextInfoKHR");

                    //    clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 32 * sizeof(cl_device_id), devices, &size);

                    //    printf("%d", size);
                    //}


                }
            }

            for (cl_uint i = 0; i < numDevices; i++){
                clReleaseDevice(devices[i]);
            }
            totalDevices += numDevices;
            delete[] devices;
            devices = NULL;
        }
    }

    if (platforms)
        delete[] platforms;


    *listLength = totalDevices;
    *deviceListPtr = deviceList;

    return 0;
}

/*
cl_device_id devId;
cl_device_type devType;
char *deviceName;
char *deviceVendor;
char *driverVersion;
bool supportsTAN;
bool hasAttachedDisplay;
int maxClockFrequency;
float ComputeUnitPerfFactor;
int totalComputeUnits;
int maxReservableComputeUnits;
int reserveComputeUnitsGranularity;
int maxMemory;
*/

//using namespace amf;



int main(int argc, char* argv[])
{

    printf("%s, Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.",argv[0]);

    TanDeviceCapabilities *devCapsList = NULL;
    int count = 0;

    /*#ifdef _WIN32
        HMODULE GPUUtilitiesDll = NULL;

		typedef int (GPUUTILITIES_CDECL_CALL  *listTanDevicesAndCapsType)(TanDeviceCapabilities **deviceList, int *count);
		listTanDevicesAndCapsType listTanDevicesAndCaps = nullptr;

        GPUUtilitiesDll = LoadLibraryA("GPUUtilities.dll");
        if (NULL != GPUUtilitiesDll)
        {
            listTanDevicesAndCaps = (listTanDevicesAndCapsType)GetProcAddress(GPUUtilitiesDll, "listTanDevicesAndCaps");
            if (NULL != listTanDevicesAndCaps)
            {
                int result = listTanDevicesAndCaps(&devCapsList, &count);
            }
            else
            {
                MessageBoxA(NULL, "NOT FOUND listTanDevicesAndCaps", "GPUUtils...", MB_ICONERROR);
            }
        }
        else
        {
            MessageBoxA( NULL, "NOT FOUND GPUUtilities.dll", "GPUUtils...", MB_ICONERROR );
        }
    #else*/
        int result = listTanDevicesAndCaps(&devCapsList, &count);
    /*#endif*/

    for (int i = 0; i < count; i++){

       // clCreateCommandQueue()

        puts("");
        printf("Device #%d\n", i);
        printf("                         devId: %0x\n", devCapsList[i].devId);
        printf("                       devType: %s\n", (devCapsList[i].devType == CL_DEVICE_TYPE_GPU) ? "GPU" : "CPU");
        printf("                    deviceName: %s\n", devCapsList[i].deviceName);
        printf("                  deviceVendor: %s\n", devCapsList[i].deviceVendor);
        printf("                 driverVersion: %s\n", devCapsList[i].driverVersion);
        printf("                   supportsTAN: %d\n", devCapsList[i].supportsTAN);
        printf("            hasAttachedDisplay: %d\n", devCapsList[i].hasAttachedDisplay);
        printf("             maxClockFrequency: %d MHz\n", devCapsList[i].maxClockFrequency);
        printf("         ComputeUnitPerfFactor: %f\n", devCapsList[i].ComputeUnitPerfFactor);
        printf("             totalComputeUnits: %d\n", devCapsList[i].totalComputeUnits);
        printf("     maxReservableComputeUnits: %d\n", devCapsList[i].maxReservableComputeUnits);
        printf("reserveComputeUnitsGranularity: %d\n", devCapsList[i].reserveComputeUnitsGranularity);
        printf("                     maxMemory: %d MB\n", devCapsList[i].maxMemory /(1024*1024));
        printf("               MemoryBandwidth: %d MB/s\n", devCapsList[i].memoryBandwidth);
        printf("                   localMemory: %d KB\n", devCapsList[i].localMemSize/1024 );


    }

    // Usage:
    // 1) add code to select device based on reported properties.
    // 2) call clRetainDevice(devId) for selected device.
    // 3) delete devCapsList

    delete[] devCapsList;

    // 4) call clCreateContext with the selected device.


	return 0;
}

