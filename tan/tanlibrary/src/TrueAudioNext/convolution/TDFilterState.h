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
///-------------------------------------------------------------------------
///  @file   ConvolutionImpl.h
///  @brief  TANConvolutionImpl interface implementation
///-------------------------------------------------------------------------
#pragma once
#include "TrueAudioNext.h"   //TAN
#include "public/include/core/Context.h"        //AMF
#include "public/include/core/Buffer.h"         //AMF
#include "public/include/components/Component.h"//AMF
#include "public/common/PropertyStorageExImpl.h"//AMF

#include "GraalConv.hpp"
#include "GraalConv_clFFT.hpp"
//#include "tanlibrary/src/Graal2/GraalWrapper.h"

#include "Debug.h"

#define SAFE_ARR_DELETE(x) {if(x){ delete[] x;} (x) = nullptr; }

namespace amf
{
    typedef struct _tdFilterState
    {
        float **m_Filter = nullptr;

#ifndef TAN_NO_OPENCL
        cl_mem *m_clFilter = nullptr;
        cl_mem *m_clTemp = nullptr;
#else
        //todo: use vectors
        amf::AMFBuffer ** amfFilter = nullptr;
        amf::AMFBuffer ** amfTemp = nullptr;
#endif

        int *firstNz = nullptr;
        int *lastNz = nullptr;
        float **m_SampleHistory = nullptr;

#ifndef TAN_NO_OPENCL
        cl_mem *m_clSampleHistory = nullptr;
#else
        std::vector<amf::AMFBuffer *> amfSampleHistory;
#endif
        int *m_sampHistPos = nullptr;

        void SetupHost(size_t channelsCount)
        {
            m_Filter = new float *[channelsCount];
            std::memset(m_Filter, 0, sizeof(float *) * channelsCount);

			m_SampleHistory = new float*[channelsCount];
            std::memset(m_SampleHistory, 0, sizeof(float *) * channelsCount);

			m_sampHistPos = new int[channelsCount];
            std::memset(m_sampHistPos, 0, sizeof(int) * channelsCount);

			firstNz = new int[channelsCount];
            std::memset(firstNz, 0, sizeof(int) * channelsCount);

			lastNz = new int[channelsCount];
            std::memset(lastNz, 0, sizeof(int) * channelsCount);
        }

        void SetupHostData(size_t index, size_t length)
        {
            m_Filter[index] = new float[length];
            std::memset(m_Filter[index], 0, sizeof(float) * length);

            m_SampleHistory[index] = new float[length];
            std::memset(m_SampleHistory[index], 0, sizeof(float) * length);

			m_sampHistPos[index] = 0;
			firstNz[index] = 0;
			lastNz[index] = 0;
        }

        void FreeHostData(size_t index)
        {
            SAFE_ARR_DELETE(m_Filter[index]);
            SAFE_ARR_DELETE(m_SampleHistory[index]);

            m_sampHistPos[index] = 0;
			firstNz[index] = 0;
			lastNz[index] = 0;
        }

#ifndef TAN_NO_OPENCL
        void SetupCL(size_t channelsCount)
        {
            m_clFilter = new cl_mem[channelsCount];
            std::memset(m_clFilter, 0, sizeof(cl_mem) * channelsCount);

            m_clTemp = new cl_mem[channelsCount];
            std::memset(m_clTemp, 0, sizeof(cl_mem) * channelsCount);

            m_clSampleHistory = new cl_mem[channelsCount];
            std::memset(m_clSampleHistory, 0, sizeof(cl_mem) * channelsCount);
        }

        AMF_RESULT SetupCLData(cl_context context, size_t index, size_t length)
        {
            cl_int returnCode(0);

            m_clFilter[index] = clCreateBuffer(context, CL_MEM_READ_WRITE, length * sizeof(float), nullptr, &returnCode);
            AMF_RETURN_IF_CL_FAILED(returnCode);

            m_clTemp[index] = clCreateBuffer(context, CL_MEM_READ_WRITE, length * sizeof(float), nullptr, &returnCode);
            AMF_RETURN_IF_CL_FAILED(returnCode);

            m_clSampleHistory[index] = clCreateBuffer(context, CL_MEM_READ_WRITE, length * sizeof(float), nullptr, &returnCode);
            AMF_RETURN_IF_CL_FAILED(returnCode);
        }

        AMF_RESULT FreeCLData(size_t index)
        {
            assert(m_clFilter[index] && m_clTemp[index] && m_clSampleHistory[index]);

            DBG_CLRELEASE_MEMORYOBJECT(m_clFilter[index]);
            DBG_CLRELEASE_MEMORYOBJECT(m_clTemp[index]);
            DBG_CLRELEASE_MEMORYOBJECT(m_clSampleHistory[index]);
        }

        void DeallocateCL()
        {
            if(m_clFilter)
            {
                delete [] m_clFilter, m_clFilter = nullptr;
            }

            if(m_clTemp)
            {
                delete [] m_clTemp, m_clTemp = nullptr;
            }

            if(m_clSampleHistory)
            {
                delete [] m_clSampleHistory, m_clSampleHistory = nullptr;
            }
        }
#else
        void SetupAMF(size_t channelsCount)
        {
            amfFilter = new amf::AMFBuffer *[channelsCount];
            std::memset(amfFilter, 0, sizeof(amf::AMFBuffer *) * channelsCount);

            amfTemp = new amf::AMFBuffer *[channelsCount];
            std::memset(amfTemp, 0, sizeof(amf::AMFBuffer *) * channelsCount);

            amfSampleHistory.resize(channelsCount);
        }

        AMF_RESULT SetupAMFData(AMFContext * context, size_t index, size_t length)
        {
            AMF_RETURN_IF_FAILED(
                context->AllocBuffer(
#ifndef ENABLE_METAL
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
                    ,
                    length * sizeof(float),
                    &amfFilter[index]
                    )
                );

            AMF_RETURN_IF_FAILED(
                context->AllocBuffer(
#ifndef ENABLE_METAL
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
                    ,
                    length * sizeof(float),
                    &amfTemp[index]
                    )
                );

            AMF_RETURN_IF_FAILED(
                context->AllocBuffer(
#ifndef ENABLE_METAL
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
                    ,
                    length * sizeof(float),
                    &amfSampleHistory[index]
                    )
                );

            return AMF_OK;
        }

        void FreeAMFData(size_t index)
        {
            assert(amfFilter[index] && amfTemp[index] && amfSampleHistory[index]);

            amfFilter[index] = nullptr;
            amfTemp[index] = nullptr;
            amfSampleHistory[index] = nullptr;
        }

        void DeallocateAMF()
        {
            if(amfFilter)
            {
                delete [] amfFilter, amfFilter = nullptr;
            }

            if(amfTemp)
            {
                delete [] amfTemp, amfTemp = nullptr;
            }

            amfSampleHistory.resize(0);
        }
#endif
    } tdFilterState;
}