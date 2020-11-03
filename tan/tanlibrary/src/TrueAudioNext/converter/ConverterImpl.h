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
///-------------------------------------------------------------------------
///  @file   TANConverterImpl.h
///  @brief  TANConverter interface implementation
///-------------------------------------------------------------------------
#pragma once
#include "TrueAudioNext.h"   //TAN
#include "public/include/core/Context.h"        //AMF
#include "public/include/components/Component.h"//AMF
#include "public/common/PropertyStorageExImpl.h"//AMF

namespace amf
{
    class TANConverterImpl
        : public virtual AMFInterfaceImpl < AMFPropertyStorageExImpl< TANConverter> >
    {
    public:
        typedef AMFInterfacePtr_T<TANConverterImpl> Ptr;

        TANConverterImpl(TANContext *pContextTAN, AMFContext* pContextAMF);
        virtual ~TANConverterImpl(void);

// interface access
        AMF_BEGIN_INTERFACE_MAP
            AMF_INTERFACE_CHAIN_ENTRY(AMFInterfaceImpl< AMFPropertyStorageExImpl <TANConverter> >)
        AMF_END_INTERFACE_MAP

//TANConverter interface
        AMF_RESULT  AMF_STD_CALL Init() override;
        AMF_RESULT  AMF_STD_CALL Terminate() override;
        TANContext* AMF_STD_CALL GetContext() override { return m_pContextTAN; }

        AMF_RESULT  AMF_STD_CALL    Convert(short* inputBuffer, amf_size inputStep,
                                            amf_size numOfSamplesToProcess,
                                            float* outputBuffer, amf_size outputStep,
                                            float conversionGain) override;
        AMF_RESULT  AMF_STD_CALL    Convert(float* inputBuffer, amf_size inputStep,
                                            amf_size numOfSamplesToProcess,
                                            short* outputBuffer, amf_size outputStep,
                                            float conversionGain, bool* outputClipped = NULL) override;

        AMF_RESULT  AMF_STD_CALL    Convert(short** inputBuffers, amf_size inputStep,
                                            amf_size numOfSamplesToProcess,
                                            float** outputBuffers, amf_size outputStep,
                                            float conversionGain, int count) override;

        AMF_RESULT  AMF_STD_CALL    Convert(float** inputBuffers, amf_size inputStep,
                                            amf_size numOfSamplesToProcess,
                                            short** outputBuffers, amf_size outputStep,
                                            float conversionGain, int count, bool* outputClipped = NULL) override;

#ifndef TAN_NO_OPENCL

		AMF_RESULT  AMF_STD_CALL    Convert(cl_mem inputBuffer, amf_size inputStep,
											amf_size inputOffset,TAN_SAMPLE_TYPE inputType,

                                            cl_mem outputBuffer, amf_size outputOffset,
                                            amf_size outputStep, TAN_SAMPLE_TYPE outputType,

                                            amf_size numOfSamplesToProcess,
                                            float conversionGain, bool* outputClipped = NULL) override;

		AMF_RESULT  AMF_STD_CALL    Convert(cl_mem* inputBuffers, amf_size inputStep,
											amf_size* inputOffsets, TAN_SAMPLE_TYPE inputType,

											cl_mem* outputBuffers, amf_size outputStep,
											amf_size* outputOffsets, TAN_SAMPLE_TYPE outputType,

                                            amf_size numOfSamplesToProcess,
                                            float conversionGain,

                                            int count, bool* outputClipped = NULL) override;

#else

        AMF_RESULT  AMF_STD_CALL    Convert(AMFBuffer * inputBuffer,
                                            amf_size inputStep,
                                            amf_size inputOffset,
                                            TAN_SAMPLE_TYPE inputType,

                                            AMFBuffer * outputBuffer,
                                            amf_size outputStep,
                                            amf_size outputOffset,
                                            TAN_SAMPLE_TYPE outputType,

                                            amf_size numOfSamplesToProcess,
                                            float conversionGain,
                                            bool* outputClipped = nullptr
                                            ) override;

        AMF_RESULT  AMF_STD_CALL    Convert(
                                            AMFBuffer ** inputBuffers,
                                            amf_size inputStep,
                                            amf_size* inputOffsets,
                                            TAN_SAMPLE_TYPE inputType,

                                            AMFBuffer ** outputBuffers,
                                            amf_size outputStep,
                                            amf_size* outputOffsets,
                                            TAN_SAMPLE_TYPE outputType,

                                            amf_size numOfSamplesToProcess,
                                            float conversionGain,

                                            int count,
                                            bool* outputClipped = nullptr
                                            ) override;

#endif

    protected:
        TANContextPtr               m_pContextTAN;
        AMFContextPtr               m_pContextAMF;
        AMFComputePtr               m_pDeviceAMF;

        AMFComputeKernelPtr         m_pKernelCopy;
        AMF_MEMORY_TYPE             m_eOutputMemoryType;
        AMFCriticalSection          m_sect;

#ifndef TAN_NO_OPENCL
        cl_command_queue			m_pQueueCl = nullptr;
        cl_context					m_pContextCl = nullptr;
        cl_device_id				m_pDeviceCl = nullptr;

        cl_program					m_program = nullptr;

		cl_kernel					m_clkFloat2Short = nullptr;
		cl_kernel					m_clkShort2Short = nullptr;
		cl_kernel					m_clkFloat2Float = nullptr;
		cl_kernel					m_clkShort2Float = nullptr;

        cl_mem                      m_overflowBuffer = NULL;

#else

        amf::AMFComputePtr          mQueueAMF;

        amf::AMFComputeKernelPtr    mFloat2Short;
        amf::AMFComputeKernelPtr    mShort2Short;
        amf::AMFComputeKernelPtr    mFloat2Float;
        amf::AMFComputeKernelPtr    mShort2Float;

        amf::AMFBufferPtr           mOverflowBuffer;
#endif

    private:
        static bool useSSE2;
        AMF_RESULT	AMF_STD_CALL InitCpu();
        AMF_RESULT	AMF_STD_CALL InitGpu();
        AMF_RESULT	AMF_STD_CALL ConvertGpu(amf_handle inputBuffer,
                                            amf_size inputOffset,
                                            amf_size inputStep,
                                            TAN_SAMPLE_TYPE inputType,

                                            amf_handle outputBuffer,
                                            amf_size outputOffset,
                                            amf_size outputStep,
                                            TAN_SAMPLE_TYPE outputType,

                                            amf_size numOfSamplesToProcess,
                                            float conversionGain,
                                            bool* outputClipped = NULL);
    };
} //amf
