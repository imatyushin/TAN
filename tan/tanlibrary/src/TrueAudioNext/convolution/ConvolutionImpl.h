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
///  @file   ConvolutionImpl.h
///  @brief  TANConvolutionImpl interface implementation
///-------------------------------------------------------------------------
#pragma once
#include "TrueAudioNext.h"   //TAN
#include "public/include/core/Factory.h"        //AMF
#include "public/include/core/Context.h"        //AMF
#include "public/include/core/Buffer.h"         //AMF
#include "public/include/components/Component.h"//AMF
#include "public/common/PropertyStorageExImpl.h"//AMF

#include "GraalConv.hpp"
#include "GraalConv_clFFT.hpp"
#ifdef AMF_FACILITY
#  undef AMF_FACILITY
#endif

#define SAFE_DELETE(x) { delete x; (x) = nullptr; }
#define SAFE_ARR_DELETE(x) { delete[] x; (x) = nullptr; }

namespace amf
{
    struct TANSampleBuffer
    {
        union
        {
            float **host = nullptr;

#ifndef TAN_NO_OPENCL
            cl_mem *clmem;
#else
            amf::AMFBuffer **amfBuffers;
#endif
        } buffer;

    protected:
        AMF_MEMORY_TYPE mType = AMF_MEMORY_UNKNOWN;
        bool mAllocated = false;
        size_t mSize = 0;

    public:
        ~TANSampleBuffer()
        {
            Release();
        }

        void Release()
        {
            if(amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST == mType && buffer.host)
            {
				if(mAllocated)
				{
					delete[] buffer.host;
				}

                buffer.host = nullptr;
            }
#ifndef TAN_NO_OPENCL
            else if(amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL == mType && buffer.clmem)
            {
				if(mAllocated)
				{
					delete[] buffer.clmem;
				}
                delete [] buffer.clmem;
                buffer.clmem = nullptr;
            }
#else
            else if(amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL == mType && buffer.amfBuffers)
            {
				if(mAllocated)
				{
					delete[] buffer.amfBuffers;
				}

                buffer.amfBuffers = nullptr;
            }
#endif

			mAllocated = false;
            mType = AMF_MEMORY_UNKNOWN;
        }

        void FreeObjects()
        {
#ifndef TAN_NO_OPENCL
            if(amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL == mType && buffer.clmem)
            {
				for(size_t channel(0); channel < mSize; ++mSize)
                {
                    clReleaseMemObject(buffer.clmem[channel]);
                    buffer.clmem[channel] = nullptr;
                }
            }
#else
            if(amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL == mType && buffer.amfBuffers)
            {
				for(size_t channel(0); channel < mSize; ++channel)
                {
                    if(buffer.amfBuffers[channel])
                    {
                        buffer.amfBuffers[channel]->Release();
                        buffer.amfBuffers[channel] = nullptr;
                    }
                }
            }
#endif

			mAllocated = false;
            mType = AMF_MEMORY_UNKNOWN;
        }

		TANSampleBuffer & operator =(const TANSampleBuffer & other)
		{
			if(IsSet())
			{
				Release();
			}

			mType = other.mType;
			mAllocated = false;
			buffer = other.buffer;

			return *this;
		}

        inline bool IsSet() const {return amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN != mType && mAllocated;}
        inline AMF_MEMORY_TYPE GetType() const {return mType;}

#ifndef TAN_NO_OPENCL
        void PrepareCL(size_t channelsCount)
        {
            mType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;
            mAllocated = true;
            mSize = channelsCount;

            buffer.clmem = new cl_mem[channelsCount];
            std::memset(buffer.clmem, 0, sizeof(cl_mem) * channelsCount);
        }

        void SetCLBuffers(cl_mem *buffers)
        {
            mType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;
            mAllocated = false;
            buffer.clmem = buffers;
        }
#else
        void PrepareAMF(size_t channelsCount)
        {
            mType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;
            mAllocated = true;
            mSize = channelsCount;

            buffer.amfBuffers = new amf::AMFBuffer *[channelsCount];
            std::memset(buffer.amfBuffers, 0, sizeof(amf::AMFBuffer *) * channelsCount);
        }

        void SetAMFBuffers(amf::AMFBuffer ** amfBuffers)
        {
            mType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;
            mAllocated = false;
            buffer.amfBuffers = amfBuffers;
        }
#endif
        void PrepareHost(size_t channelsCount)
        {
            mType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST;
            mAllocated = true;
            mSize = channelsCount;

            buffer.host = new float *[channelsCount];
            std::memset(buffer.host, 0, sizeof(float *) * channelsCount);
        }

        void SetHost(float ** buffers)
        {
            mType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST;
            mAllocated = false;
            buffer.host = buffers;
        }
    };

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

        void ResetCL()
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
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                    length * sizeof(float),
                    &amfFilter[index]
                    )
                );

            AMF_RETURN_IF_FAILED(
                context->AllocBuffer(
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                    length * sizeof(float),
                    &amfTemp[index]
                    )
                );

            AMF_RETURN_IF_FAILED(
                context->AllocBuffer(
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                    length * sizeof(float),
                    &amfSampleHistory[index]
                    )
                );
        }
#endif
    } tdFilterState;

    class TANConvolutionImpl
        : public virtual AMFInterfaceImpl < AMFPropertyStorageExImpl< TANConvolution> >
    {
    public:
        typedef AMFInterfacePtr_T<TANConvolutionImpl> Ptr;

        TANConvolutionImpl(TANContext *pContextTAN);
        virtual ~TANConvolutionImpl(void);

// interface access
        AMF_BEGIN_INTERFACE_MAP
            AMF_INTERFACE_CHAIN_ENTRY(AMFInterfaceImpl< AMFPropertyStorageExImpl <TANConvolution> >)
        AMF_END_INTERFACE_MAP

//TANConvolution interface

		AMF_RESULT	AMF_STD_CALL    Init(TAN_CONVOLUTION_METHOD convolutionMethod,
										amf_uint32 responseLengthInSamples,
										amf_uint32 bufferSizeInSamples,
										amf_uint32 channels) override;
        AMF_RESULT  AMF_STD_CALL    InitCpu(TAN_CONVOLUTION_METHOD convolutionMethod,
                                        amf_uint32 responseLengthInSamples,
                                        amf_uint32 bufferSizeInSamples,
                                        amf_uint32 channels) override;
        AMF_RESULT  AMF_STD_CALL    InitGpu(TAN_CONVOLUTION_METHOD convolutionMethod,
                                        amf_uint32 responseLengthInSamples,
                                        amf_uint32 bufferSizeInSamples,
                                        amf_uint32 channels) override;
        AMF_RESULT  AMF_STD_CALL    InitGpuAMF(amf::AMFFactory * factory,
                                        TAN_CONVOLUTION_METHOD convolutionMethod,
                                        amf_uint32 responseLengthInSamples,
                                        amf_uint32 bufferSizeInSamples,
                                        amf_uint32 channels) override;
        AMF_RESULT  AMF_STD_CALL    Terminate() override;

        AMF_RESULT  AMF_STD_CALL    UpdateResponseTD(
                                        float* ppBuffer[],
                                        amf_size numOfSamplesToProcess,
                                        const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
                                        const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
                                        ) override;
#ifndef TAN_NO_OPENCL
        AMF_RESULT  AMF_STD_CALL    UpdateResponseTD(
                                        cl_mem ppBuffer[],
                                        amf_size numOfSamplesToProcess,
                                        const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
                                        const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
                                        ) override;
#else
        AMF_RESULT AMF_STD_CALL     UpdateResponseTD(
                                        AMFBuffer * ppBuffer[],
                                        amf_size numOfSamplesToProcess,
                                        const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
                                        const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
                                        ) override;
#endif

        AMF_RESULT  AMF_STD_CALL    UpdateResponseFD(
                                        float* ppBuffer[],
                                        amf_size numOfSamplesToProcess,
                                        const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
                                        const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
                                        ) override;

#ifndef TAN_NO_OPENCL
        AMF_RESULT  AMF_STD_CALL    UpdateResponseFD(cl_mem ppBuffer[],
                                        amf_size numOfSamplesToProcess,
                                        const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
                                        const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
                                        ) override;
#else
        AMF_RESULT  AMF_STD_CALL    UpdateResponseFD(const AMFBuffer * ppBuffer[],
                                        amf_size numOfSamplesToProcess,
                                        const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
                                        const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
                                        ) override;
#endif

        AMF_RESULT  AMF_STD_CALL    Process(float* ppBufferInput[],
                                        float* ppBufferOutput[],
                                        amf_size numOfSamplesToProcess,
                                        // Masks of flags from enum
                                        // TAN_CONVOLUTION_CHANNEL_FLAG.
                                        const amf_uint32 flagMasks[],
                                        amf_size *pNumOfSamplesProcessed = nullptr) override; // system memory
#ifndef TAN_NO_OPENCL
        AMF_RESULT  AMF_STD_CALL    Process(cl_mem pBufferInput[],
                                            cl_mem pBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            // Masks of flags from enum
                                            // TAN_CONVOLUTION_CHANNEL_FLAG.
                                            const amf_uint32 flagMasks[],
                                            amf_size *pNumOfSamplesProcessed = nullptr) override; // cl_mem
        AMF_RESULT  AMF_STD_CALL    Process(float* pBufferInput[],
                                            cl_mem pBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            // Masks of flags from enum
                                            // TAN_CONVOLUTION_CHANNEL_FLAG.
                                            const amf_uint32 flagMasks[],
                                            amf_size *pNumOfSamplesProcessed = nullptr) override; // cl_mem
#else
        AMF_RESULT  AMF_STD_CALL    Process(const AMFBuffer * pBufferInput[],
                                            AMFBuffer * pBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            // Masks of flags from enum
                                            // TAN_CONVOLUTION_CHANNEL_FLAG.
                                            const amf_uint32 flagMasks[],
                                            amf_size *pNumOfSamplesProcessed = nullptr) override;
        AMF_RESULT  AMF_STD_CALL    Process(float* pBufferInput[],
                                            AMFBuffer * pBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            // Masks of flags from enum
                                            // TAN_CONVOLUTION_CHANNEL_FLAG.
                                            const amf_uint32 flagMasks[],
                                            amf_size *pNumOfSamplesProcessed = nullptr) override;
#endif

        // Process direct (no update required), system memory buffers:
        AMF_RESULT  AMF_STD_CALL    ProcessDirect(
                                            float* ppImpulseResponse[],
                                            float* ppBufferInput[],
                                            float* ppBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            amf_size *pNumOfSamplesProcessed,
                                            int *nzFirstLast = NULL
                                            ) override;
#ifndef TAN_NO_OPENCL
        // Process direct (no update required),  OpenCL cl_mem  buffers:
        AMF_RESULT  AMF_STD_CALL    ProcessDirect(
                                            cl_mem* ppImpulseResponse[],
                                            cl_mem* ppBufferInput[],
                                            cl_mem* ppBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            amf_size *pNumOfSamplesProcessed,
                                            int *nzFirstLast = NULL
                                            ) override;
#else
     AMF_RESULT  AMF_STD_CALL    ProcessDirect(
                                            const AMFBuffer * ppImpulseResponse[],
                                            const AMFBuffer * ppBufferInput[],
                                            AMFBuffer * ppBufferOutput[],
                                            amf_size numOfSamplesToProcess,
                                            amf_size *pNumOfSamplesProcessed,
                                            int *nzFirstLast = NULL
                                            ) override;
#endif


        AMF_RESULT AMF_STD_CALL GetNextFreeChannel(amf_uint32 *pChannelIndex,
                                                   const amf_uint32 flagMasks[] // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG.
                                                   ) override;

        virtual TANContext* AMF_STD_CALL GetContext() {return m_pContextTAN;}

    protected:
        virtual AMF_RESULT  Init(TAN_CONVOLUTION_METHOD convolutionMethod,
                                amf_uint32 responseLengthInSamples,
                                amf_uint32 bufferSizeInSamples,
                                amf_uint32 channels,
                                bool doProcessingOnGpu,
                                amf::AMFFactory * factory
                                );

        virtual AMF_RESULT Flush(amf_uint32 filterStateId, amf_uint32 channelId);

        AMF_RESULT  AMF_STD_CALL UpdateResponseTD(
            const TANSampleBuffer & pBuffer,
            amf_size numOfSamplesToProcess,
            const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
            const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
            );

        AMF_RESULT  AMF_STD_CALL    Process(const TANSampleBuffer & pBufferInput,
                                            const TANSampleBuffer & pBufferOutput,
                                            amf_size numOfSamplesToProcess,
                                            // Masks of flags from enum
                                            // TAN_CONVOLUTION_CHANNEL_FLAG.
                                            const amf_uint32 flagMasks[],
                                            amf_size *pNumOfSamplesProcessed = nullptr); // TAN Audio buffers

        amf::AMFFactory *           mFactory;

        TANContextPtr               m_pContextTAN;
        AMFComputePtr               m_pProcContextAMF;
        AMFComputePtr               m_pUpdateContextAMF;

        TAN_CONVOLUTION_METHOD      m_eConvolutionMethod;
        amf_uint32                  m_iLengthInSamples;
        amf_uint32                  m_iBufferSizeInSamples;
        amf_uint32                  m_iChannels;    // Number of channels our buffers are allocated
                                                    // to, as opposed to m_MaxChannels.

#ifndef TAN_NO_OPENCL
		cl_kernel					m_pKernelCrossfade;
#else
        amf::AMFComputeKernelPtr    mKernelCrossfade;
#endif

        AMF_MEMORY_TYPE             m_eOutputMemoryType;

        AMF_KERNEL_ID               m_KernelIdCrossfade;

        AMFCriticalSection          m_sect;

        // In the current implementation we can run neither Update() nor Process() in parallel to
        // itself (as we use many different preallocated buffers).
        AMFCriticalSection          m_sectAccum;
        AMFCriticalSection          m_sectUpdate;
        AMFCriticalSection          m_sectProcess;

        //cl_mem           m_pUpdateInputOCL;
        //cl_mem           m_pInputsOCL;
        //cl_mem           m_pOutputsOCL;

        TANFFTPtr                   m_pTanFft;
        TANFFTPtr                   m_pUpdateTanFft;

        bool                        m_doProcessOnGpu;

    private:
        bool                        m_initialized = false;

#ifndef TAN_NO_OPENCL
        //cl_program m_TimeDomainProgram;
        cl_kernel                   m_TimeDomainKernel = nullptr;
#else
        amf::AMFComputeKernelPtr    mTimeDomainKernel;
#endif

        int                         m_length = 0;           // Next power of two for the response's length.
        int                         m_log2len = 0;          // =log2(m_length)
        int                         m_MaxChannels = 0;      // Current number of channels, less or equal to m_iChannels.
        float **                    m_OutSamples = nullptr;   // Buffer to store FFT, multiplication and ^FFT for a buffer.
        float **                    m_ovlAddLocalInBuffs = nullptr;
        float **                    m_ovlAddLocalOutBuffs = nullptr;

        TANSampleBuffer             m_FadeSubbufers[2];  // For cross-fading on GPU is created as subfolder of m_pCLXFadeMasterBuf[] memory objects

#ifndef TAN_NO_OPENCL
        cl_mem                      m_pCLXFadeMasterBuf[2] = {nullptr};
#else
        amf::AMFBufferPtr           mAMFCLXFadeMasterBuffers[2];
#endif

        TANSampleBuffer m_pXFadeSamples;  // For cross-fading on CPU
        float *m_silence;       // Array filled with zeroes, used to emulate silent signal.

        TANSampleBuffer m_internalOutBufs;
        TANSampleBuffer m_internalInBufs;
        bool *m_availableChannels;
        bool *m_flushedChannels;// if a channel has just been flushed no need to flush it repeatedly
        int *m_tailLeftOver;

        AMF_RESULT allocateBuffers();
        AMF_RESULT deallocateBuffers();
        AMF_RESULT AMF_FAST_CALL Crossfade(
            TANSampleBuffer pBufferOutput, amf_size numOfSamplesToProcess);

        typedef struct _ovlAddFilterState {
            float **m_Filter;
            float **m_Overlap;
            float **m_internalFilter;
            float **m_internalOverlap;
        } ovlAddFilterState;

#  define N_FILTER_STATES 3
        ovlAddFilterState *m_FilterState[N_FILTER_STATES] = {0};
        tdFilterState *m_tdFilterState[N_FILTER_STATES] = {0};
        tdFilterState *m_tdInternalFilterState[N_FILTER_STATES] = {0};
        int m_idxFilter;                        // Currently USED current index.
        int m_idxPrevFilter;                    // Currently USED previous index (for crossfading).
        int m_idxUpdateFilter;                  // Next FREE  index.
        int m_idxUpdateFilterLatest;
        int m_first_round_ever;

        AMFEvent m_procReadyForNewResponsesEvent;
        AMFEvent m_updateFinishedProcessing;
        AMFEvent m_xFadeStarted;

        class UpdateThread : public AMFThread
        {
        protected:
            TANConvolutionImpl *m_pParent;
        public:
            UpdateThread(TANConvolutionImpl *pParent) : m_pParent(pParent) {}
            void Run() override { m_pParent->UpdateThreadProc(this); }
        };
        UpdateThread m_updThread;

        //todo: legacy debug code?
        // HACK Windows specific
        //HANDLE m_updThreadHandle;

        void UpdateThreadProc(AMFThread *pThread);

        AMF_RESULT VectorComplexMul(float *vA, float *vB, float *out, int count);

        AMF_RESULT ovlAddProcess(
            amf_size &              numberOfSamplesProcessed,
            ovlAddFilterState *     state,
            const TANSampleBuffer & inputData,
            const TANSampleBuffer & outputData,
            amf_size                length,
            amf_uint32              n_channels,
            bool                    advanceOverlap = true
            );

        amf_size ovlTDProcess(tdFilterState *state, float **inputData, float **outputData, amf_size length,
            amf_uint32 n_channels);

        AMF_RESULT ovlTimeDomainCPU(float *resp, amf_uint32 firstNonZero, amf_uint32 lastNonZero,
            float *in, float *out, float *histBuf, amf_uint32 bufPos,
            amf_size datalength, amf_size convlength);

        AMF_RESULT ovlTimeDomain(
            tdFilterState *state,
            int chIdx,
            float *resp,
            amf_uint32 firstNonZero,
            amf_uint32 lastNonZero,
            float *in,
            float *out,
            amf_uint32 bufPos,
            amf_size datalength,
            amf_size convlength
            );

#ifndef TAN_NO_OPENCL
        AMF_RESULT ovlTimeDomainGPU(cl_mem resp, amf_uint32 firstNonZero, amf_uint32 lastNonZero,
             cl_mem out, cl_mem histBuf, amf_uint32 bufPos,
            amf_size datalength, amf_size convlength);
#else
        AMF_RESULT ovlTimeDomainGPU(
            AMFBuffer * resp,
            amf_uint32 firstNonZero,
            amf_uint32 lastNonZero,
            AMFBuffer * out,
            AMFBuffer * histBuf,
            amf_uint32 bufPos,
            amf_size datalength,
            amf_size convlength
            );
#endif

        AMF_RESULT ProcessInternal(
            int idx,
            const TANSampleBuffer & pBuffersInput,
            const TANSampleBuffer & pBufferOutput,
            amf_size length,
            const amf_uint32 flagMasks[],
            amf_size *pNumOfSamplesProcessed,
            int prev_input = 0,
            int advance_time = 1,
            int ocl_skip_stage = 0,
            int ocl_crossfade_state = 0
            );

        /*********************************************************************************
        OCL convolution library
        **********************************************************************************/
        /****************************************************************************************
        Graal convolution library adapter interface
        *****************************************************************************************/
        void * m_graal_conv;

        struct GraalArgs
        {
            GraalArgs() :
                versions(nullptr),
                prevVersions(nullptr),
                channels(nullptr),
                lens(nullptr),
                responses(nullptr),
                negateCnt(0),
                updatesCnt(0)
            {
            }

            ~GraalArgs()
            {
                SAFE_ARR_DELETE(versions);
                SAFE_ARR_DELETE(prevVersions);
                SAFE_ARR_DELETE(channels);
                SAFE_ARR_DELETE(lens);
                SAFE_ARR_DELETE(responses);
                SAFE_ARR_DELETE(negateCnt);
            }

            void Alloc(amf_uint32 channelCnt)
            {
                versions = new int[channelCnt];
                prevVersions = new int[channelCnt];
                channels = new int[channelCnt];
                lens = new int[channelCnt];
                responses = new float*[channelCnt];
                negateCnt = new int[channelCnt];
                std::memset(negateCnt, 0, sizeof(int) * channelCnt);
                Clear(channelCnt);
            }

            void Clear(amf_uint32 channelCnt)
            {
                std::memset(versions, 0, sizeof(int) * channelCnt);
                std::memset(prevVersions, 0, sizeof(int) * channelCnt);
                std::memset(channels, 0, sizeof(int) * channelCnt);
                std::memset(lens, 0, sizeof(int) * channelCnt);
                std::memset(responses, 0, sizeof(float*) * channelCnt);
                updatesCnt = 0;
            }

            void Pack(const GraalArgs &from, amf_uint32 channelCnt)
            {
                Clear(channelCnt);

                for (amf_uint32 channelId = 0; channelId < channelCnt; channelId++)
                {
                    if (from.lens[channelId] > 0) {
                        versions[updatesCnt] = from.versions[channelId];
                        channels[updatesCnt] = from.channels[channelId];
                        lens[updatesCnt] = from.lens[channelId];
                        responses[updatesCnt] = from.responses[channelId];
                        updatesCnt++;
                    }
                }
            }

            void Negate(
                const GraalArgs &from,
                amf_uint32 channelCnt,
                amf_uint32 fromVersion,
                amf_uint32 toVersion)
            {
                Clear(channelCnt);

                amf_uint32 lastChannelId = 0;
                for (amf_uint32 channelId = 0; channelId < channelCnt; channelId++)
                {
                    if (from.lens[channelId] == 0) {
                        // We keep copying a stopped channel from curr slot to update slot up
                        // until its propagated through out all the IR buffers (N_FILTER_STATES)
                        if (negateCnt[channelId] >= N_FILTER_STATES-1) { continue; }
                        versions[updatesCnt] = toVersion;
                        prevVersions[updatesCnt] = fromVersion;
                        channels[updatesCnt] = channelId;
                        negateCnt[channelId]++;
                        updatesCnt++;
                    } else {
                        negateCnt[channelId] = 0;// reset the counter
                    }
                }
            }

            int *versions;
            int *prevVersions;
            int *channels;
            int *lens;
            float **responses;
            int *negateCnt; // Counts how many times a stopped channel has been copied from curr to update slot
            amf_uint32 updatesCnt;
        };

        // response upload parameters (to pass to upload facilities).
        GraalArgs m_uploadArgs;
        // response accumulated parameters (to use in update facility).
        GraalArgs m_accumulatedArgs;
        // response copying parameters (to pass to update facilities).
        GraalArgs m_copyArgs;
        // response update parmeters (to pass to update facilities).
        GraalArgs m_updateArgs;
        // audio stream parameters (to pass to process facilities).
        // Two buffers are used for storing the IR indices; so that back to back calls
        // to the ProcessInternal() during the crossfade will not corrupt the already stored data
        const static int PARAM_BUF_COUNT = 2;
        int * m_s_versions[PARAM_BUF_COUNT];
        int * m_s_channels;
        int m_n_delays_onconv_switch;
        int m_onconv_switch_delay_counter;
        bool m_doHeadTailXfade;
        // m_param_buf_idx is used to flip flop between alternate m_s_versions buffers,
        int m_param_buf_idx;
        /*************************************************************************************
        AmdTrueAudioConvolution destructor
        deallocates all internal buffers.
        *************************************************************************************/

    };
} //amf
