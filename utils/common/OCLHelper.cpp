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

#include "OCLHelper.h"
#include "StringUtility.h"

#ifndef TAN_NO_OPENCL

bool GetOclKernel
(
    cl_kernel &                 resultKernel,
    const amf::AMFComputePtr &  device,
    const cl_command_queue      c_queue,

    const std::string &         kernelID,
    const std::string &         kernelSource,
    size_t                      kernelSourceSize,
    const std::string &         kernelName,

    const std::string &         comp_options
)
{
    resultKernel = nullptr;

    ///First: Try to load kernel via AMF interface
    if(device)
    {
        std::wstring wKernelId(toWideString(kernelID));
        amf::AMF_KERNEL_ID amfKernelID = -1;

        amf::AMFPrograms *pPrograms(nullptr);
        g_AMFFactory.GetFactory()->GetPrograms(&pPrograms);

        if(pPrograms)
        {
            AMF_RESULT result(
                pPrograms->RegisterKernelSource(
                    &amfKernelID,
                    wKernelId.c_str(),
                    kernelName.c_str(),
                    kernelSourceSize,
                    reinterpret_cast<const amf_uint8 *>(kernelSource.c_str()),
                    comp_options.c_str()
                    )
                );

            if(AMF_OK == result)
            {
                amf::AMFComputeKernel* pAMFComputeKernel = nullptr;
                device->GetKernel(amfKernelID, &pAMFComputeKernel);

                if(nullptr != pAMFComputeKernel)
                {
                    resultKernel = (cl_kernel)pAMFComputeKernel->GetNative();

                    return true;
                }
            }
        }
    }

    //second: try to load kernel via OpenCL interface
    if(c_queue)
    {
        cl_kernel ret = 0;

        std::string key = kernelID + "." + comp_options;

        cl_context context = nullptr;
        size_t param_value_size_ret = 0;
        clGetCommandQueueInfo(c_queue, CL_QUEUE_CONTEXT, sizeof(context), &context, &param_value_size_ret);

        const char *source = &kernelSource.front();

        cl_int status = CL_SUCCESS;
        cl_program program = clCreateProgramWithSource(
            context,
            1,
            &source,
            &kernelSourceSize,
            &status
            );

        if(CL_SUCCESS == status)
        {
            cl_device_id device_id = 0;
            status = clGetCommandQueueInfo(
                c_queue,
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
                    resultKernel = clCreateKernel(program, kernelName.c_str(), &status);

                    if((CL_SUCCESS == status) && resultKernel)
                    {
                        return true;
                    }
                }
                else
                {
                    if(CL_BUILD_PROGRAM_FAILURE == status)
                    {
                        cl_int logStatus(0);
                        char *buildLog = nullptr;
                        size_t buildLogSize = 0;

                        logStatus = clGetProgramBuildInfo(
                            program,
                            device_id,
                            CL_PROGRAM_BUILD_LOG,
                            buildLogSize,
                            buildLog,
                            &buildLogSize
                            );

                        buildLog = (char*)malloc(buildLogSize);
                        memset(buildLog, 0, buildLogSize);
                        logStatus = clGetProgramBuildInfo(
                            program,
                            device_id,
                            CL_PROGRAM_BUILD_LOG,
                            buildLogSize,
                            buildLog,
                            &buildLogSize
                            );
                        fprintf(stderr, buildLog);
                        free(buildLog);
                    }
                }
            }
        }
    }

    return false;
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
    cl_int error = CL_SUCCESS;
    
    cl_event event = clCreateUserEvent(context, &error);
    if(error != CL_SUCCESS)
    {
        return error;
    }

    error = clEnqueueFillBuffer(
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
    if(error != CL_SUCCESS)
    {
        return error;
    }
    
    error = clWaitForEvents(1, &event);

    return error;
}

#else

bool GetOclKernel
(
    amf::AMFComputeKernelPtr &  resultKernel,
    const amf::AMFComputePtr &  compute,

    const std::string &         kernelID,
    const std::string &         kernelSource,
    size_t                      kernelSourceSize,
    const std::string &         kernelName,

    const std::string &         comp_options,
    amf::AMFFactory *           factory
)
{
    resultKernel = nullptr;

    ///First: Try to load kernel via AMF interface
    if(compute)
    {
        std::wstring wKernelId(toWideString(kernelID));
        amf::AMF_KERNEL_ID amfKernelID = -1;

        if(!factory)
        {
            factory = g_AMFFactory.GetFactory();
        }

        amf::AMFPrograms *pPrograms(nullptr);
        factory->GetPrograms(&pPrograms);

        if(pPrograms)
        {
            AMF_RESULT result(
                pPrograms->RegisterKernelSource(
                    &amfKernelID,
                    wKernelId.c_str(),
                    kernelName.c_str(),
                    kernelSourceSize,
                    reinterpret_cast<const amf_uint8 *>(kernelSource.c_str()),
                    comp_options.c_str()
                    )
                );

            if(AMF_OK == result)
            {
                amf::AMFComputeKernel* pAMFComputeKernel = nullptr;
                compute->GetKernel(amfKernelID, &pAMFComputeKernel);

                if(nullptr != pAMFComputeKernel)
                {
                    resultKernel = pAMFComputeKernel;

                    return true;
                }
            }
        }
    }

    return false;
}

#endif
