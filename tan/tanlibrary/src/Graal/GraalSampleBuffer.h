//
// MIT license
//
// Copyright (c) 2019 Advanced Micro Devices, Inc. All rights reserved.
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

#include "public/include/core/Buffer.h"

#ifndef TAN_NO_OPENCL
  #include <CL/cl.h>
#endif

namespace graal
{

struct GraalSampleBuffer
{
    union
    {
        float **            host;
#ifndef TAN_NO_OPENCL
        cl_mem *            clmem;
#else
        amf::AMFBuffer **   amfBuffers;
#endif
    } buffer = {nullptr};

    amf::AMF_MEMORY_TYPE    type = amf::AMF_MEMORY_UNKNOWN;

    inline bool IsSet() const {return type != amf::AMF_MEMORY_UNKNOWN;}
    inline bool IsHost() const {return type == amf::AMF_MEMORY_HOST;}
    inline bool IsComputeBuffer() const {return IsSet() && !IsHost();}

    inline void ReferHostBuffers(float ** buffers)
    {
        assert(!IsSet());

        type = amf::AMF_MEMORY_HOST;
        buffer.host = buffers;
    }

#ifndef TAN_NO_OPENCL
    inline void ReferCLBuffers(cl_mem * buffers)
    {
        assert(!IsSet());
        assert(buffers);                                                                     //todo: think about host
        assert(type != amf::AMF_MEMORY_UNKNOWN && type != amf::AMF_MEMORY_HOST);

        type = amf::AMF_MEMORY_OPENCL;
        buffer.clmem = buffers;
    }
#else
    inline void ReferAMFBuffers(amf::AMFBuffer ** amfBuffers)
    {
        assert(!IsSet());
        assert(amfBuffers);                                                                     //todo: think about host
        assert(amfBuffers[0]->GetMemoryType() != amf::AMF_MEMORY_UNKNOWN && amfBuffers[0]->GetMemoryType() != amf::AMF_MEMORY_HOST);

        type = amfBuffers[0]->GetMemoryType();
        buffer.amfBuffers = amfBuffers;
    }
#endif
};

}