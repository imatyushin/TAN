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
///  @file   MathImpl.h
///  @brief  TANMath interface implementation
///-------------------------------------------------------------------------
#pragma once

#ifdef USE_IPP
  #include <ipp.h>
  #include <ippcore_tl.h>
  #include <ippversion.h>
#endif

#include "TrueAudioNext.h"	//TAN
#include "public/include/core/Factory.h"
#include "public/include/core/Context.h"        //AMF
#include "public/include/components/Component.h"//AMF
#include "public/common/PropertyStorageExImpl.h"

namespace amf
{
    class TANMathImpl :
        public virtual AMFInterfaceImpl < AMFPropertyStorageExImpl< TANMath> >    {

    public:
        typedef AMFInterfacePtr_T<TANMathImpl> Ptr;

		TANMathImpl(TANContext *pContextTAN, bool useConvQueue);
		virtual ~TANMathImpl(void);

        static bool useAVX256;
		static bool useAVX512;

        // interface access
        AMF_BEGIN_INTERFACE_MAP
            AMF_INTERFACE_CHAIN_ENTRY(AMFInterfaceImpl< AMFPropertyStorageExImpl <TANMath> >)
        AMF_END_INTERFACE_MAP

        //TANMath interface
        virtual AMF_RESULT  AMF_STD_CALL Init() override;
        virtual AMF_RESULT  AMF_STD_CALL Terminate() override;
        virtual TANContext* AMF_STD_CALL GetContext() override { return m_pContextTAN; }

        virtual AMF_RESULT ComplexMultiplication(	const float* const inputBuffers1[],
                                                    const float* const inputBuffers2[],
                                                    float *outputBuffers[],
                                                    amf_uint32 channels,
                                                    amf_size countOfComplexNumbers) override;

#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT ComplexMultiplication(	const cl_mem inputBuffers1[],
                                                    const amf_size buffers1OffsetInSamples[],
                                                    const cl_mem inputBuffers2[],
                                                    const amf_size buffers2OffsetInSamples[],
                                                    cl_mem outputBuffers[],
                                                    const amf_size outputBuffersOffsetInSamples[],
                                                    amf_uint32 channels,
                                                    amf_size countOfComplexNumbers) override;
#else

        virtual AMF_RESULT ComplexMultiplication(	const AMFBuffer * inputBuffers1[],
                                                    const amf_size buffers1OffsetInSamples[],
                                                    const AMFBuffer * inputBuffers2[],
                                                    const amf_size buffers2OffsetInSamples[],
                                                    AMFBuffer * outputBuffers[],
                                                    const amf_size outputBuffersOffsetInSamples[],
                                                    amf_uint32 channels,
                                                    amf_size countOfComplexNumbers) override;
#endif

		virtual AMF_RESULT PlanarComplexMultiplyAccumulate(
                                                    const float* const inputBuffers1[],
													const float* const inputBuffers2[],
													float *accumbuffers[],
													amf_uint32 channels,
													amf_size numOfSamplesToProcess,
													amf_uint riPlaneSpacing) override;

        virtual AMF_RESULT ComplexMultiplyAccumulate(
                                                    const float* const inputBuffers1[],
													const float* const inputBuffers2[],
													float *accumbuffers[],
													amf_uint32 channels,
													amf_size numOfSamplesToProcess) override;

#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT ComplexMultiplyAccumulate(
                                                    const cl_mem inputBuffers1,
													const cl_mem inputBuffers2,
													cl_mem accumBuffers,
													amf_uint32 channels,
                                                    amf_size countOfComplexNumbers,
													amf_size inputOffset1,
													amf_size inputOffset2,
													amf_size inputStride1,
													amf_size inputStride2) override;
#else
        virtual AMF_RESULT ComplexMultiplyAccumulate(
                                                    const AMFBuffer * inputBuffers1,
													const AMFBuffer * inputBuffers2,
													AMFBuffer * accumBuffers,
													amf_uint32 channels,
                                                    amf_size countOfComplexNumbers,
													amf_size inputOffset1,
													amf_size inputOffset2,
													amf_size inputStride1,
													amf_size inputStride2) override;
#endif

#ifdef USE_IPP
		virtual AMF_RESULT IPPComplexMultiplyAccumulate(
                                                    const float* const inputBuffers1[],
													const float* const inputBuffers2[],
													float *accumbuffers[],
													float *workbuffer[],
													amf_uint32 channels,
													amf_size numOfSamplesToProcess) override;
#endif

        virtual AMF_RESULT ComplexDivision(	        const float* const inputBuffers1[],
                                                    const float* const inputBuffers2[],
                                                    float *outputBuffers[],
                                                    amf_uint32 channels,
                                                    amf_size numOfSamplesToProcess) override;
#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT ComplexDivision(
                                                    const cl_mem inputBuffers1[],
                                                    const amf_size buffers1OffsetInSamples[],
                                                    const cl_mem inputBuffers2[],
                                                    const amf_size buffers2OffsetInSamples[],
                                                    cl_mem outputBuffers[],
                                                    const amf_size outputBuffersOffsetInSamples[],
                                                    amf_uint32 channels,
                                                    amf_size countOfComplexNumbers) override;
#else
        virtual AMF_RESULT ComplexDivision(	        const AMFBuffer * inputBuffers1[],
                                                    const amf_size buffers1OffsetInSamples[],
                                                    const AMFBuffer * inputBuffers2[],
                                                    const amf_size buffers2OffsetInSamples[],
                                                    AMFBuffer * outputBuffers[],
                                                    const amf_size outputBuffersOffsetInSamples[],
                                                    amf_uint32 channels,
                                                    amf_size countOfComplexNumbers) override;
#endif

    protected:
        virtual AMF_RESULT ComplexMultiplication(
        	const float inputBuffer1[],
            const float inputBuffer2[],
            float outputBuffer[],
            amf_size countOfComplexNumbers);

#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT ComplexMultiplication(
            const cl_mem inputBuffer1,
            const amf_size buffer1OffsetInSamples,
            const cl_mem inputBuffer2,
            const amf_size buffer2OffsetInSamples,
            cl_mem outputBuffer,
            const amf_size outputBufferOffsetInSamples,
            amf_size countOfComplexNumbers);
#else
        virtual AMF_RESULT ComplexMultiplication(
            const AMFBuffer * inputBuffer1,
            const amf_size buffer1OffsetInSamples,
            const AMFBuffer * inputBuffer2,
            const amf_size buffer2OffsetInSamples,
            AMFBuffer * outputBuffer,
            const amf_size outputBufferOffsetInSamples,
            amf_size countOfComplexNumbers);
#endif

        virtual AMF_RESULT ComplexMultiplyAccumulate(
			const float inputBuffer1[],
			const float inputBuffer2[],
			float accumBuffer[],
            amf_size countOfComplexNumbers);

#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT ComplexMultiplyAccumulate(
			const cl_mem inputBuffer1,
			const amf_size buffer1OffsetInSamples,
			const cl_mem inputBuffer2,
			const amf_size buffer2OffsetInSamples,
			cl_mem accumBuffer,
			const amf_size accumBufferOffsetInSamples,
            amf_size countOfComplexNumbers);
#else
        virtual AMF_RESULT ComplexMultiplyAccumulate(
			const AMFBuffer * inputBuffer1,
			const amf_size buffer1OffsetInSamples,
			const AMFBuffer * inputBuffer2,
			const amf_size buffer2OffsetInSamples,
			AMFBuffer * accumBuffer,
			const amf_size accumBufferOffsetInSamples,
            amf_size countOfComplexNumbers);
#endif

        virtual AMF_RESULT ComplexDivision(
            const float inputBuffer1[],
            const float inputBuffer2[],
            float outputBuffer[],
            amf_size countOfComplexNumbers);

#ifndef TAN_NO_OPENCL
        virtual AMF_RESULT ComplexDivision(
            const cl_mem inputBuffer1,
            const amf_size buffer1OffsetInSamples,
            const cl_mem inputBuffer2,
            const amf_size buffer2OffsetInSamples,
            cl_mem outputBuffer,
            const amf_size outputBufferOffsetInSamples,
            amf_size countOfComplexNumbers);
#else
        virtual AMF_RESULT ComplexDivision(
            const AMFBuffer * inputBuffer1,
            const amf_size buffer1OffsetInSamples,
            const AMFBuffer * inputBuffer2,
            const amf_size buffer2OffsetInSamples,
            AMFBuffer * outputBuffer,
            const amf_size outputBufferOffsetInSamples,
            amf_size countOfComplexNumbers);
#endif

    protected:
        TANContextPtr               m_pContextTAN;
        AMFComputePtr               m_pDeviceCompute;

#ifndef TAN_NO_OPENCL
        cl_kernel			        m_pKernelComplexDiv = nullptr;
        cl_kernel			        m_pKernelComplexMul = nullptr;
		cl_kernel			        m_pKernelComplexSum = nullptr;
        cl_kernel			        m_pKernelComplexMulAccum = nullptr;
#else
        AMFComputeKernelPtr         mKernelComplexDiv;
		AMFComputeKernelPtr         mKernelComplexMul;
		AMFComputeKernelPtr         mKernelComplexSum;
        AMFComputeKernelPtr         mKernelComplexMulAccum;
#endif

        AMF_MEMORY_TYPE             m_eOutputMemoryType = AMF_MEMORY_HOST;

        AMFCriticalSection          m_sect;

#ifndef TAN_NO_OPENCL

		// multiply accumulate internal buffer
		cl_mem	                    m_pInternalSwapBuffer1_MulAccu = nullptr;
		cl_mem	                    m_pInternalSwapBuffer2_MulAccu = nullptr;
		cl_mem	                    m_pInternalBufferIn1_MulAccu = nullptr;
		cl_mem	                    m_pInternalBufferIn2_MulAccu = nullptr;
		cl_mem	                    m_pInternalBufferOut_MulAccu = nullptr;

#else

        AMFBufferPtr                m_pInternalBufferOut_MulAccu;

#endif

		amf_size                    m_iInternalSwapBuffer1Size_MulAccu = 0;
		amf_size                    m_iInternalSwapBuffer2Size_MulAccu = 0;
       	amf_size                    m_iInternalBufferIn1Size_MulAccu = 0;
		amf_size                    m_iInternalBufferIn2Size_MulAccu = 0;
		amf_size                    m_iInternalBufferOutSize_MulAccu = 0;

        amf_uint32                  m_gpuMultiplicationRunNum = 0;
        amf_uint32                  m_gpuDivisionRunNum = 0;

#ifndef TAN_NO_OPENCL
		// Division internal buffer
		cl_mem	m_pInternalBufferIn1_Division = nullptr;
		cl_mem	m_pInternalBufferIn2_Division = nullptr;
		cl_mem	m_pInternalBufferOut_Division = nullptr;
#else
        // Division internal buffer
		AMFBufferPtr m_pInternalBufferIn1_Division;
		AMFBufferPtr m_pInternalBufferIn2_Division;
		AMFBufferPtr m_pInternalBufferOut_Division;
#endif

		amf_size m_iInternalBufferIn1Size_Division = 0;
		amf_size m_iInternalBufferIn2Size_Division = 0;
		amf_size m_iInternalBufferOutSize_Division = 0;

#ifndef TAN_NO_OPENCL
		// Multiplication internal buffer
		cl_mem	m_pInternalBufferIn1_Multiply = nullptr;
		cl_mem	m_pInternalBufferIn2_Multiply = nullptr;
		cl_mem	m_pInternalBufferOut_Multiply = nullptr;
#else
        // Multiplication internal buffer
		AMFBufferPtr m_pInternalBufferIn1_Multiply;
		AMFBufferPtr m_pInternalBufferIn2_Multiply;
		AMFBufferPtr m_pInternalBufferOut_Multiply;
#endif

		amf_size m_iInternalBufferIn1Size_Multiply = 0;
		amf_size m_iInternalBufferIn2Size_Multiply = 0;
		amf_size m_iInternalBufferOutSize_Multiply = 0;

    private:
		bool                m_useConvQueue = false;

#ifndef TAN_NO_OPENCL
		cl_command_queue	m_clQueue = nullptr;
#else
        amf::AMFComputePtr  mQueue;
#endif

        virtual AMF_RESULT  AMF_STD_CALL InitCpu();
        virtual AMF_RESULT  AMF_STD_CALL InitGpu();

#ifndef TAN_NO_OPENCL
		AMF_RESULT AdjustInternalBufferSize(
			cl_mem* _buffer,
			amf_size* size,
			const amf_size requiredSize
		    );
#else
        AMF_RESULT AdjustInternalBufferSize(
			amf::AMFBuffer ** _buffer,
			amf_size* size,
			const amf_size requiredSize
		    );
#endif
    };
} //amf
