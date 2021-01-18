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
#pragma once

#include <CL/cl.h>

#include <cstdint>
#include <vector>

//define export declaration
#ifdef _WIN32

#define GPUUTILITIES_EXPORT __declspec(dllexport)
#define GPUUTILITIES_CDECL_CALL __cdecl

#else

#define GPUUTILITIES_EXPORT
#define GPUUTILITIES_CDECL_CALL

#endif

typedef void(GPUUTILITIES_CDECL_CALL *listGpuDeviceNamesType)(std::vector<std::string> & devicesNames);
typedef void(GPUUTILITIES_CDECL_CALL *listCpuDeviceNamesType)(std::vector<std::string> & devicesNames);
typedef int(GPUUTILITIES_CDECL_CALL *getDeviceAndContextType)(int devIdx, cl_context *pContext, cl_device_id *pDevice);
typedef cl_command_queue(GPUUTILITIES_CDECL_CALL *createQueueType)(cl_context context, cl_device_id device, int flag, int cu_);

typedef struct _TanDeviceCapabilities {
    cl_device_id devId;                     // OpenCL device ID
    cl_device_type devType;                 // device type [ CL_DEVICE_TYPE_CPU  | CL_DEVICE_TYPE_GPU | ...]
    char *deviceName;                       // device name string
    char *deviceVendor;                     // device vendor string
    char *driverVersion;                    // driver version string
    bool supportsTAN;                       // This device supports AMD True Audio Next
    bool hasAttachedDisplay;                // This device has a connected monitor
    int maxClockFrequency;                  // This device's maximum clock in MHz
    float ComputeUnitPerfFactor;            // Device specific performance factor
    int totalComputeUnits;                  // Total number of compute units in this device
    int maxReservableComputeUnits;          // Maximum reservable compute units <= 20% of Total
    int reserveComputeUnitsGranularity;     // granualarity of Cu reservation: typically 4
    std::uint64_t maxMemory;             // Maximum memory allocation on device
    std::uint64_t memoryBandwidth;       // Memory Bandwidth
    // extended
    std::uint64_t localMemSize;          // Local Memory size

    int reserved[1024 - sizeof(std::int64_t)];
} TanDeviceCapabilities;



// TAN objects creation functions.
extern "C"
{
    GPUUTILITIES_EXPORT void GPUUTILITIES_CDECL_CALL listGpuDeviceNames(std::vector<std::string> & devicesNames);
    GPUUTILITIES_EXPORT void GPUUTILITIES_CDECL_CALL listCpuDeviceNames(std::vector<std::string> & devicesNames);
    GPUUTILITIES_EXPORT void GPUUTILITIES_CDECL_CALL listOClDeviceNames(std::vector<std::string> & devicesNames, cl_device_type clDeviceType);

    GPUUTILITIES_EXPORT int GPUUTILITIES_CDECL_CALL getDeviceAndContext(int devIdx, cl_context *pContext, cl_device_id *pDevice, cl_device_type clDeviceType = CL_DEVICE_TYPE_GPU);

    GPUUTILITIES_EXPORT cl_command_queue GPUUTILITIES_CDECL_CALL createQueue(cl_context context, cl_device_id device, int flag = 0, int cu_ = 0);
}
