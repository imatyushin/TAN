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

#include "public/common/AMFFactory.h"

#ifndef TAN_NO_OPENCL

#include <CL/cl.h>

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
);

cl_int FixedEnqueueFillBuffer(
    cl_context                  context,
    cl_command_queue            command_queue,
    cl_mem                      buffer,
    const void *                pattern,
    size_t                      pattern_size,
    size_t                      offset,
    size_t                      size
    );

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
    amf::AMFFactory *           factory = nullptr
);

#endif