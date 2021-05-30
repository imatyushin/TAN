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
#include "public/common/AMFFactoryHelper.h"
#include "public/common/TraceAdapter.h"

#include "OCLHelper.h"
#include "StringUtility.h"

#include <string>
#include <iostream>

#define AMF_FACILITY L"OCL_HELPER"

amf::AMFComputeKernel * GetOclKernel(
    const amf::AMFComputePtr &  compute,

    const std::string &         sourceFileName,
    const amf_uint8 *           sourceData,
    amf_size                    sourceSize,
    const std::string &         kernelName,

    const std::string &         options,
    amf::AMFFactory *           factory
    )
{
    assert(compute);

    if(!factory)
    {
        factory = g_AMFFactory.GetFactory();
    }
    AMF_RETURN_IF_FALSE(factory != nullptr, nullptr);

    amf::AMFPrograms *programs(nullptr);
    AMF_RETURN_IF_FALSE(amf::AMFResultIsOK(factory->GetPrograms(&programs)) && programs, nullptr);

    amf::AMF_KERNEL_ID kernelID = -1;

    AMF_RETURN_IF_FALSE(
        amf::AMFResultIsOK(
            programs->RegisterKernelSource(
                &kernelID,
                toWideString(sourceFileName).c_str(),
                kernelName.c_str(),
                sourceSize,
                sourceData,
                options.c_str()
                )
            ),
        nullptr
        );

    amf::AMFComputeKernel *kernel(nullptr);
    if(amf::AMFResultIsOK(compute->GetKernel(kernelID, &kernel)) && kernel)
    {
        return kernel;
    }

    return nullptr;
}

#ifndef TAN_NO_OPENCL

cl_kernel GetOclKernel
(
    const amf::AMFComputePtr &  compute,
    const cl_command_queue      queue,

    const std::string &         kernelID,
    const amf_uint8 *           sourceData,
    amf_size                    sourceSize,
    const std::string &         kernelName,

    const std::string &         comp_options,

    amf::AMFFactory *           factory
)
{
    std::string key = kernelID + "." + comp_options;

    cl_context context = nullptr;
    size_t param_value_size_ret = 0;
    clGetCommandQueueInfo(queue, CL_QUEUE_CONTEXT, sizeof(context), &context, &param_value_size_ret);

    const char *source = (const char *)sourceData;

    cl_int status = CL_SUCCESS;
    cl_program program = clCreateProgramWithSource(
        context,
        1,
        &source,
        &sourceSize,
        &status
        );

    if(CL_SUCCESS == status)
    {
        cl_device_id device_id = 0;
        status = clGetCommandQueueInfo(
            queue,
            CL_QUEUE_DEVICE,
            sizeof(device_id),
            &device_id,
            &param_value_size_ret
            );

        if(CL_SUCCESS == status)
        {
            status = clBuildProgram(program, 1, &device_id, comp_options.c_str(), NULL, NULL);

            if(CL_SUCCESS == status)
            {
                cl_kernel resultKernel = clCreateKernel(program, kernelName.c_str(), &status);

                if((CL_SUCCESS == status) && resultKernel)
                {
                    return resultKernel;
                }
            }
            else if(CL_BUILD_PROGRAM_FAILURE == status)
            {
                size_t buildLogSize = 1024;
                std::string buildLog(buildLogSize, 0);

                cl_int logStatus = clGetProgramBuildInfo(
                    program,
                    device_id,
                    CL_PROGRAM_BUILD_LOG,
                    buildLogSize,
                    &buildLog.front(),
                    &buildLogSize
                    );

                if(CL_SUCCESS == logStatus)
                {
                    std::cerr << std::endl << buildLog << std::endl;
                }
            }
        }
    }

    return nullptr;
}

cl_int FixedEnqueueFillBuffer(
    cl_context                  context,
    cl_command_queue            command_queue,
    cl_mem                      buffer,
    const void *                pattern,
    size_t                      pattern_size,
    size_t                      offset,
    size_t                      size
    )
{
    cl_int returnCode = CL_SUCCESS;

    cl_event event = clCreateUserEvent(context, &returnCode);
    if(returnCode != CL_SUCCESS)
    {
        return returnCode;
    }

    returnCode = clEnqueueFillBuffer(
        command_queue,
        buffer,
        pattern,
        pattern_size,
        offset,
        size,
        0,
        nullptr,
        &event
        );
    if(returnCode != CL_SUCCESS)
    {
        return returnCode;
    }

    returnCode = clWaitForEvents(1, &event);

    return returnCode;
}
#endif
