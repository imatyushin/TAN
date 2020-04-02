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
///  @file   TANMixerImpl.h
///  @brief  TANMixer interface implementation
///-------------------------------------------------------------------------
#pragma once
#include "TrueAudioNext.h"   //TAN
#include "public/include/core/Context.h"        //AMF
#include "public/include/components/Component.h"//AMF
#include "public/common/PropertyStorageExImpl.h"//AMF

namespace amf
{
    class TANMixerImpl
        : public virtual AMFInterfaceImpl < AMFPropertyStorageExImpl< TANMixer> >
    {
    public:
        typedef AMFInterfacePtr_T<TANMixerImpl> Ptr;

        TANMixerImpl(TANContext *pContextTAN, AMFContext* pContextAMF);
        virtual ~TANMixerImpl(void);

// interface access
        AMF_BEGIN_INTERFACE_MAP
            AMF_INTERFACE_CHAIN_ENTRY(AMFInterfaceImpl< AMFPropertyStorageExImpl <TANMixer> >)
        AMF_END_INTERFACE_MAP

//TANMixer interface
        AMF_RESULT  AMF_STD_CALL Init(
            amf_size buffer_size,
            int num_channels,
            amf::AMFFactory * factory = nullptr
            ) override;
        AMF_RESULT  AMF_STD_CALL Terminate() override;
        TANContext* AMF_STD_CALL GetContext() override { return m_pContextTAN; }

        AMF_RESULT  AMF_STD_CALL    Mix(float* ppBufferInput[],
                                        float* ppBufferOutput
                                        ) override;

#ifndef TAN_NO_OPENCL
        // For disjoint cl_mem input buffers
        AMF_RESULT  AMF_STD_CALL    Mix(cl_mem pBufferInput[],
                                        cl_mem pBufferOutput
                                        ) override;

        // For contigous cl_mem input buffers
        AMF_RESULT  AMF_STD_CALL    Mix(cl_mem pBufferInput,
                                        cl_mem pBufferOutput,
                                        amf_size inputStride
                                        ) override;
#else
        AMF_RESULT  AMF_STD_CALL    Mix(AMFBuffer * pBufferInput[],
                                        AMFBuffer * pBufferOutput
                                        ) override;

        // For contigous cl_mem input buffers
        AMF_RESULT  AMF_STD_CALL    Mix(AMFBuffer * pBufferInput,
                                        AMFBuffer * pBufferOutput,
                                        amf_size inputStride
                                        ) override;
#endif

    protected:
        TANContextPtr               m_pContextTAN;
        AMFContextPtr               m_pContextAMF;
        AMFComputePtr               mAMFCompute;

        AMF_MEMORY_TYPE             m_eOutputMemoryType = AMF_MEMORY_HOST;
        AMFCriticalSection          m_sect;

#ifndef TAN_NO_OPENCL
        cl_command_queue			m_pCommandQueueCl = nullptr;
        cl_context					m_pContextCl = nullptr;
        cl_device_id				m_pDeviceCl = nullptr;

        cl_kernel					m_clMix = nullptr;
#else
        AMFComputeKernelPtr         mMixKernel;
#endif

        /// It defines how many channels can be mixed together by a single call into the MixerMultiBuffer kernel
        const static int            MAX_CHANNELS_TO_MIX_PER_KERNEL_CALL = 16;
    private:
        static bool useSSE2;
        AMF_RESULT	AMF_STD_CALL InitCpu();
        AMF_RESULT	AMF_STD_CALL InitGpu(amf::AMFFactory * factory);
		amf_size m_bufferSize = 0;

#ifndef TAN_NO_OPENCL
		cl_mem m_internalBuff = nullptr;
#else
        AMFBufferPtr mInternalBufferAMF;
#endif

		int m_numChannels = 0;

        bool mInitialized = false;
    };
} //amf
