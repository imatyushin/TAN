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
#include "TANSampleBuffer.h"
#include "TDFilterState.h"
#include "FilterState.h"
#include "GraalArgs.h"

//#include "tanlibrary/src/Graal2/GraalWrapper.h"

#include "Debug.h"

#ifdef AMF_FACILITY
#  undef AMF_FACILITY
#endif

#define TRANSFORMTYPE_FFTCOMPLEX 1
#define TRANSFORMTYPE_FFTREAL 2
#define TRANSFORMTYPE_FFTREAL_PLANAR 3
#define TRANSFORMTYPE_HARTLEY 4

#define PARTITION_PAD_FFTREAL_PLANAR 16
#define PARTITION_PAD_FFTREAL 4 // might break CPU ??? was 2

namespace amf
{
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

		virtual AMF_RESULT  AMF_STD_CALL    ProcessFinalize() override;

        AMF_RESULT AMF_STD_CALL GetNextFreeChannel(amf_uint32 *pChannelIndex,
                                                   const amf_uint32 flagMasks[] // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG.
                                                   ) override;

        virtual TANContext* AMF_STD_CALL GetContext() override {return m_pContextTAN;}

    protected:
        virtual AMF_RESULT  Init(TAN_CONVOLUTION_METHOD convolutionMethod,
                                 amf_uint32 responseLengthInSamples,
                                 amf_uint32 bufferSizeInSamples,
                                 amf_uint32 channels,
                                 bool doProcessingOnGpu
                                 );

        virtual AMF_RESULT Flush(amf_uint32 filterStateId, amf_uint32 channelId);

        AMF_RESULT  AMF_STD_CALL UpdateResponseTD(
            TANSampleBuffer & pBuffer,
            amf_size numOfSamplesToProcess,
            const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
            const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
            );

        AMF_RESULT  AMF_STD_CALL    Process(
            const TANSampleBuffer & bufferInput,
            TANSampleBuffer & bufferOutput,
            amf_size numOfSamplesToProcess,
            // Masks of flags from enum
            // TAN_CONVOLUTION_CHANNEL_FLAG.
            const amf_uint32 flagMasks[],
            amf_size *pNumOfSamplesProcessed = nullptr
            ); // TAN Audio buffers

        bool                        ReadyForIRUpdate();

		TANContextPtr               m_pContextTAN;
        AMFComputePtr               m_pProcContextAMF;
        AMFComputePtr               m_pUpdateContextAMF;
		TANMathPtr                  m_pMath;

        TAN_CONVOLUTION_METHOD      m_eConvolutionMethod = TAN_CONVOLUTION_METHOD::TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD;
		bool						m_bUseProcessFinalize = false;
		amf_uint32                  m_iLengthInSamples = 0;
        amf_uint32                  m_iBufferSizeInSamples = 0;
        amf_uint32                  m_iChannels = 0; // Number of channels our buffers are allocated
                                                     // to, as opposed to m_MaxChannels.

#ifndef TAN_NO_OPENCL
		cl_kernel					m_pKernelCrossfade;
#else
        amf::AMFComputeKernelPtr    mKernelCrossfade;
#endif
        AMF_MEMORY_TYPE             m_eOutputMemoryType = AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN;

        AMF_KERNEL_ID               m_KernelIdCrossfade = 0;

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

        bool                        m_doProcessOnGpu = false;
		int			                m_TransformType = 0;


    private:
        bool                        m_initialized = false;
		//GraalWrapper                m_nonUniformGraal;

#ifndef TAN_NO_OPENCL
        //cl_program m_TimeDomainProgram;
        cl_kernel                   m_TimeDomainKernel = nullptr;
#else
        amf::AMFComputeKernelPtr    mTimeDomainKernel;
#endif

        int                         m_length = -1;                       // Next power of two for the response's length.
        int                         m_log2len = -1;                      // =log2(m_length)
		int                         m_log2bsz = 0;                      // = log2( m_iBufferSizeInSamples)

		int                         m_RunningChannels = -1;              // Current number of channels, less or equal to m_iChannels. //maxChannels
        float                       **m_OutSamples = nullptr;           // Buffer to store FFT, multiplication and ^FFT for a buffer.
		float                       **m_OutSamplesXFade = nullptr;      // Buffer to store FFT, multiplication and ^FFT for a buffer.

        std::vector<std::vector<float>>
                                    m_ovlAddLocalInBuffs;
        std::vector<std::vector<float>>
                                    m_ovlAddLocalOutBuffs;

        TANSampleBuffer             mFadeSubbufers[2];                  //(m_pCLXFadeSubBuf) For cross-fading on GPU is created as subfolder of m_pCLXFadeMasterBuf[] memory objects

#ifndef TAN_NO_OPENCL
        cl_mem                      m_pCLXFadeMasterBuf[2] = {nullptr};
#else
        amf::AMFBufferExPtr         mAMFCLXFadeMasterBuffers[2];
#endif

        TANSampleBuffer             m_pXFadeSamples;                    // For cross-fading on CPU
        float                       *m_silence = nullptr;               // Array filled with zeroes, used to emulate silent signal.

        TANSampleBuffer m_internalOutBufs;
        TANSampleBuffer m_internalInBufs;
        bool *m_availableChannels = nullptr;
        bool *m_flushedChannels = nullptr;                              // if a channel has just been flushed no need to flush it repeatedly
        int *m_tailLeftOver = nullptr;

        AMF_RESULT allocateBuffers();
        AMF_RESULT deallocateBuffers();
        AMF_RESULT AMF_FAST_CALL Crossfade(
            TANSampleBuffer & pBufferOutput,
            amf_size numOfSamplesToProcess,
            int curFadeSample,
			int fadeLength
            );

        int m_currentDataPartition = 0;
		int m_dataRowLength = 0;
		int m_DelayedUpdate = 0;
        int m_curCrossFadeSample = 0;
		float **m_FilterTD = nullptr;

		//const int m_PartitionPad = 8;
		typedef struct _ovlUniformPartitionFilterState {
			float **m_Filter = nullptr;
            float **m_DataPartitions = nullptr;
            float **m_Overlap = nullptr;
            float **m_internalFilter = nullptr;
            float **m_internalOverlap = nullptr;
            float **m_internalDataPartitions = nullptr;
            cl_mem *m_clFilters = nullptr;
            cl_mem *m_clDataPartitions = nullptr;
            cl_mem m_clOutput = nullptr;
			//cl_mem *m_clFilterSubParts;
			//cl_mem *m_clDataSubParts;
			//cl_mem *m_clOutputSubChans;
		} ovlUniformPartitionFilterState;

		int m_2ndBufSizeMultiple = 4;  // default 4
		int m_2ndBufCurrentSubBuf = 0;  // 0 -> m_2ndBufSizeMultiple - 1
		float **m_NUTailAccumulator = nullptr;   // store complex multiply accumulate data calculated in ovlNUPProcessTail
		float **m_NUTailSaved = nullptr;   // save last complex multiply accumulate results
		bool m_CrossFading = false;
		typedef struct _ovlNonUniformPartitionFilterState {
			float **m_Filter = nullptr;
            float **m_DataPartitions = nullptr;
            float **m_Overlap = nullptr;
            float **m_internalFilter = nullptr;
            float **m_internalOverlap = nullptr;
            float **m_internalDataPartitions = nullptr;
            float **m_SubPartitions = nullptr;
            float **m_workBuffer = nullptr;
		} ovlNonUniformPartitionFilterState;
        size_t mNUPSize = 0;
        size_t mNUPSize2 = 0;

        ovlAddFilterState m_FilterState[N_FILTER_STATES];

		_ovlUniformPartitionFilterState *m_upFilterState[N_FILTER_STATES] = {nullptr};
		_ovlUniformPartitionFilterState *m_upTailState = nullptr;
		_ovlNonUniformPartitionFilterState *m_nupFilterState[N_FILTER_STATES] = {nullptr};
		_ovlNonUniformPartitionFilterState *m_nupTailState = nullptr;
		tdFilterState *m_tdFilterState[N_FILTER_STATES] = {nullptr};
        tdFilterState *m_tdInternalFilterState[N_FILTER_STATES] = {nullptr};
        int m_idxFilter = 1;                        // Currently USED current index.
        int m_idxPrevFilter = 0;                    // Currently USED previous index (for crossfading).
        int m_idxUpdateFilter = 2;                  // Next FREE  index.
        int m_idxUpdateFilterLatest = 0;
        int m_first_round_ever = 0;

		int bestNUMultiple(int responseLength, int blockLength);

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
            TANSampleBuffer &       outputData,
            amf_size                length,
            amf_uint32              n_channels,
            bool                    advanceOverlap = true
            );

		amf_size ovlNUPProcess(
            ovlNonUniformPartitionFilterState *
                                            state,
            const TANSampleBuffer &         inputData,
            TANSampleBuffer &               outputData,
            amf_size                        length,
			amf_uint32                      n_channels,
            bool                            advanceOverlap = true,
            bool                            useXFadeAccumulator = false
            );

		amf_size ovlNUPProcessCPU(
            ovlNonUniformPartitionFilterState *
                                            state,
            const TANSampleBuffer &         inputData,
            TANSampleBuffer &               outputData,
            amf_size                        length,
			amf_uint32                      n_channels,
            bool                            advanceOverlap = true,
            bool                            useXFadeAccumulator = false
            );

		int ovlNUPProcessTail(_ovlNonUniformPartitionFilterState *state = NULL, bool useXFadeAccumulator = false);


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
            const TANSampleBuffer & buffersInput,
            TANSampleBuffer & bufferOutput,
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
        std::unique_ptr<graal::CGraalConv> mGraalConv;

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
        int * m_s_versions[PARAM_BUF_COUNT] = {nullptr};
        int * m_s_channels = nullptr;
        int m_n_delays_onconv_switch = 0;
        int m_onconv_switch_delay_counter = 0;
        bool m_doHeadTailXfade = false;
        // m_param_buf_idx is used to flip flop between alternate m_s_versions buffers,
        int m_param_buf_idx = 0;
		bool m_startNonUniformConvXFade = false;
		/*************************************************************************************
        AmdTrueAudioConvolution destructor
        deallocates all internal buffers.
        *************************************************************************************/

    };
} //amf
