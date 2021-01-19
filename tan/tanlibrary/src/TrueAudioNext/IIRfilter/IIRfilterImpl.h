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
///  @file   IIRfilterImpl.h
///  @brief  IIRfilter interface implementation
///-------------------------------------------------------------------------
#pragma once
#include "TrueAudioNext.h"	//TAN
#include "public/include/core/Context.h"        //AMF
#include "public/include/components/Component.h"//AMF
#include "public/common/PropertyStorageExImpl.h"

namespace amf
{
#define MAX_CHANNELS	32

    class TANIIRfilterImpl :
        public virtual AMFInterfaceImpl < AMFPropertyStorageExImpl< TANIIRfilter> >    {
    public:
        typedef AMFInterfacePtr_T<TANIIRfilterImpl> Ptr;

        TANIIRfilterImpl(TANContext *pContextTAN);
        virtual ~TANIIRfilterImpl(void);

        // interface access
        AMF_BEGIN_INTERFACE_MAP
            AMF_INTERFACE_CHAIN_ENTRY(AMFInterfaceImpl< AMFPropertyStorageExImpl <TANIIRfilter> >)
            AMF_END_INTERFACE_MAP

            //TANMath interface
            virtual AMF_RESULT  AMF_STD_CALL Init(
            amf_uint32 numInputTaps,
            amf_uint32 numOutputTaps,
			amf_uint32 bufferSizeInSamples,
            amf_uint32 channels);

        virtual AMF_RESULT  AMF_STD_CALL    Terminate();
        virtual TANContext* AMF_STD_CALL    GetContext()	{ return m_pContextTAN; }

        virtual AMF_RESULT AMF_STD_CALL     UpdateIIRResponses(float* ppInputResponse[], float* ppOutputResponse[],
            amf_size inResponseSz, amf_size outResponseSz,
            const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_IIR_CHANNEL_FLAG, can be NULL.
            const amf_uint32 operationFlags // Mask of flags from enum TAN_IIR_OPERATION_FLAG.
            );

        virtual AMF_RESULT  AMF_STD_CALL    Process(float* ppBufferInput[],
            float* ppBufferOutput[],
            amf_size numOfSamplesToProcess,
            const amf_uint32 flagMasks[],    // Masks of flags from enum TAN_IIR_CHANNEL_FLAG, can be NULL.
            amf_size *pNumOfSamplesProcessed // Can be NULL.
            );

		virtual AMF_RESULT  AMF_STD_CALL    ProcessDirect(float* ppBufferInput[],
			float* ppBufferOutput[],
			amf_size numOfSamplesToProcess,
			const amf_uint32 flagMasks[],    // Masks of flags from enum TAN_IIR_CHANNEL_FLAG, can be NULL.
			amf_size *pNumOfSamplesProcessed // Can be NULL.
		);

#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT  AMF_STD_CALL    Process(
            cl_mem ppBufferInput[],
            cl_mem ppBufferOutput[],
            amf_size numOfSamplesToProcess,
            const amf_uint32 flagMasks[],    // Masks of flags from enum TAN_IIR_CHANNEL_FLAG, can be NULL.
            amf_size *pNumOfSamplesProcessed // Can be NULL.
            );
#else
        virtual AMF_RESULT  AMF_STD_CALL    Process(
            const AMFBuffer * ppBufferInput[],
            AMFBuffer * ppBufferOutput[],
            amf_size numOfSamplesToProcess,
            const amf_uint32 flagMasks[],    // Masks of flags from enum TAN_IIR_CHANNEL_FLAG, can be NULL.
            amf_size *pNumOfSamplesProcessed // Can be NULL.
            );
#endif

    protected:

        TANContextPtr               m_pContextTAN;
        AMFComputePtr               m_pDeviceCompute;
		AMFComputePtr               m_pDeviceAMF;

#ifndef TAN_NO_OPENCL

		cl_command_queue			m_pCommandQueueCl = nullptr;
		cl_context					m_pContextCl = nullptr;
		cl_device_id				m_pDeviceCl = nullptr;
		cl_program					m_program = nullptr;
		cl_kernel					m_kernel_IIRfilter = nullptr;
		cl_mem						m_clTempIn = nullptr;
		cl_mem						m_clTempInSubBufs[MAX_CHANNELS] = {nullptr};
		cl_mem						m_clTempOutSubBufs[MAX_CHANNELS] = {nullptr};
		cl_mem						m_clTempOut = nullptr;
		cl_mem						m_clInputTaps = nullptr;
		cl_mem						m_clOutputTaps = nullptr;
		cl_mem						m_clInputHistory = nullptr;
		cl_mem						m_clOutputHistory = nullptr;
		cl_mem						m_clInOutHistPos = nullptr;

#else
#endif

		amf_uint32					m_numSamples = 0;
		amf_uint32					m_bufSize = 0;

        AMF_MEMORY_TYPE             m_eOutputMemoryType = AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN;
        AMFCriticalSection          m_sect;

    private:
        virtual AMF_RESULT  AMF_STD_CALL InitCpu();
        virtual AMF_RESULT  AMF_STD_CALL InitGpu();

#ifndef TAN_NO_OPENCL
		virtual AMF_RESULT  AMF_STD_CALL IIRFilterProcessGPU(cl_mem hisBuf, cl_mem out, amf_size numOfSamplesToProcess, const amf_uint32 flagMasks[], amf_size *pNumOfSamplesProcessed);
#else
#endif

        amf_uint32 m_numInputTaps = 0;
        amf_uint32 m_numOutputTaps = 0;
        amf_uint32 m_channels = 0;
        float **m_inputTaps = nullptr;
        float **m_outputTaps = nullptr;
        float **m_inputHistory = nullptr;
        float **m_outputHistory = nullptr;
        amf_uint32 m_inputHistPos = 0;
        amf_uint32 m_outputHistPos = 0;
		bool m_doProcessOnGpu = false;
    };
} //amf
