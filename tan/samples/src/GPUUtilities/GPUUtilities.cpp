//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
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
#include "stdafx.h"
#include "GpuUtilities.h"

#include "public/include/core/Context.h"
#include "public/common/AMFFactoryHelper.h"

#include "../ADL/ADLQuery.h"

#include <stdio.h>
#include <string.h>
#include <iostream>

#ifdef _WIN32
  #include <io.h>
#endif

#include <CL/cl_ext.h>

#ifndef _WIN32
  #define sscanf_s sscanf
  #define sprintf_s sprintf
#endif

#ifndef CL_DEVICE_TOPOLOGY_AMD
    #define CL_DEVICE_TOPOLOGY_AMD 0x4037
#endif

#if defined(DEFINE_AMD_OPENCL_EXTENSION)

typedef union
{
    struct { cl_uint type; cl_uint data[5]; } raw;
    struct { cl_uint type; cl_uchar unused[17]; cl_uchar bus; cl_uchar device; cl_uchar function; } pcie;
} cl_device_topology_amd;

#endif

/* To Do: Check for GPU device not used by display....

// Create CL context properties, add GLX context & handle to DC
cl_context_properties properties[] = {
 CL_GL_CONTEXT_KHR, (cl_context_properties)glXGetCurrentContext(), // GLX Context
 CL_GLX_DISPLAY_KHR, (cl_context_properties)glXGetCurrentDisplay(), // GLX Display
 CL_CONTEXT_PLATFORM, (cl_context_properties)platform, // OpenCL platform
 0
};

// Find CL capable devices in the current GL context
cl_device_id devices[32];
size_t size;
clGetGLContextInfoKHR(properties, CL_DEVICES_FOR_GL_CONTEXT_KHR, 32 * sizeof(cl_device_id), devices, &size);

// We want to create context on device NOT in above list, to avoid conflict with graphics....

*/

/*
// partition CUs into two devices equally:
cl_uint max_CUs = 0;
clGetDeviceInfo(device, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &max_CUs, NULL);
fprintf(stdout, "   max compute units: %d\n", max_CUs);

cl_device_partition_property  props{ CL_DEVICE_PARTITION_EQUALLY, max_CUs/2, 0 }
cl_device_id devices[2];
clCreateSubDevices(device, props, 2, devices, NULL);
*/

/**
*******************************************************************************
* @fn listDeviceNames
* @brief returns list of installed  devices
*
* @param[out] devNames    : Points to empty string array to return device names
* @param[in] count		  : length of devNames
* @param[in] clDeviceType : CL_DEVICE_TYPE_GPU or CL_DEVICE_TYPE_CPU

* @return INT number of strings written to devNames array. (<= count)
*
*******************************************************************************
*/
void listOClDeviceNames(std::vector<std::string> & devicesNames, cl_device_type clDeviceType)
{
    int status;

    /*
    * Have a look at the available platforms and pick
    * the AMD one.
    */

    cl_platform_id platform = NULL;
    std::vector<cl_platform_id> platforms;

    cl_uint numPlatforms = 0;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);

    if (status != CL_SUCCESS)
    {
        std::cerr << "clGetPlatformIDs returned error: " << status << std::endl;

        return;
    }

    if (numPlatforms > 0)
    {
        platforms.resize(numPlatforms);
        status = clGetPlatformIDs(numPlatforms, &platforms.front(), NULL);

        if (status != CL_SUCCESS)
        {
            std::cerr << "clGetPlatformIDs returned error: " << status << std::endl;

            return;
        }

        for (unsigned i = 0; i < numPlatforms; ++i)
        {
            char vendor[128];
            status = clGetPlatformInfo(platforms[i],
                CL_PLATFORM_VENDOR,
                sizeof(vendor),
                vendor,
                NULL
                );

            if(status != CL_SUCCESS)
            {
                std::cerr << "clGetPlatformInfo returned error: " << status << std::endl;
                continue;
            }

            char version[128] = {0};
            status = clGetPlatformInfo(
                platforms[i],
                CL_PLATFORM_VERSION,
                sizeof(version),
                version,
                NULL
                );

            if(status != CL_SUCCESS)
            {
                std::cerr << "clGetPlatformInfo returned error: " << status << std::endl;
                continue;
            }

            std::cout << "OpenCL version for device " << vendor << ": " << version << std::endl;
        }
    }

    /*if (platform == NULL) {
        fprintf(stdout, "No suitable platform found!\n");
        return 0;
    }*/

    // enumerate devices
    char driverVersion[128] = {0};

    // Retrieve device
    for (unsigned int nPlatform = 0; nPlatform < numPlatforms; nPlatform++)
    {
        platform = platforms[nPlatform];

        cl_uint numDevices = 0;
        clGetDeviceIDs(platform, clDeviceType, 0, NULL, &numDevices);
        if (numDevices == 0)
        {
            continue;
        }

        std::vector<cl_device_id> devices(numDevices);

        status = clGetDeviceIDs(platform, clDeviceType, numDevices, &devices.front(), &numDevices);
        if (status != CL_SUCCESS) {
            std::cerr << "clGetDeviceIDs returned error: " << status << std::endl;
        }
        status = clGetDeviceInfo(devices[0], CL_DRIVER_VERSION, sizeof(driverVersion), driverVersion, NULL);

        if (status != CL_SUCCESS)
        {
            std::cerr << "clGetDeviceInfo returned error: " << status << std::endl;
        }
        else
        {
            std::cout
                << std::endl
                << "Driver version: " << driverVersion << std::endl
                << numDevices << " devices found" << std::endl
                ;

            //numDevices = 0;

            for (unsigned int n = 0; n < numDevices; n++)
            {
                char buffer[1024] = {0};

                clGetDeviceInfo(devices[n], CL_DEVICE_NAME, 1024, buffer, NULL);
                std::cout << "OpenCL device: " << buffer << std::endl;

                devicesNames.push_back(buffer);

                cl_device_topology_amd pciBusInfo = {0};
                status = clGetDeviceInfo(devices[n], CL_DEVICE_TOPOLOGY_AMD, sizeof(cl_device_topology_amd), &pciBusInfo, NULL);

                if(status == CL_SUCCESS)
                {
                    std::cout << "PCI bus: " << pciBusInfo.pcie.bus << " device: " << pciBusInfo.pcie.device << " function: " << pciBusInfo.pcie.function << std::endl;
                }
                else
                {
                    std::cout << "Error: could not retrieve CL_DEVICE_TOPOLOGY_AMD device topology!" << std::endl;
                }

                cl_uint max_CUs = 0;
                clGetDeviceInfo(devices[n], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(cl_uint), &max_CUs, NULL);
                std::cout << "max compute units: " << max_CUs << std::endl;
            }
        }

        for (cl_uint i = 0; i < numDevices; i++)
        {
            clReleaseDevice(devices[i]);
        }
    }
}


/**
*******************************************************************************
* @fn listGpuDeviceNames
* @brief returns list of installed GPU devices
*
* @param[out] devNames    : Points to empty string array to return device names
* @param[in] count		  : length of devNames

* @return INT number of strings written to devNames array. (<= count)
*
*******************************************************************************
*/
void listGpuDeviceNames(std::vector<std::string> & devicesNames)
{
    listOClDeviceNames(devicesNames, CL_DEVICE_TYPE_GPU);
}

void listCpuDeviceNames(std::vector<std::string> & devicesNames)
{
    listOClDeviceNames(devicesNames, CL_DEVICE_TYPE_CPU);
}

int getDeviceAndContext(int devIdx, cl_context *pContext, cl_device_id *pDevice, cl_device_type clDeviceType)
{
    cl_int error = CL_DEVICE_NOT_FOUND;

    cl_command_queue cmdQueue = NULL;

    int status;

    fprintf(stdout, "Creating General Queue\n");
    /*
    * Have a look at the available platforms and pick either
    * the AMD one if available or a reasonable default.
    */

    cl_uint numPlatforms = 0;
    cl_platform_id platform = NULL;
    cl_platform_id* platforms = NULL;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS) {
        fprintf(stdout, "clGetPlatformIDs returned error: %d\n", status);
        return NULL;
    }
    if (0 < numPlatforms)
    {
        platforms = new cl_platform_id[numPlatforms];
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        if (status != CL_SUCCESS) {
            fprintf(stdout, "clGetPlatformIDs returned error: %d\n", status);
            delete[] platforms;
            return NULL;
        }

        // ToDo: we need to extend this logic to read CL_PLATFORM_NAME, etc.
        // and add logic to decide which of several AMD GPUs to use.
        for (unsigned i = 0; i < numPlatforms; ++i)
        {
            char vendor[100];
            //char name[100];
            status = clGetPlatformInfo(platforms[i],
                CL_PLATFORM_VENDOR,
                sizeof(vendor),
                vendor,
                NULL);

            if (status != CL_SUCCESS) {
                fprintf(stdout, "clGetPlatformInfo returned error: %d\n", status);
                continue;
            }
        }
    }


    // enumerate devices

    // To Do: handle multi-GPU case, pick appropriate GPU/APU
    char driverVersion[100] = "\0";


    // Retrieve device
    int totalDevices = 0;
    for (unsigned int nPlatform = 0; nPlatform < numPlatforms; nPlatform++) {
        platform = platforms[nPlatform];

        cl_context_properties contextProps[3] =
        {
            CL_CONTEXT_PLATFORM,
            (cl_context_properties)platform,
            0
        };

        // Retrieve device
        cl_uint numDevices = 0;
        clGetDeviceIDs(platform, clDeviceType, 0, NULL, &numDevices);
        if (numDevices == 0)
        {
            continue;
        }

        cl_device_id* devices = new cl_device_id[numDevices];
        status = clGetDeviceIDs(platform, clDeviceType, numDevices, devices, &numDevices);
        if (status != CL_SUCCESS) {
            fprintf(stdout, "clGetDeviceIDs returned error: %d\n", status);
            delete[] devices;
            return NULL;
        }
        clGetDeviceInfo(devices[0], CL_DRIVER_VERSION, sizeof(driverVersion), driverVersion, NULL);

        // log all devices found:
        fprintf(stdout, "Driver version: %s\n", driverVersion);
        fprintf(stdout, "%d devices found:\n", numDevices);
        for (unsigned int n = 0; n < numDevices; n++) {
            char deviceName[100] = "\0";
            clGetDeviceInfo(devices[n], CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
            fprintf(stdout, "   GPU device %s\n", deviceName);
        }

        int deviceId = 0;
        if (devIdx >= totalDevices && devIdx < totalDevices + (int)numDevices){

            deviceId = devIdx - totalDevices;

            *pDevice = devices[deviceId];
            clRetainDevice(*pDevice);
            *pContext = clCreateContext(contextProps, 1, pDevice, NULL, NULL, &error);
            // log chosen device:
            if (error == CL_SUCCESS){
                char deviceName[100] = "\0";
                clGetDeviceInfo(devices[deviceId], CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
                fprintf(stdout, " Using OpenCL device %s\n", deviceName);
            }
            else {
                fprintf(stdout, "clCreateContext failed: %d \n", error);
            }
        }

        for (unsigned int idx = 0; idx < numDevices; idx++){
            clReleaseDevice(devices[idx]);
        }
        delete[] devices;
        devices = NULL;

        totalDevices += numDevices;
    }

    if (platforms)
        delete[] platforms;

    return error;
}


cl_command_queue createQueue(cl_context context, cl_device_id device, int flag, int var )
{
    cl_int error = 0;
    cl_command_queue cmdQueue = NULL;

    // Create a command queue
#if CL_TARGET_OPENCL_VERSION >= 200
    if (flag != 0)
    {
        // use clCreateCommandQueueWithProperties to pass custom queue properties to driver:
        const cl_queue_properties cprops[] = {
            CL_QUEUE_PROPERTIES,
            0,
            static_cast<cl_queue_properties>(std::uint64_t(flag)),
            static_cast<cl_queue_properties>(std::uint64_t(var)),
            static_cast<cl_queue_properties>(std::uint64_t(0))
            };
        // OpenCL 2.0
        cmdQueue = clCreateCommandQueueWithProperties(context, device, cprops, &error);
    }
    else
#else
    {
        // OpenCL 1.2
        cmdQueue = clCreateCommandQueue(context, device, NULL, &error);
    }
#endif

    //printf("\r\nOpenCL queue created: 0x%llX, error code: %d\r\n", cmdQueue, error);

    return cmdQueue;
}


/* */
int create1QueueOnDevice(cl_command_queue *queue, int devIdx = 0)
{
    cl_context context;
    cl_device_id device;
    getDeviceAndContext(devIdx, &context, &device);

    *queue = createQueue(context, device);

    clReleaseContext(context);
    clReleaseDevice(device);

    return 0;
}
int create2QueuesOnDevice(cl_command_queue *queue1, cl_command_queue *queue2, int devIdx = 0)
{
    cl_context context;
    cl_device_id device;
    getDeviceAndContext(devIdx, &context, &device);

    *queue1 = createQueue(context, device);
    *queue2 = createQueue(context, device);

    clReleaseContext(context);
    clReleaseDevice(device);


    return 0;
}



bool checkOpenCL2_XCompatibility(cl_device_id device_id)
{

    char deviceVersion[256];
    memset(deviceVersion, 0, 256);
    clGetDeviceInfo(device_id, CL_DEVICE_VERSION, 256, deviceVersion, NULL);
    cl_device_type clDeviceType = CL_DEVICE_TYPE_GPU;
    clGetDeviceInfo(device_id, CL_DEVICE_TYPE, sizeof(clDeviceType), &clDeviceType, NULL);

    bool isOpenCL2_XSupported = false;

    int majorRev, minorRev;
    if (sscanf_s(deviceVersion, "OpenCL %d.%d", &majorRev, &minorRev) == 2)
    {
        if (majorRev >= 2) {
            isOpenCL2_XSupported = true;
        }
    }

    // we only care about GPU devices:
    return isOpenCL2_XSupported || (clDeviceType != CL_DEVICE_TYPE_GPU);
}



bool getAMFdeviceProperties(cl_command_queue queue, int *maxReservedComputeUnits){
    AMF_RESULT res = AMF_OK;
    amf::AMFContextPtr amfContext = NULL;
    int maxCus = 0;

    g_AMFFactory.Init();
    // Create default CPU AMF context.
    if (g_AMFFactory.GetFactory() != NULL) {
        res = g_AMFFactory.GetFactory()->CreateContext(&amfContext);
    }
    if (res == AMF_OK && amfContext != 0){
        if (queue != NULL)
        {
            res = amfContext->InitOpenCL(queue);
            if (res == AMF_OK) {
                amf::AMFComputeFactoryPtr pOCLFactory;

                res = amfContext->GetOpenCLComputeFactory(&pOCLFactory);

                if (res == AMF_OK){
                    amf_int32 deviceCount = pOCLFactory->GetDeviceCount();
                    for (amf_int32 i = 0; i < deviceCount; i++)
                    {
                        amf::AMFComputeDevicePtr         pDeviceAMF;
                        res = pOCLFactory->GetDeviceAt(i, &pDeviceAMF);
                        if (amfContext->GetOpenCLDeviceID() == pDeviceAMF->GetNativeDeviceID())
                        {

                            pDeviceAMF->GetProperty(L"MaxRealTimeComputeUnits", &maxCus);
                            break; //TODO:AA
                        }
                    }
                }
            }
        }



        //amf_int64 tmp = 0;
        //amfContext->GetProperty(L"MaxRealTimeComputeUnits", &tmp);
        //*maxReservedComputeUnits = int(tmp);
        //amfContext->Terminate();
        *maxReservedComputeUnits = maxCus;
        return true;
    }
    else {
        return false;
    }
}
