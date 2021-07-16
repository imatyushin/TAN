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
#include <stdio.h>
#include <string.h>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#endif
#include <CL/cl.h>

#include "public/common/TraceAdapter.h"
#include "public/common/AMFFactory.h"
#include "public/include/core/Variant.h"
#include "public/include/core/ComputeFactory.h"

#define AMF_FACILITY amf::AMF_FACILITY

#ifdef RTQ_ENABLED
  #define AMFQUEPROPERTY L"MaxRealTimeComputeUnits"
#else
  #define AMFQUEPROPERTY L"None"
#endif

#include "GpuUtilities.h"

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
void listGpuDeviceNamesWrapper(std::vector<std::string> & devicesNames, const AMFFactoryHelper & factory)
{
    devicesNames.clear();

    AMF_RESULT res = g_AMFFactory.Init(); // initialize AMF

    if(AMF_OK == res)
    {
        // Create default CPU AMF context.
        amf::AMFContextPtr contextAMF;
        res = g_AMFFactory.GetFactory()->CreateContext(&contextAMF);

        if (AMF_OK == res)
        {
            amf::AMFComputeFactoryPtr pOCLFactory = NULL;

#if !defined ENABLE_METAL
            res = contextAMF->GetOpenCLComputeFactory(&pOCLFactory);
#else
            amf::AMFContext3Ptr context3(contextAMF);

            if(!context3)
            {
                return;
            }

            res = context3->GetMetalComputeFactory(&pOCLFactory);
#endif

            if (AMF_OK ==  res)
            {
                amf_int32 deviceCount = pOCLFactory->GetDeviceCount();

                for (amf_int32 i = 0; i < deviceCount; i++)
                {
                    amf::AMFComputeDevicePtr pDeviceAMF;
                    res = pOCLFactory->GetDeviceAt(i, &pDeviceAMF);
                    {
                        amf::AMFVariant name;
                        res = pDeviceAMF->GetProperty(AMF_DEVICE_NAME, &name);

                        if (AMF_OK == res)
                        {
                            devicesNames.push_back(name.stringValue);
                        }
                    }
                }
            }

            pOCLFactory.Release();
        }

        contextAMF.Release();
        g_AMFFactory.Terminate();
    }

#ifndef ENABLE_METAL
    else //USE OpenCL wrapper
    {
#ifdef _WIN32
        HMODULE GPUUtilitiesDll = NULL;
        typedef int(__cdecl * listGpuDeviceNamesType)(std::vector<std::string>& devicesNames);
        listGpuDeviceNamesType listGpuDeviceNames = nullptr;

        GPUUtilitiesDll = LoadLibraryA("GPUUtilities.dll");
        if (NULL != GPUUtilitiesDll)
        {
            listGpuDeviceNames = (listGpuDeviceNamesType)GetProcAddress(GPUUtilitiesDll, "listGpuDeviceNames");
            if (NULL != listGpuDeviceNames)
            {
                listGpuDeviceNames(devicesNames);
            }
            else
            {
                MessageBoxA(NULL, "NOT FOUND listGpuDeviceNames", "GPUUtils...", MB_ICONERROR);
            }
        }
        else
        {
            MessageBoxA(NULL, "NOT FOUND GPUUtilities.dll", "GPUUtils...", MB_ICONERROR);
        }
#else
        listGpuDeviceNames(devicesNames);
#endif
    }
#endif
}

/**
*******************************************************************************
* @fn listCpuDeviceNames
* @brief returns list of installed CPU devices
*
* @param[out] devNames    : Points to empty string array to return device names
* @param[in] count		  : length of devNames

* @return INT number of strings written to devNames array. (<= count)
*
*******************************************************************************
*/
void listCpuDeviceNamesWrapper(std::vector<std::string> & devicesNames, const AMFFactoryHelper & factory)
{
#if defined ENABLE_METAL
    return;
#else

    int foundCount = 0;
    AMF_RESULT res = g_AMFFactory.Init(); // initialize AMF
    if (AMF_OK == res)
    {
        // Create default CPU AMF context.
        amf::AMFContextPtr pContextAMF = NULL;
        res = g_AMFFactory.GetFactory()->CreateContext(&pContextAMF);
        amf::AMFVariantStruct value;

        //Hack To Do: define AMFContext property enum and use that instead of passing actual clDevice type
        // e.g. pContextAMF->SetProperty(L"AMF_CONTEXT_DEVICETYPE", AMF_CONTEXT_DEVICETYPE_GPU);

        pContextAMF->SetProperty(AMF_CONTEXT_DEVICE_TYPE, AMF_CONTEXT_DEVICE_TYPE_CPU);

        if (AMF_OK == res)
        {
            amf::AMFComputeFactoryPtr factory;
            res = pContextAMF->GetOpenCLComputeFactory(&factory);

            if(amf::AMFResultIsOK(res) && factory)
            {
                amf_int32 deviceCount = factory->GetDeviceCount();
                for (amf_int32 i = 0; i < deviceCount; i++)
                {
                    amf::AMFComputeDevicePtr pDeviceAMF;
                    res = factory->GetDeviceAt(i, &pDeviceAMF);
                    {
                        amf::AMFVariant name;
                        res = pDeviceAMF->GetProperty(AMF_DEVICE_NAME, &name);

                        if (AMF_OK == res)
                        {
                            devicesNames.push_back(name.stringValue);
                        }
                    }
                }
            }
        }

        pContextAMF.Release();
        g_AMFFactory.Terminate();
    }
    else
    {
#ifdef _WIN32
        HMODULE GPUUtilitiesDll = NULL;
        typedef int(__cdecl * listCpuDeviceNamesType)(std::vector<std::string>& devicesNames);
        listCpuDeviceNamesType listCpuDeviceNames = nullptr;

        GPUUtilitiesDll = LoadLibraryA("GPUUtilities.dll");
        if (NULL != GPUUtilitiesDll)
        {
            listCpuDeviceNames = (listCpuDeviceNamesType)GetProcAddress(GPUUtilitiesDll, "listCpuDeviceNames");
            if (NULL != listCpuDeviceNames)
            {
                listCpuDeviceNames(devicesNames);
            }
            else
            {
                MessageBoxA(NULL, "NOT FOUND listCpuDeviceNames", "GPUUtils...", MB_ICONERROR);
            }
        }
        else
        {
            MessageBoxA(NULL, "NOT FOUND GPUUtilities.dll", "GPUUtils...", MB_ICONERROR);
        }
#else
        listCpuDeviceNames(devicesNames);
#endif
    }
#endif
}

AMF_RESULT CreateCommandQueuesVIAamf(int deviceIndex, int32_t flag1, cl_command_queue *pcmdQueue1, int32_t flag2, cl_command_queue *pcmdQueue2, int amfDeviceType = AMF_CONTEXT_DEVICE_TYPE_GPU)
{
    bool AllIsOK = true;

    if (NULL != pcmdQueue1)
    {
        clReleaseCommandQueue(*pcmdQueue1);
        *pcmdQueue1 = NULL;
    }
    if (NULL != pcmdQueue2)
    {
        clReleaseCommandQueue(*pcmdQueue2);
        *pcmdQueue2 = NULL;
    }

    AMF_RESULT res = g_AMFFactory.Init(); // initialize AMF
    if (AMF_OK == res)
    {
        // Create default CPU AMF context.
        amf::AMFContextPtr contextAMF = NULL;
        res = g_AMFFactory.GetFactory()->CreateContext(&contextAMF);

        contextAMF->SetProperty(AMF_CONTEXT_DEVICE_TYPE, amfDeviceType);
        if (AMF_OK == res)
        {
            amf::AMFComputeFactoryPtr pOCLFactory = NULL;

#if !defined ENABLE_METAL
            res = contextAMF->GetOpenCLComputeFactory(&pOCLFactory);
            AMF_RETURN_IF_FAILED(res, L"GetOpenCLComputeFactory failed");
#else
            amf::AMFContext3Ptr context3(contextAMF);
            AMF_RETURN_IF_FALSE(context3 != nullptr, AMF_NOT_SUPPORTED);

            res = context3->GetMetalComputeFactory(&pOCLFactory);
            AMF_RETURN_IF_FAILED(res, L"GetMetalComputeFactory failed");
#endif

            if (AMF_OK == res)
            {
                amf_int32 deviceCount = pOCLFactory->GetDeviceCount();
                if (deviceIndex < deviceCount)
                {
                    amf::AMFComputeDevicePtr pDeviceAMF;
                    res = pOCLFactory->GetDeviceAt(deviceIndex, &pDeviceAMF);
                    if (nullptr != pDeviceAMF)
                    {
                        contextAMF->InitOpenCLEx(pDeviceAMF);
                        cl_context clContext = static_cast<cl_context>(pDeviceAMF->GetNativeContext());
                        cl_device_id clDevice = static_cast<cl_device_id>(pDeviceAMF->GetNativeDeviceID());
#ifdef RTQ_ENABLED
#define QUEUE_MEDIUM_PRIORITY 0x00010000
#define QUEUE_REAL_TIME_COMPUTE_UNITS 0x00020000
#endif
                        if (NULL != pcmdQueue1)
                        { //user requested one queue
                            int ComputeFlag = 0;
                            amf_int64 Param = flag1 & 0x0FFFF;
#ifdef RTQ_ENABLED
                            if (QUEUE_MEDIUM_PRIORITY == (flag1 & QUEUE_MEDIUM_PRIORITY))
                            {
                                ComputeFlag = 2;
                            }
                            if (QUEUE_REAL_TIME_COMPUTE_UNITS == (flag1 & QUEUE_REAL_TIME_COMPUTE_UNITS))
                            {
                                ComputeFlag = 1;
                            }
#endif
                            cl_command_queue tempQueue = NULL;
                            pDeviceAMF->SetProperty(AMFQUEPROPERTY, Param);
                            amf::AMFComputePtr AMFDevice;
                            pDeviceAMF->CreateCompute(&ComputeFlag, &AMFDevice);
                            if (nullptr != AMFDevice)
                            {
                                tempQueue = static_cast<cl_command_queue>(AMFDevice->GetNativeCommandQueue());
                            }
                            if (NULL == tempQueue)
                            {
                                fprintf(stdout, "createQueue failed to create cmdQueue1 ");
                                AllIsOK = false;
                            }
                            clRetainCommandQueue(tempQueue);

                            *pcmdQueue1 = tempQueue;
                        }

                        if (NULL != pcmdQueue2)
                        { //user requested second queue
                            int ComputeFlag = 0;
                            amf_int64 Param = flag2 & 0x0FFFF;
#ifdef RTQ_ENABLED
                            if (QUEUE_MEDIUM_PRIORITY == (flag2 & QUEUE_MEDIUM_PRIORITY))
                            {
                                ComputeFlag = 2;
                            }
                            if (QUEUE_REAL_TIME_COMPUTE_UNITS == (flag2 & QUEUE_REAL_TIME_COMPUTE_UNITS))
                            {
                                ComputeFlag = 1;
                            }
#endif
                            cl_command_queue tempQueue = NULL;
                            pDeviceAMF->SetProperty(AMFQUEPROPERTY, Param);
                            amf::AMFComputePtr AMFDevice;
                            pDeviceAMF->CreateCompute(&ComputeFlag, &AMFDevice);
                            if (nullptr != AMFDevice)
                            {
                                tempQueue = static_cast<cl_command_queue>(AMFDevice->GetNativeCommandQueue());
                            }
                            if (NULL == tempQueue)
                            {
                                fprintf(stdout, "createQueue failed to create cmdQueue2 ");
                                AllIsOK = false;
                            }
                            clRetainCommandQueue(tempQueue);

                            *pcmdQueue2 = tempQueue;
                        }
                    }
                }
                else
                {
                    res = AMF_INVALID_ARG;
                }
            }
            pOCLFactory.Release();
        }

        contextAMF.Release();
        g_AMFFactory.Terminate();
    }
    else
    {
        return AMF_NOT_INITIALIZED;
    }

    if (false == AllIsOK)
    {
        if (NULL != pcmdQueue1)
        {
            if (NULL != *pcmdQueue1)
            {
                clReleaseCommandQueue(*pcmdQueue1);
                *pcmdQueue1 = NULL;
            }
        }
        if (NULL != pcmdQueue2)
        {
            if (NULL != *pcmdQueue2)
            {
                clReleaseCommandQueue(*pcmdQueue2);
                *pcmdQueue2 = NULL;
            }
        }
    }

    return res;
}

AMF_RESULT CreateCommandQueuesVIAamf(
    int                         deviceIndex,
    amf_int32                   flag1,
    amf::AMFCompute             **compute1,
    amf_int32                   flag2,
    amf::AMFCompute             **compute2,
    AMF_CONTEXT_DEVICETYPE_ENUM amfDeviceType,
    amf::AMFContext             **context = nullptr
    )
{
    bool AllIsOK = true;

    if (compute1 && *compute1)
    {
        (*compute1)->Release();
        *compute1 = nullptr;
    }

    if (compute2 && *compute2)
    {
        (*compute2)->Release();
        *compute2 = nullptr;
    }

    AMF_RESULT result = g_AMFFactory.Init();
    AMF_RETURN_IF_FAILED(result, L"Factory init failed");

    // Create default CPU AMF context.
    amf::AMFContextPtr contextAMF = nullptr;
    result = g_AMFFactory.GetFactory()->CreateContext(&contextAMF);
    AMF_RETURN_IF_FAILED(result, L"CreateContext failed");

    if (context)
    {
        *context = contextAMF;
        (*context)->Acquire();
    }

    result = contextAMF->SetProperty(AMF_CONTEXT_DEVICE_TYPE, amfDeviceType);
    AMF_RETURN_IF_FAILED(result, L"SetProperty failed");

    amf::AMFComputeFactoryPtr computeFactory = nullptr;

#if !defined ENABLE_METAL
    result = contextAMF->GetOpenCLComputeFactory(&computeFactory);
    AMF_RETURN_IF_FAILED(result, L"GetOpenCLComputeFactory failed");
#else
    amf::AMFContext3Ptr context3(contextAMF);
    AMF_RETURN_IF_FALSE(context3 != nullptr, AMF_NOT_SUPPORTED);

    result = context3->GetMetalComputeFactory(&computeFactory);
    AMF_RETURN_IF_FAILED(result, L"GetMetalComputeFactory failed");
#endif

    amf_int32 deviceCount = computeFactory->GetDeviceCount();

    if (deviceIndex >= deviceCount)
    {
        AMF_RETURN_IF_FAILED(AMF_INVALID_ARG, L"Incorrect deviceIndex");
    }

    amf::AMFComputeDevicePtr computeDevice;
    result = computeFactory->GetDeviceAt(deviceIndex, &computeDevice);
    AMF_RETURN_IF_FAILED(result, L"GetDeviceAt failed");

#if !defined ENABLE_METAL
    result = contextAMF->InitOpenCLEx(computeDevice);
#else
    result = context3->InitMetalEx(computeDevice);
#endif

    AMF_RETURN_IF_FAILED(result, L"Initialization failed");

    if (compute1)
    {
        amf_int64 param = flag1 & 0x0FFFF;
        result = computeDevice->SetProperty(AMFQUEPROPERTY, param);
        AMF_RETURN_IF_FAILED(result, L"SetProperty failed");

        result = computeDevice->CreateCompute(nullptr, compute1);
        AMF_RETURN_IF_FAILED(result, L"CreateCompute 1 failed");
    }

    if (compute2)
    {
        amf_int64 param = flag2 & 0x0FFFF;
        result = computeDevice->SetProperty(AMFQUEPROPERTY, param);
        AMF_RETURN_IF_FAILED(result, L"SetProperty failed");

        result = computeDevice->CreateCompute(nullptr, compute2);
        AMF_RETURN_IF_FAILED(result, L"CreateCompute 2 failed");
    }

    return result;
}

bool GetDeviceFromIndex(int deviceIndex, cl_device_id *device, cl_context *context, cl_device_type clDeviceType)
{

#ifdef _WIN32
    HMODULE GPUUtilitiesDll = NULL;
    GPUUtilitiesDll = LoadLibraryA("GPUUtilities.dll");
    if (NULL == GPUUtilitiesDll)
        return false;

    typedef int(WINAPI * getDeviceAndContextType)(int devIdx, cl_context *pContext, cl_device_id *pDevice, cl_device_type clDeviceType);
    getDeviceAndContextType getDeviceAndContext = nullptr;
    getDeviceAndContext = (getDeviceAndContextType)GetProcAddress(GPUUtilitiesDll, "getDeviceAndContext");
    if (NULL == getDeviceAndContext)
        return false;
#endif

    cl_context clContext = NULL;
    cl_device_id clDevice = NULL;
    //get context of open device via index 0,1,2,...
    getDeviceAndContext(deviceIndex, &clContext, &clDevice, clDeviceType);
    if (NULL == clContext)
        return false;
    if (NULL == clDevice)
        return false;

    *device = clDevice;
    *context = clContext;
    return true;
}

bool CreateCommandQueuesWithCUcount(int deviceIndex, cl_command_queue *pcmdQueue1, cl_command_queue *pcmdQueue2, int Q1CUcount, int Q2CUcount)
{
    cl_int err = 0;
    cl_device_id device = NULL;
    cl_context context = NULL;

    cl_device_partition_property props[] = {
        CL_DEVICE_PARTITION_BY_COUNTS,
        Q2CUcount,
        Q1CUcount,
        CL_DEVICE_PARTITION_BY_COUNTS_LIST_END,
        0
        }; // count order seems reversed!

    GetDeviceFromIndex(deviceIndex, &device, &context, CL_DEVICE_TYPE_CPU); // only implemented for CPU

    cl_device_id outdevices[2] = {NULL, NULL};

    err = clCreateSubDevices(
        device,
        props,
        2,
        outdevices,
        NULL
        );

    *pcmdQueue1 = clCreateCommandQueue(context, outdevices[0], NULL, &err);
    *pcmdQueue2 = clCreateCommandQueue(context, outdevices[1], NULL, &err);

    return err;
}

bool CreateCommandQueuesVIAocl(int deviceIndex, int32_t flag1, cl_command_queue *pcmdQueue1, int32_t flag2, cl_command_queue *pcmdQueue2, cl_device_type clDeviceType)
{
    bool AllIsOK = true;

#ifdef _WIN32
    HMODULE GPUUtilitiesDll = NULL;
    GPUUtilitiesDll = LoadLibraryA("GPUUtilities.dll");
    if (NULL == GPUUtilitiesDll)
        return false;

    typedef int(WINAPI * getDeviceAndContextType)(int devIdx, cl_context *pContext, cl_device_id *pDevice, cl_device_type clDeviceType);
    getDeviceAndContextType getDeviceAndContext = nullptr;
    getDeviceAndContext = (getDeviceAndContextType)GetProcAddress(GPUUtilitiesDll, "getDeviceAndContext");
    if (NULL == getDeviceAndContext)
        return false;

    typedef cl_command_queue(WINAPI * createQueueType)(cl_context context, cl_device_id device, int, int);
    createQueueType createQueue = nullptr;
    createQueue = (createQueueType)GetProcAddress(GPUUtilitiesDll, "createQueue");
    if (NULL == createQueue)
        return false;
#endif

    cl_context clContext = NULL;
    cl_device_id clDevice = NULL;
    //get context of open device via index 0,1,2,...
    getDeviceAndContext(deviceIndex, &clContext, &clDevice, clDeviceType);
    if (NULL == clContext)
        return false;
    if (NULL == clDevice)
        return false;
    clRetainDevice(clDevice);
    clRetainContext(clContext);

    if (NULL != pcmdQueue1)
    { //user requested one queue
        cl_command_queue tempQueue = createQueue(clContext, clDevice, 0, 0);
        if (NULL == tempQueue)
        {
            fprintf(stdout, "createQueue failed to create cmdQueue1 ");
            AllIsOK = false;
        }
        *pcmdQueue1 = tempQueue;
    }

    if (NULL != pcmdQueue2)
    { //user requested second queue
        cl_command_queue tempQueue = createQueue(clContext, clDevice, 0, 0);
        if (NULL == tempQueue)
        {
            fprintf(stdout, "createQueue failed to create cmdQueue2 ");
            AllIsOK = false;
        }
        *pcmdQueue2 = tempQueue;
    }

    if (false == AllIsOK)
    {
        if (NULL != pcmdQueue1)
        {
            if (NULL != *pcmdQueue1)
            {
                clReleaseCommandQueue(*pcmdQueue1);
                *pcmdQueue1 = NULL;
            }
        }
        if (NULL != pcmdQueue2)
        {
            if (NULL != *pcmdQueue2)
            {
                clReleaseCommandQueue(*pcmdQueue2);
                *pcmdQueue2 = NULL;
            }
        }
    }

    if (NULL != clContext)
    {
        clReleaseContext(clContext);
        clContext = NULL;
    }
    if (NULL != clDevice)
    {
        clReleaseDevice(clDevice);
        clDevice = NULL;
    }
    return AllIsOK;
}

#ifndef TAN_NO_OPENCL
bool CreateGpuCommandQueues(int deviceIndex, int32_t flag1, cl_command_queue *pcmdQueue1, int32_t flag2, cl_command_queue *pcmdQueue2)
{
    bool bResult = false;
    switch (CreateCommandQueuesVIAamf(deviceIndex, flag1, pcmdQueue1, flag2, pcmdQueue2, AMF_CONTEXT_DEVICE_TYPE_GPU))
    {
    case AMF_NOT_INITIALIZED:
        bResult = CreateCommandQueuesVIAocl(deviceIndex, 0, pcmdQueue1, 0, pcmdQueue2, CL_DEVICE_TYPE_GPU);
        break;
    default:;
    }
    return bResult;
}

bool CreateCpuCommandQueues(int deviceIndex, int32_t flag1, cl_command_queue *pcmdQueue1, int32_t flag2, cl_command_queue *pcmdQueue2)
{
    bool bResult = false;
    switch (CreateCommandQueuesVIAamf(deviceIndex, flag1, pcmdQueue1, flag2, pcmdQueue2, AMF_CONTEXT_DEVICE_TYPE_CPU))
    {
    case AMF_NOT_INITIALIZED:
        bResult = CreateCommandQueuesVIAocl(deviceIndex, 0, pcmdQueue1, 0, pcmdQueue2, CL_DEVICE_TYPE_CPU);
        break;
    default:;
    }
    return bResult;
}

#else

bool CreateGpuCommandQueues(
    int deviceIndex,
    int32_t flag1,
    amf::AMFCompute **compute1,
    int32_t flag2,
    amf::AMFCompute **compute2,
    amf::AMFContext **context = nullptr
    )
{
    return CreateCommandQueuesVIAamf(deviceIndex, 0, compute1, 0, compute2, AMF_CONTEXT_DEVICE_TYPE_GPU, context);
}

bool CreateCpuCommandQueues(
    int deviceIndex,
    int32_t flag1,
    amf::AMFCompute **compute1,
    int32_t flag2,
    amf::AMFCompute **compute2,
    amf::AMFContext **context = nullptr
    )
{
    return CreateCommandQueuesVIAamf(deviceIndex, 0, compute1, 0, compute2, AMF_CONTEXT_DEVICE_TYPE_CPU, context);
}

bool CreateCommandQueuesWithCUcount(int deviceIndex, amf::AMFCompute** compute1, amf::AMFCompute** compute2, int Q1CUcount, int Q2CUcount)
{
    //todo: implement amf version, currently skipped

    return false;
}

#endif
