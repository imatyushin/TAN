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
#include "ConvolutionImpl.h"
#include "../core/TANContextImpl.h"
#include "../fft/FFTImpl.h"
#include "../math/MathImpl.h"

#include "OCLHelper.h"
#include "FileUtility.h"
#include "Exceptions.h"
#include "Debug.h"
#include "Log.h"

#include "public/common/AMFFactoryHelper.h"

#ifdef ENABLE_METAL
  #include "MetalKernel_Crossfading.h"
#else
  #include "CLKernel_Crossfading.h"
#endif

#ifndef TAN_NO_OPENCL
  #include <CL/cl.h>
#endif

#include <tuple>

#define AMF_FACILITY L"TANConvolutionImpl"

using namespace amf;

//-------------------------------------------------------------------------------------------------
#define RETURN_IF_FAILED(ret) \
    if ((ret) != AMF_OK) goto ErrorHandling;
#define RETURN_IF_FALSE(expr, retVariable, retCode) \
    if ((expr) == false) { \
        retVariable = retCode;\
        goto ErrorHandling; \
    }
//-------------------------------------------------------------------------------------------------
TAN_SDK_LINK AMF_RESULT AMF_CDECL_CALL TANCreateConvolution(
    amf::TANContext* pContext,
    amf::TANConvolution** ppConvolution
    )
{
    AMF_RETURN_IF_INVALID_POINTER(pContext, L"pContext == nullptr");
    AMF_RETURN_IF_INVALID_POINTER(ppConvolution,
		L"ppConvolution == nullptr");

    TANContextImplPtr contextImpl(pContext);
    *ppConvolution = new TANConvolutionImpl(pContext);
    (*ppConvolution)->Acquire();

    return AMF_OK;
}

//-------------------------------------------------------------------------------------------------
TANConvolutionImpl::TANConvolutionImpl(TANContext *pContextTAN)
    :m_pContextTAN(pContextTAN)
    ,m_eConvolutionMethod(TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD)
    ,m_iLengthInSamples(0)
    ,m_iBufferSizeInSamples(0)
    ,m_iChannels(0)
    ,m_eOutputMemoryType(AMF_MEMORY_HOST)
    ,m_updThread(this)
    ,m_doHeadTailXfade(0)
	,m_OutSamplesXFade(nullptr)
	,m_bUseProcessFinalize(false)
#ifdef USE_TAIL_THREAD
	, m_tailThread(this)
#endif
{
    TANContextImplPtr contextImpl(pContextTAN);

    m_pUpdateContextAMF = contextImpl->GetGeneralCompute();
    m_pProcContextAMF = contextImpl->GetConvolutionCompute();

    PrintDebug("99 m_xFadeStarted.SetEvent()");
    m_xFadeStarted.SetEvent();

    // CPU processing case.
    if (!m_pProcContextAMF)
    {
        m_pProcContextAMF = m_pUpdateContextAMF;
    }

    AMFPrimitivePropertyInfoMapBegin
        AMFPropertyInfoEnum(TAN_OUTPUT_MEMORY_TYPE ,  L"Output Memory Type", AMF_MEMORY_HOST, AMF_MEMORY_ENUM_DESCRIPTION, false),
    AMFPrimitivePropertyInfoMapEnd

    m_initialized = false;
    m_idxFilter = 1;
    m_idxPrevFilter = 0;
    m_idxUpdateFilter = 2;

    m_graal_conv = nullptr;

    m_s_versions[0] = NULL;
    m_s_versions[1] = NULL;
    m_s_channels = NULL;
    m_n_delays_onconv_switch = 0;
    m_onconv_switch_delay_counter = 0;

    memset(m_upFilterState, 0, sizeof(m_upFilterState));
	m_tailLeftOver = nullptr;
    m_availableChannels = nullptr;
    m_flushedChannels = nullptr;

    m_silence = nullptr;
    m_OutSamples = nullptr;
	m_NUTailAccumulator = nullptr;
	m_NUTailSaved = nullptr;
	m_RunningChannels = -1;
	m_log2len = -1;
    m_length = -1;
    m_param_buf_idx = 0;

    m_doProcessOnGpu = false;

	m_currentDataPartition = 0;
	m_DelayedUpdate = 0;

	m_2ndBufSizeMultiple = 4;
	m_2ndBufCurrentSubBuf = 0;
}
//-------------------------------------------------------------------------------------------------
TANConvolutionImpl::~TANConvolutionImpl(void)
{
    Terminate();
}

//-------------------------------------------------------------------------------------------------
bool TANConvolutionImpl::ReadyForIRUpdate()
{
    if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED)
    {
        assert(false);
        THROW_NOT_IMPLEMENTED;

		/*if (!m_nonUniformGraal.ready4IrUpdate()) // For the non-uniform convolution case, the internal state should be ready for the IR buffer flip
			return false;*/
    }

    auto empty(m_accumulatedArgs.updatesCnt == 0);
    auto locked(m_updateFinishedProcessing.Lock(0));

    //PrintDebug(ES + "ReadyForIRUpdate: " + (empty ? "1" : "0") + " " + (locked ? "1" : "0") + " SetEvent()");

	return empty && locked;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::Init(
    TAN_CONVOLUTION_METHOD convolutionMethod,
    amf_uint32 responseLengthInSamples,
    amf_uint32 bufferSizeInSamples,
    amf_uint32 channels
    )
{
    AMF_RETURN_IF_FALSE(m_pContextTAN != NULL, AMF_WRONG_STATE,
        L"Cannot initialize after termination");

    // Determine how to initialize based on context, CPU for CPU and GPU for GPU
#ifndef TAN_NO_OPENCL
	if (m_pContextTAN->GetOpenCLContext())
#else
	if (m_pContextTAN->GetAMFConvQueue())
#endif
    {
        return Init(
            convolutionMethod, responseLengthInSamples, bufferSizeInSamples, channels, true);
    }
    else
    {
        return Init(
            convolutionMethod, responseLengthInSamples, bufferSizeInSamples, channels, false);
    }
}

/* Best fft length multiple for non-uniform partitioned convolution CPU implementation
TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM

Derivation:
We estimate the sum of time in FFT Tf and time in complex multiply accumulate Tm.
where:
M = multiple
B = block size
R = response length

FFT for convolution is double size so:
Tf = 2*B*M * log(2*B*M)

we need the new tail calculation only after M blocks, so:
Tm = 2*R/M

time per block is Tf + Tm, but there are sRate/B blocks per second so

Ttot = 2*M*log(2*M*B) + 2*R/(M*B)

*/


int TANConvolutionImpl::bestNUMultiple(int responseLength, int blockLength) {
	int multiple = 1;
	// Heuristic to guess best multiple:

	multiple = responseLength / (blockLength * 32);
	if (multiple < 1)
		multiple = 1;

	int M = 1;
	int B = blockLength;
	int R = responseLength;
	float  Tmin = std::numeric_limits<float>::max();
	int maxM = R / (8 * B);
	for (M = 1; M < maxM; M *= 2) {
		float t = 2 * M*log(double(2 * B*M)) + 2 * R / (B*M);
		if (t < Tmin) {
			Tmin = t;
			multiple = M;
		}
	}

	return multiple;
}


//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::InitCpu(
    TAN_CONVOLUTION_METHOD convolutionMethod,
    amf_uint32 responseLengthInSamples,
    amf_uint32 bufferSizeInSamples,
    amf_uint32 channels
    )
{
	// Heuristic to guess best multiple:
	if ((convolutionMethod & ~TAN_CONVOLUTION_METHOD_USE_PROCESS_TAILTHREAD) == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM) {
		m_2ndBufSizeMultiple = bestNUMultiple(responseLengthInSamples, bufferSizeInSamples);
	}
	else {
		m_2ndBufSizeMultiple = 1;
	}

    AMF_RETURN_IF_FALSE(m_pContextTAN != NULL, AMF_WRONG_STATE,
        L"Cannot initialize after termination");

	if (convolutionMethod == TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD) {
		m_TransformType = TRANSFORMTYPE_FFTCOMPLEX;
	}
	else {
#ifdef USE_IPP
		m_TransformType = TRANSFORMTYPE_FFTREAL; // TRANSFORMTYPE_FFTREAL;// TRANSFORMTYPE_FFTCOMPLEX;
#else
		m_TransformType = TRANSFORMTYPE_FFTREAL_PLANAR; // TRANSFORMTYPE_FFTREAL;// TRANSFORMTYPE_FFTCOMPLEX;
#endif
	}

    return Init(convolutionMethod, responseLengthInSamples, bufferSizeInSamples, channels, false);
}
//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::InitGpu(
    TAN_CONVOLUTION_METHOD convolutionMethod,
    amf_uint32 responseLengthInSamples,
    amf_uint32 bufferSizeInSamples,
    amf_uint32 channels)
{
	//m_TransformType = TRANSFORMTYPE_FFTREAL;// TRANSFORMTYPE_FFTCOMPLEX;//TRANSFORMTYPE_FFTREAL; // TRANSFORMTYPE_FFTCOMPLEX;

	if (convolutionMethod == TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD) {
		m_TransformType = TRANSFORMTYPE_FFTCOMPLEX;
	}
	else {
		m_TransformType = TRANSFORMTYPE_FFTREAL; // TRANSFORMTYPE_FFTREAL;// TRANSFORMTYPE_FFTCOMPLEX;
	}

	AMF_RETURN_IF_FALSE(m_pContextTAN != NULL, AMF_WRONG_STATE,
        L"Cannot initialize after termination");

#ifndef TAN_NO_OPENCL
	AMF_RETURN_IF_FALSE(
        m_pContextTAN->GetOpenCLContext() != nullptr,
		AMF_WRONG_STATE,
		L"Cannot initialize on GPU with a CPU context"
		);
#else
	AMF_RETURN_IF_FALSE(
        m_pContextTAN->GetAMFContext() && m_pProcContextAMF && m_pUpdateContextAMF,
        AMF_WRONG_STATE,
        L"Cannot initialize on GPU with a CPU context"
        );
#endif

    return Init(
        convolutionMethod,
        responseLengthInSamples,
        bufferSizeInSamples,
        channels,
        true
        );
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::Terminate()
{
    //int tID = 0;
    //tID = GetThreadId((HANDLE)m_updThread.getNativeThreadHandle());
    PrintDebug("328 m_xFadeStarted.SetEvent()");
    m_xFadeStarted.SetEvent();

    AMFLock lock(&m_sect);

    m_initialized = false;

	deallocateBuffers();
    m_updThread.RequestStop();

    PrintDebug("338 m_procReadyForNewResponsesEvent.SetEvent()");
    m_procReadyForNewResponsesEvent.SetEvent();

    // Windows specific:
   // tID = GetThreadId((HANDLE)m_updThread.getNativeThreadHandle());

    //if (tID != 0) {
        m_updThread.WaitForStop();
    //}

#ifdef USE_TAIL_THREAD
		m_tailThread.RequestStop();
        PrintDebug("350 m_runTailEvent.SetEvent()");
		m_runTailEvent.SetEvent();
		m_tailThread.WaitForStop();
#endif

    PrintDebug("354 m_updateFinishedProcessing.SetEvent()");
    m_updateFinishedProcessing.SetEvent();

#ifndef TAN_NO_OPENCL
	if (m_pContextTAN->GetOpenCLContext() != nullptr)
	{
		AMF_RETURN_IF_CL_FAILED(clReleaseKernel(m_pKernelCrossfade), L"Failed to release kernel");
	}
#else
    mKernelCrossfade = nullptr;
#endif

    m_pContextTAN.Release();
    m_pProcContextAMF.Release();
    m_pUpdateContextAMF.Release();

    m_pTanFft.Release();
    m_pUpdateTanFft.Release();
    m_idxUpdateFilterLatest = -1;// Initially when no IR update has been received m_idxUpdateFilterLatest is -1
    m_idxFilter = 0;
    m_idxPrevFilter = 2;
    m_idxUpdateFilter = 1;

    return AMF_OK;
}
//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::UpdateResponseTD(
    float* pBuffer[],
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],
    const amf_uint32 operationFlags
)
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    AMF_RETURN_IF_FALSE(pBuffer != NULL, AMF_INVALID_ARG, L"pBuffer == NULL");

    AMF_RESULT res = AMF_OK;

    // process

    TANSampleBuffer sampleBuffer;
    sampleBuffer.ReferHostChannels(pBuffer);

    return UpdateResponseTD(sampleBuffer, numOfSamplesToProcess, flagMasks, operationFlags);
}

//-------------------------------------------------------------------------------------------------
#ifndef TAN_NO_OPENCL

AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::UpdateResponseTD(
    cl_mem *pBuffer,
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],
    const amf_uint32 operationFlags
)
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    AMF_RETURN_IF_FALSE(pBuffer != NULL, AMF_INVALID_ARG, L"pBuffer == NULL");

    // process
    TANSampleBuffer sampleBuffer;
    sampleBuffer.ReferCLChannels(pBuffer);

    return UpdateResponseTD(sampleBuffer, numOfSamplesToProcess, flagMasks, operationFlags);
}

#else

AMF_RESULT AMF_STD_CALL TANConvolutionImpl::UpdateResponseTD(
    AMFBuffer * ppBuffer[],
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
    const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
    )
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    AMF_RETURN_IF_FALSE(ppBuffer != NULL, AMF_INVALID_ARG, L"pBuffer == NULL");

    /*PrintAMFArray(
        "::UpdateResponseTD-input[0]",
        ppBuffer[0],
        m_pContextTAN->GetAMFConvQueue(),
        numOfSamplesToProcess * sizeof(float)
        );
    /*PrintAMFArray(
        "::UpdateResponseTD-input[1]",
        ppBuffer[1],
        m_pContextTAN->GetAMFConvQueue(),
        numOfSamplesToProcess * sizeof(float)
        );*/

    // process
    TANSampleBuffer sampleBuffer;
    sampleBuffer.ReferAMFChannels(ppBuffer);

    return UpdateResponseTD(
        sampleBuffer,
        numOfSamplesToProcess,
        flagMasks,
        operationFlags
        );
}
#endif
//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::UpdateResponseTD(
    TANSampleBuffer & pBuffer,
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],
    const amf_uint32 operationFlags
)
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    AMF_RETURN_IF_FALSE(numOfSamplesToProcess <= m_iLengthInSamples, AMF_INVALID_ARG,
                        L"Inconsistent with one set in Init() call length passed");
    // Check if blocking flag is used
    const bool blockUntilReady =
        (operationFlags & TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY);

    {
        //hack
        //	if (m_DelayedUpdate > 0)
        //		return AMF_OK;

		while (m_DelayedUpdate > 0)
        {
			//Sleep(5);
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
	}

    {
        AMFLock lock(&m_sectUpdate);
        AMFLock lockAccum(&m_sectAccum);

        // Check if all the channels are disabled.
        if (flagMasks)
        {
            amf_uint32 actualChannelCnt = 0;
            for (amf_uint32 channelId = 0; channelId < m_iChannels; channelId++)
            {
                if (!(flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
                {
                    actualChannelCnt++;
                }

                // If we need to flush the current stream
                if (flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_FLUSH_STREAM)
                {
                    AMF_RETURN_IF_FAILED(Flush(m_idxFilter, channelId), L"Flush failed");
                }
            }

            //AMF_RETURN_IF_FALSE(actualChannelCnt > 0, AMF_WRONG_STATE, L"No active channels found");
        }

        AMF_RESULT ret = AMF_OK;

        m_idxUpdateFilterLatest = m_idxUpdateFilter;

        switch (m_eConvolutionMethod)
        {
        case TAN_CONVOLUTION_METHOD_TIME_DOMAIN:

#ifndef TAN_NO_OPENCL
            m_accumulatedArgs.updatesCnt = 0;
            if (pBuffer.GetType() == AMF_MEMORY_HOST)
            {
                float **filter = m_tdFilterState[m_idxUpdateFilter]->m_Filter;
                for (amf_uint32 n = 0; n < m_iChannels; n++){
                    if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
                    {
                        memset(filter[n], 0, m_length * sizeof(float));

                        for (int k = 0; k < numOfSamplesToProcess; k++){
							filter[n][k] = *(pBuffer.GetHostBuffers()[n] + k);
							//filter[n][k] = pBuffer.GetHostBuffers()[n][k];
                        }
						m_accumulatedArgs.updatesCnt++;
						m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
                    }
                }
            }
            else
            {
                return AMF_NOT_IMPLEMENTED;
            }
#else
            THROW_NOT_IMPLEMENTED;

            return AMF_NOT_IMPLEMENTED;
#endif
            break;

        case TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD:
            {
                AMFLock syncLock(&m_sectUpdate);

                m_accumulatedArgs.updatesCnt = 0;

                float *const*inputBuffers(nullptr);
                std::vector<float *> ovlAddLocalInBuffers(m_iChannels, nullptr); //gpu conversion stuff

                if(pBuffer.IsComputeBuffer())
                {
                    if(!m_doProcessOnGpu)
                    {
                        return AMF_NOT_IMPLEMENTED;
                    }

                    auto generalQ = m_pContextTAN->GetGeneralQueue();

                    // Read and pass the IRs to a local buffer
                    for(amf_uint32 n = 0; n < m_iChannels; n++)
                    {
                        //memset(m_ovlAddLocalInBuffs[n], 0, numOfSamplesToProcess * sizeof(float));
                        m_ovlAddLocalInBuffs[n].assign(numOfSamplesToProcess, {0});

#ifndef TAN_NO_OPENCL
                        AMF_RETURN_IF_CL_FAILED(
                            clEnqueueReadBuffer(
                                generalQ,
                                pBuffer.GetCLBuffers()[n],
                                CL_TRUE,
                                0,
                                numOfSamplesToProcess * sizeof(float),
                                m_ovlAddLocalInBuffs[n].data(),
                                0,
                                NULL,
                                NULL
                                ),
                                L"Failed reading the IR OCL buffers"
                                );
#else
                        AMF_RETURN_IF_FAILED(
                            generalQ->CopyBufferToHost(
                                pBuffer.GetAMFBuffers()[n],
                                0,
                                numOfSamplesToProcess * sizeof(float),
                                m_ovlAddLocalInBuffs[n].data(),
                                true
                                ),
                            L"Failed reading the IR AMF buffers"
                            );
#endif

                        //PrintFloatArray(ES + "m_ovlAddLocalInBuffs[" + std::to_string(n) + "]", m_ovlAddLocalInBuffs[n].data(), numOfSamplesToProcess * sizeof(float), 64);
                    }

                    //pBuffer.Release();

                    //output = m_ovlAddLocalOutBuffs;
                    for(int channel(0); channel < m_iChannels; ++channel)
                    {
                        ovlAddLocalInBuffers[channel] = m_ovlAddLocalInBuffs[channel].data();
                    }

                    inputBuffers = ovlAddLocalInBuffers.data();
                }
                else
                {
                    inputBuffers = pBuffer.GetHostBuffers();
                }

                float **filter = m_FilterState[m_idxUpdateFilter].m_Filter;
                float **overlap = m_FilterState[m_idxUpdateFilter].m_Overlap;

                for (amf_uint32 n = 0; n < m_iChannels; n++)
                {
                    if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
                    {
                        memset(filter[n], 0, 2 * m_length * sizeof(float));

                        for (int k = 0; k < numOfSamplesToProcess; k++)
                        {
                            // copy data to real part (even samples):
							filter[n][k << 1] = inputBuffers[n][k];
						}

                        //PrintReducedFloatArray("filter[n]", filter[n], 2 * numOfSamplesToProcess * sizeof(float));
                        //PrintFloatArray("filter[n]", filter[n], 2 * numOfSamplesToProcess * sizeof(float), 64);

                        m_accumulatedArgs.responses[n] = filter[n];
                        m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
                        m_accumulatedArgs.updatesCnt++;
                     }
                }

                for(int filter = 0; filter < N_FILTER_STATES; ++filter)
                {
                    for(int channel = 0; channel < m_iChannels; ++channel)
                    {
                        m_FilterState[filter].DebugPrint(
                            ES + __FUNCTION__ + std::to_string(__LINE__) + " ["
                           + std::to_string(filter) + "] ["
                           + std::to_string(channel) + "]",
                            channel
                            );
                    }
                }

                int i = 0;
                ++i;
            }
        break;

		case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM:
		case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM:
		{
			m_accumulatedArgs.updatesCnt = 0;

            const float* const * inputBuffers = pBuffer.GetHostBuffers();

            int nParts = (1 << m_log2len) / (1 << m_log2bsz);

			nParts /= m_2ndBufSizeMultiple; //NU

			float **filter = m_FilterTD;

			for (amf_uint32 n = 0; n < m_iChannels; n++) {
				if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
				{
					memset(filter[n], 0, (2 * m_length + PARTITION_PAD_FFTREAL_PLANAR * nParts) * sizeof(float));

					for (int k = 0; k < numOfSamplesToProcess; k++) {
						// copy real data:
						filter[n][k] = *(inputBuffers[n] + k);
					}

					m_accumulatedArgs.responses[n] = filter[n];
					m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
					m_accumulatedArgs.updatesCnt++;
				}
			}
		}
		break;

        case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED:
        case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED:
        case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:
        {
            //AMFLock *syncLock = new AMFLock(&m_sectUpdate);
            m_accumulatedArgs.updatesCnt = 0;
            graal::CGraalConv*graalConv = (graal::CGraalConv*)m_graal_conv;
            amf_uint32 n_channels = 0;

            if(pBuffer.IsComputeBuffer())
            {
                assert(amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL == pBuffer.GetType());

                cl_mem *clResponses = new cl_mem[m_iChannels];
                printf("todo: remove allocation from runtime\n");

                for (amf_uint32 n = 0; n < m_iChannels; n++)
                {
                    if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
                    {
                        //m_uploadArgs.responses[n_channels] = pBuffer.GetHostBuffers()[n];
                        m_accumulatedArgs.updatesCnt++;

#ifndef TAN_NO_OPENCL
                        clResponses[n_channels] = pBuffer.GetCLBuffers()[n];
#else
                        clResponses[n_channels] = (cl_mem)pBuffer.GetAMFBuffersReadWrite()[n]->GetNative();
#endif

                        m_uploadArgs.versions[n_channels] = m_idxUpdateFilter;
                        m_uploadArgs.channels[n_channels] = n;
                        m_uploadArgs.lens[n_channels] = static_cast<int>(numOfSamplesToProcess);
                        n_channels++;

                        m_accumulatedArgs.versions[n] = m_idxUpdateFilter;
                        m_accumulatedArgs.channels[n] = n;
                        m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
                    }
                }

                AMF_RETURN_IF_FALSE(
                    graalConv->uploadConvGpuPtrs(
                    n_channels,
                    m_uploadArgs.versions,
                    m_uploadArgs.channels,
                    clResponses,
                    m_uploadArgs.lens,
                    true
                    ) == GRAAL_SUCCESS,
                    AMF_UNEXPECTED,
                    L"Internal graal's failure"
                    );

                delete [] clResponses;
            }
            else if (pBuffer.GetType() == AMF_MEMORY_HOST)
            {
                for (amf_uint32 n = 0; n < m_iChannels; n++)
                {
                    if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
                    {
                        m_uploadArgs.responses[n_channels] = pBuffer.GetHostBuffers()[n];
                        m_uploadArgs.versions[n_channels] = m_idxUpdateFilter;
                        m_uploadArgs.channels[n_channels] = n;
                        m_uploadArgs.lens[n_channels] = static_cast<int>(numOfSamplesToProcess);
                        n_channels++;

                        m_accumulatedArgs.versions[n] = m_idxUpdateFilter;
                        m_accumulatedArgs.channels[n] = n;
                        m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
                        m_accumulatedArgs.updatesCnt++;
                    }
                }

                AMF_RETURN_IF_FALSE(
                    graalConv->uploadConvHostPtrs(
                        n_channels,
                        m_uploadArgs.versions,
                        m_uploadArgs.channels,
                        const_cast<const float**>(m_uploadArgs.responses),
                        m_uploadArgs.lens, true
                        ) == GRAAL_SUCCESS,
                    AMF_UNEXPECTED,
                    L"Internal graal's failure"
                    );
            }
        }
        break;

		case TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED:
		{
			amf_uint32 n_channels = 0;
			m_accumulatedArgs.updatesCnt = 0;

            if(pBuffer.IsComputeBuffer())
			{
                THROW_NOT_IMPLEMENTED;

                return AMF_FAIL;
				/*cl_mem *clResponses = new cl_mem[m_iChannels];

				for (amf_uint32 n = 0; n < m_iChannels; n++)
				{
					if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
					{
						clResponses[n_channels] = pBuffer.GetCLBuffers()[n];
						m_uploadArgs.versions[n_channels] = m_idxUpdateFilter;
						m_uploadArgs.channels[n_channels] = n;
						m_uploadArgs.lens[n_channels] = static_cast<int>(numOfSamplesToProcess);
						n_channels++;

						m_accumulatedArgs.versions[n] = m_idxUpdateFilter;
						m_accumulatedArgs.channels[n] = n;
						m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
					}
				}

                AMF_RETURN_IF_FALSE(m_nonUniformGraal.UploadOCLIRs(n_channels, clResponses, m_uploadArgs.lens, m_uploadArgs.channels, m_idxUpdateFilter) == 0,
					AMF_UNEXPECTED, L"Internal graal's failure");

				delete [] clResponses;
				m_accumulatedArgs.updatesCnt = n_channels;*/
			}
			else if (pBuffer.GetType() == AMF_MEMORY_HOST)
			{
                THROW_NOT_IMPLEMENTED;

                return AMF_FAIL;

				/*
                for (amf_uint32 n = 0; n < m_iChannels; n++)
				{
					if (!flagMasks || !(flagMasks[n] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT))
					{
						m_uploadArgs.responses[n_channels] = pBuffer.GetHostBuffers()[n];
						m_uploadArgs.versions[n_channels] = m_idxUpdateFilter;
						m_uploadArgs.channels[n_channels] = n;
						m_uploadArgs.lens[n_channels] = static_cast<int>(numOfSamplesToProcess);
						n_channels++;

						m_accumulatedArgs.versions[n] = m_idxUpdateFilter;
						m_accumulatedArgs.channels[n] = n;
						m_accumulatedArgs.lens[n] = static_cast<int>(numOfSamplesToProcess);
						m_accumulatedArgs.updatesCnt++;
					}
				}

				AMF_RETURN_IF_FALSE(m_nonUniformGraal.UploadHostIRs(n_channels, m_uploadArgs.responses, m_uploadArgs.lens, m_uploadArgs.channels, m_idxUpdateFilter) == 0,
					AMF_UNEXPECTED, L"Internal graal's failure");
                */
			}
		}
		break;

        default:
            AMF_RETURN_IF_FAILED(AMF_NOT_IMPLEMENTED, L"Unsupported convolution method");
    }
    }

    if (blockUntilReady)
    {
        if ((m_idxUpdateFilterLatest == m_idxUpdateFilter) && (m_accumulatedArgs.updatesCnt != 0))
        {
            m_xFadeStarted.Lock(50);
        }
    }

    return AMF_OK;
}
//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::UpdateResponseFD(
    float* ppBuffers[],
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],
    const amf_uint32 operationFlags
)
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    return AMF_NOT_SUPPORTED;
}
////-------------------------------------------------------------------------------------------------
#ifndef TAN_NO_OPENCL

AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::UpdateResponseFD(
    cl_mem ppBuffers[],
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],
    const amf_uint32 operationFlags
)
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    return AMF_NOT_SUPPORTED;
}

#else

AMF_RESULT AMF_STD_CALL TANConvolutionImpl::UpdateResponseFD(
    const AMFBuffer * ppBuffer[],
    amf_size numOfSamplesToProcess,
    const amf_uint32 flagMasks[],   // Masks of flags from enum TAN_CONVOLUTION_CHANNEL_FLAG, can be NULL.
    const amf_uint32 operationFlags // Mask of flags from enum TAN_CONVOLUTION_OPERATION_FLAG.
    )
{
	AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);

    return AMF_NOT_SUPPORTED;
}

#endif


// Process direct (no update required), system memory buffers:
AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::ProcessDirect(
    float* ppImpulseResponse[],
    float* inputData[],
    float* outputData[],
    amf_size numOfSamplesToProcess,
    amf_size *pNumOfSamplesProcessed,
    int *nzFirstLast
    )
{
    switch (m_eConvolutionMethod) {
    case TAN_CONVOLUTION_METHOD_TIME_DOMAIN:
        {
            int firstNZ = 0;
            int lastNZ = m_length;
            if (numOfSamplesToProcess > m_length)
                numOfSamplesToProcess = m_length;

            //float **sampleHistory = m_tdFilterState[0]->m_SampleHistory;
            int *sampHistPos = m_tdFilterState[0]->m_sampHistPos;

            for (amf_uint32 iChan = 0; iChan < m_iChannels; iChan++){

                if (nzFirstLast != NULL) {
                    firstNZ = nzFirstLast[iChan * 2];
                    lastNZ = nzFirstLast[iChan * 2 + 1];
                }

#ifndef TAN_NO_OPENCL
                ovlTimeDomain(m_tdFilterState[0], iChan, ppImpulseResponse[iChan], firstNZ, lastNZ, inputData[iChan], outputData[iChan],
                               sampHistPos[iChan], numOfSamplesToProcess, m_length);
#else
                THROW_NOT_IMPLEMENTED;

                return AMF_NOT_IMPLEMENTED;
#endif

                sampHistPos[iChan] += numOfSamplesToProcess;
            }

            *pNumOfSamplesProcessed = numOfSamplesToProcess;
            return AMF_OK;
        }
        break;
    default:
        return AMF_NOT_SUPPORTED;
        break;
    }

    return AMF_NOT_SUPPORTED;
}

#ifndef TAN_NO_OPENCL

// Process direct (no update required),  OpenCL cl_mem  buffers:
AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::ProcessDirect(
    cl_mem* ppImpulseResponse[],
    cl_mem* ppBufferInput[],
    cl_mem* ppBufferOutput[],
    amf_size numOfSamplesToProcess,
    amf_size *pNumOfSamplesProcessed,
    int *nzFirstLast
    )
{
    return AMF_NOT_SUPPORTED;
}

#else

// Process direct (no update required),  OpenCL cl_mem  buffers:
AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::ProcessDirect(
    const AMFBuffer * ppImpulseResponse[],
    const AMFBuffer * ppBufferInput[],
    AMFBuffer * ppBufferOutput[],
    amf_size numOfSamplesToProcess,
    amf_size *pNumOfSamplesProcessed,
    int *nzFirstLast
    )
{
    THROW_NOT_IMPLEMENTED;

    return AMF_NOT_SUPPORTED;
}

#endif

#ifndef TAN_NO_OPENCL

AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::Process(
    float* ppBufferInput[],
    cl_mem pBufferOutput[],
    amf_size numOfSamplesToProcess,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32 flagMasks[],
    amf_size *pNumOfSamplesProcessed
)   // output cl_mem, input system memory
{
    AMF_RETURN_IF_FALSE(m_doProcessOnGpu, AMF_NOT_SUPPORTED);
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);

    AMF_RETURN_IF_FALSE(ppBufferInput != NULL, AMF_INVALID_ARG, L"pBufferInput == NULL");
    AMF_RETURN_IF_FALSE(pBufferOutput != NULL, AMF_INVALID_ARG, L"pBufferOutput == NULL");

    AMF_RESULT res = AMF_OK;

    TANSampleBuffer inBuf, outBuf;
    inBuf.ReferHostChannels(ppBufferInput);
    outBuf.ReferCLChannels(pBufferOutput);

    return Process(inBuf, outBuf, numOfSamplesToProcess, flagMasks, pNumOfSamplesProcessed);
}

#else

AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::Process(
    float* ppBufferInput[],
    AMFBuffer * ppBufferOutput[],
    amf_size numOfSamplesToProcess,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32 flagMasks[],
    amf_size *pNumOfSamplesProcessed
    )
{
    AMF_RETURN_IF_FALSE(m_doProcessOnGpu, AMF_NOT_SUPPORTED);
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);

    AMF_RETURN_IF_FALSE(ppBufferInput != nullptr, AMF_INVALID_ARG, L"pBufferInput == NULL");
    AMF_RETURN_IF_FALSE(ppBufferOutput != nullptr, AMF_INVALID_ARG, L"pBufferOutput == NULL");

    {
        TANSampleBuffer inBuf, outBuf;
		inBuf.ReferHostChannels(ppBufferInput);
		outBuf.ReferAMFChannels(ppBufferOutput);

		AMF_RETURN_IF_FAILED(Process(inBuf, outBuf, numOfSamplesToProcess, flagMasks, pNumOfSamplesProcessed));
	}

	return AMF_OK;
}

#endif

AMF_RESULT  AMF_STD_CALL    TANConvolutionImpl::Process(
    float* ppBufferInput[],
    float* ppBufferOutput[],
    amf_size numOfSamplesToProcess,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32 flagMasks[],
    amf_size *pNumOfSamplesProcessed
)   // input and output system memory
{
    /*for(int filter = 0; filter < N_FILTER_STATES; ++filter)
    {
        for(int channel = 0; channel < m_iChannels; ++channel)
        {
            m_FilterState[filter].DebugPrint(
                ES + __FUNCTION__ + std::to_string(__LINE__),
                channel
                );
        }
    }*/

    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);

    AMF_RETURN_IF_FALSE(ppBufferInput != NULL, AMF_INVALID_ARG, L"ppBufferInput == NULL");
    AMF_RETURN_IF_FALSE(ppBufferOutput != NULL, AMF_INVALID_ARG, L"ppBufferOutput == NULL");

    // process
    TANSampleBuffer inBuf, outBuf;
    inBuf.ReferHostChannels(ppBufferInput);
    outBuf.ReferHostChannels(ppBufferOutput);

    return Process(inBuf, outBuf, numOfSamplesToProcess, flagMasks, pNumOfSamplesProcessed);
}

//-------------------------------------------------------------------------------------------------
#ifndef TAN_NO_OPENCL

AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::Process(
    cl_mem pBufferInput[],
    cl_mem pBufferOutput[],
    amf_size numOfSamplesToProcess,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32 flagMasks[],
    amf_size *pNumOfSamplesProcessed
)   // input and output cl_mem
{
    return AMF_NOT_IMPLEMENTED;
}

#else

AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::Process(
    const AMFBuffer * pBufferInput[],
    AMFBuffer * pBufferOutput[],
    amf_size numOfSamplesToProcess,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32 flagMasks[],
    amf_size *pNumOfSamplesProcessed
    )
{
    THROW_NOT_IMPLEMENTED;

    return AMF_NOT_IMPLEMENTED;
}

#endif

//-------------------------------------------------------------------------------------------------
AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::Process(
    const TANSampleBuffer & pBufferInput,
    TANSampleBuffer & pBufferOutput,
    amf_size numOfSamplesToProcess,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32 flagMasks[],
    amf_size *pNumOfSamplesProcessed
)
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);

    AMFLock lock(&m_sectProcess);

    AMF_RESULT res = AMF_OK;

    AMF_RETURN_IF_FALSE(m_idxFilter >= 0, AMF_NOT_INITIALIZED,
                        L"Update() method must be called prior any calls to Process()");

    AMF_RESULT ret = AMF_OK;
    if (pNumOfSamplesProcessed)
    {
        *pNumOfSamplesProcessed = 0;
    }

	amf_size samplesProcessed = 0;

    TANSampleBuffer xFadeBuffs[2];
    xFadeBuffs[0] = pBufferOutput.IsHost() ? pBufferOutput : mFadeSubbufers[0];
    xFadeBuffs[1] = pBufferOutput.IsHost() ? m_pXFadeSamples : mFadeSubbufers[1];

    //PrintReducedFloatArray("pBufferInput 0bf", pBufferInput.GetHostBuffers()[0], samplesProcessed * sizeof(float));
    //PrintReducedFloatArray("pBufferInput 1bf", pBufferInput.GetHostBuffers()[1], samplesProcessed * sizeof(float));
    //PrintReducedFloatArray("pBufferOutput 0bf", pBufferOutput.GetHostBuffers()[0], samplesProcessed * sizeof(float));
    //PrintReducedFloatArray("pBufferOutput 1bf", pBufferOutput.GetHostBuffers()[1], samplesProcessed * sizeof(float));

    //PrintReducedFloatArray("xfade 0bf", xFadeBuffs[0].GetHostBuffers()[0], samplesProcessed * sizeof(float));
    //PrintReducedFloatArray("xfade 1bf", xFadeBuffs[1].GetHostBuffers()[0], samplesProcessed * sizeof(float));

    //todo: why these does not work? investigate, shoud be fixed!
    /*TANSampleBuffer xFadeBuffs[2] = {
        pBufferOutput.IsHost() ? pBufferOutput : mFadeSubbufers[0],
        pBufferOutput.IsHost() ? m_pXFadeSamples : mFadeSubbufers[1]
        };*/

    // Crossfade should never start if there is a pending TD->FD  (m_accumulatedArgs.updatesCnt == 0)
	bool doCrossFade = false;

    int bufSz = (1 << m_log2bsz);
	int fadeLength = bufSz; //hack  48000; // bufSz;  // 2048;

    //todo: note: refactored after merge with beta-cross-platform, check

	//if (m_eConvolutionMethod == (TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM || TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM) && m_bUseProcessFinalize) {
	if ((m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM ||
		m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM ||
		m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL)
		&& m_bUseProcessFinalize) {
		doCrossFade = false;
		if (ReadyForIRUpdate() && m_DelayedUpdate == 0) {
			m_DelayedUpdate = true;
			m_curCrossFadeSample = 0;
		}
		else {
			doCrossFade = m_DelayedUpdate;
		}
	}
	else
    {
        //doCrossFade = ReadyForIRUpdate();

        doCrossFade = false;
        if (ReadyForIRUpdate() && m_DelayedUpdate == false) {
            m_DelayedUpdate = true;
            m_curCrossFadeSample = 0;
        }
        else {
            if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM) {
                if ((m_currentDataPartition % m_2ndBufSizeMultiple) == 0) {
                    doCrossFade = m_DelayedUpdate;
                    //fadeLength = m_2ndBufSizeMultiple * bufSz;
                }
            }
            else {
                doCrossFade = m_DelayedUpdate;
            }
        }
	}

    m_CrossFading = doCrossFade;
	if (doCrossFade) //IR_UPDATE_DETECTED_STATE;
	{
        PrintDebug("DoCrossfade...");

		AMFLock lock(&m_sectUpdate);

		//m_DelayedUpdate = 0;
		// new responses available (obtained in the Update() method).
		// We've switched to a new filter response, so we need to cross fade from old IR to the new one
		//advance indices modulo 3:
		if (m_curCrossFadeSample == 0) {
			m_idxPrevFilter = m_idxFilter; // old filter needed for cross fade
			m_idxFilter = (m_idxFilter + N_FILTER_STATES + 1) % N_FILTER_STATES; // new (updated) impulse response filter
																				 //m_idxUpdateFilter = (m_idxUpdateFilter + N_FILTER_STATES + 1) % N_FILTER_STATES; // slot for next update
			m_idxUpdateFilter = (m_idxUpdateFilter + N_FILTER_STATES + 1) % N_FILTER_STATES; // slot for next update
		}

		///m_xFadeStarted.SetEvent();
		if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED) {
			m_DelayedUpdate = false;
			// For non-uniform partitioned convolution, there is no control states needed to be passed , just triggering the crossfade and it internally takes care of the states
			m_startNonUniformConvXFade = true;
			ret = ProcessInternal(m_idxFilter, pBufferInput, pBufferOutput,
				numOfSamplesToProcess, flagMasks, &samplesProcessed);
		}
		else if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL)
		{
			m_DelayedUpdate = false;
			// Calculate the current output using the old IR, time advances so that current input frame is stored and is part of the history buffer.
			// No skip stage is selected, and since the crossfade state is set to one, the crossfade specefic accum buffer (cmad_accum_xf_) is used
			ret = ProcessInternal(m_idxPrevFilter, pBufferInput, pBufferOutput,
				numOfSamplesToProcess, flagMasks, &samplesProcessed, 0, 1, 0, 1);
			RETURN_IF_FAILED(ret);

			// Run the MAD tail for the new IR and, no output is generated (skip_stage == 1), time not advancing at this stage , it has been advanced in the previous call
			ret = ProcessInternal(m_idxFilter, pBufferInput, xFadeBuffs[0],
				numOfSamplesToProcess, flagMasks, &samplesProcessed, 1, 0, 1);
			RETURN_IF_FAILED(ret);

			m_doHeadTailXfade = true;// Real crossfade will be performed when next input buffer is received
		}
		else if (m_eConvolutionMethod == (TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM || TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM) && m_bUseProcessFinalize) {

			//new head data, old filter
			ret = ProcessInternal(m_idxPrevFilter, pBufferInput, xFadeBuffs[0],
				numOfSamplesToProcess, flagMasks, &samplesProcessed, 0, 1, 0, 1);
			RETURN_IF_FAILED(ret);

			//new head data, new filter
			ret = ProcessInternal(m_idxFilter, pBufferInput, xFadeBuffs[1],
				numOfSamplesToProcess, flagMasks, &samplesProcessed, 1,0, 1 );
			RETURN_IF_FAILED(ret);

			//cross fade
			//AMF_RETURN_IF_FAILED(Crossfade(pBufferOutput, numOfSamplesToProcess, m_curCrossFadeSample, fadeLength));
			//}
			AMF_RETURN_IF_FAILED(Crossfade(pBufferOutput, samplesProcessed, m_curCrossFadeSample, fadeLength));
			m_curCrossFadeSample += samplesProcessed;
			if (m_curCrossFadeSample >= fadeLength) {
				m_DelayedUpdate = false;
				m_curCrossFadeSample = 0;
			}

		}
        else
		{
			// for non head-tail case, do the crossfade process now and back to normal operation on the next input buffer
			// last previous conv run, do not advance the internal Graal timer
			ret = ProcessInternal(
                m_idxPrevFilter,
                pBufferInput,
                xFadeBuffs[0],
				numOfSamplesToProcess,
                flagMasks,
                &samplesProcessed,
                0,
                0
                );
			RETURN_IF_FAILED(ret);

            //PrintReducedFloatArray("xfade 0", xFadeBuffs[0].GetHostBuffers()[0], samplesProcessed * sizeof(float));

            // new conv run, previous input, advance the internal Graal timer
			ret = ProcessInternal(
                m_idxFilter,
                pBufferInput,
                xFadeBuffs[1],
				numOfSamplesToProcess,
                flagMasks,
                &samplesProcessed,
                1
                );
			RETURN_IF_FAILED(ret);

            //PrintReducedFloatArray("xfade 1", xFadeBuffs[1].GetHostBuffers()[0], samplesProcessed * sizeof(float));

			AMF_RETURN_IF_FAILED(Crossfade(pBufferOutput, numOfSamplesToProcess, m_curCrossFadeSample, fadeLength));
            m_curCrossFadeSample += samplesProcessed;
			if (m_curCrossFadeSample >= fadeLength) {
				m_DelayedUpdate = false;
				m_curCrossFadeSample = 0;
			}

            //PrintReducedFloatArray("aft Crossfade", pBufferOutput.GetHostBuffers()[0], samplesProcessed * sizeof(float));
            //PrintReducedFloatArray("aft Crossfade", pBufferOutput.GetHostBuffers()[1], samplesProcessed * sizeof(float));
		}
	}
	else if (m_doHeadTailXfade) //HEAD_TAIL_CROSS_FADE_STATE
	{
        PrintDebug("DoHeadTailXfade...");

		// Only for the head-tail algorithm, crossfade process has started before and will finish when this step is over
		m_doHeadTailXfade = false; // reset the flag
		m_DelayedUpdate = false;

		 // Avoiding useless computation by not launching the FDL accum kernel with the old IRs during IR update (skip_stage==2)
		 // crossfade_state == 2 means it uses the data accumulated in the cmad_accum_xf_ when the previous frame was received
		ret = ProcessInternal(m_idxPrevFilter, pBufferInput, xFadeBuffs[1],
			numOfSamplesToProcess, flagMasks, &samplesProcessed, 0, 0, 2, 2);
		RETURN_IF_FAILED(ret);

		// regular convolution using the previous input
		ret = ProcessInternal(m_idxFilter, pBufferInput, xFadeBuffs[0],
			numOfSamplesToProcess, flagMasks, &samplesProcessed, 1);
		RETURN_IF_FAILED(ret);

		// cross fade old to new:
		AMF_RETURN_IF_FAILED(Crossfade(pBufferOutput, samplesProcessed, m_curCrossFadeSample, fadeLength));
	}
	else //REGULAR_PROCESS_STATE;
    {
        PrintDebug("REGULAR_PROCESS_STATE...");

        // wakeup update thread. New IR updates are allowed after the conv process completely done with crossfade
        PrintDebug("1429 m_xFadeStarted.SetEvent()");
        m_xFadeStarted.SetEvent();

        ret = ProcessInternal(
            m_idxFilter,
            pBufferInput,
            pBufferOutput,
            numOfSamplesToProcess,
            flagMasks,
            &samplesProcessed
            );

        //PrintReducedFloatArray("aft ProcessInternal0", pBufferOutput.GetHostBuffers()[0], samplesProcessed * sizeof(float));
        //PrintReducedFloatArray("aft ProcessInternal1", pBufferOutput.GetHostBuffers()[1], samplesProcessed * sizeof(float));

        if (!m_bUseProcessFinalize)
		{
            PrintDebug("1442 m_procReadyForNewResponsesEvent.SetEvent()");
            m_procReadyForNewResponsesEvent.SetEvent();
        }
    }

    if (pNumOfSamplesProcessed)
    {
        *pNumOfSamplesProcessed = samplesProcessed;
    }

    return AMF_OK;

ErrorHandling:
    AMF_RETURN_IF_FAILED(ret);
    return ret;
}

AMF_RESULT  AMF_STD_CALL TANConvolutionImpl::ProcessFinalize()
{
    AMF_RESULT ret = AMF_OK;
    switch (m_eConvolutionMethod)
    {
    case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM:
    case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM:
    {

        if (m_CrossFading) {
            // old data tail, new filter
            ovlNUPProcessTail(m_nupFilterState[m_idxPrevFilter]);
        }
        ovlNUPProcessTail(m_nupFilterState[m_idxFilter], m_CrossFading);

        if (!m_CrossFading)
        {
            PrintDebug("1129 m_procReadyForNewResponsesEvent.SetEvent()");
            m_procReadyForNewResponsesEvent.SetEvent();
        }
    }
    break;
    //
    case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_HEAD_TAIL:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:
    {
        graal::CGraalConv*graalConv = (graal::CGraalConv*)m_graal_conv;
        ret = graalConv->processFinalize();
    }
    break;
    default:
        break;
    }

    return ret;
}



//-------------------------------------------------------------------------------------------------
AMF_RESULT AMF_STD_CALL TANConvolutionImpl::GetNextFreeChannel(
    amf_uint32 *pChannelIndex,
    const amf_uint32 flagMasks[]
    )
{
    AMF_RETURN_IF_FALSE(m_initialized, AMF_NOT_INITIALIZED);
    AMF_RETURN_IF_FALSE(pChannelIndex != NULL, AMF_INVALID_ARG, L"pChannelIndex == NULL");

    *pChannelIndex = -1;

    if (!flagMasks)
    {
        // No stopped channels!
        return AMF_NOT_FOUND;
    }

    for (amf_uint32 channelId = 0; channelId < m_iChannels; channelId++){
        if (flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT) {
            if (m_tailLeftOver[channelId] <= 0) {
                *pChannelIndex = channelId;
                return AMF_OK;
            }
        }
    }

    return AMF_NOT_FOUND;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT  TANConvolutionImpl::Init(
    TAN_CONVOLUTION_METHOD convolutionMethod,
    amf_uint32 responseLengthInSamples,
    amf_uint32 bufferSizeInSamples,
    amf_uint32 channels,
    bool doProcessingOnGpu
)
{
//todo: investigate (block?) usage of CL and AMF contexts simultaneously
#ifndef TAN_NO_OPENCL
    if
    (
        (doProcessingOnGpu && !m_pContextTAN->GetOpenCLContext())
        ||
        (!doProcessingOnGpu && m_pContextTAN->GetOpenCLContext())
    )
    {
        // Attempting to do processing in a state not initialized with the context
        return AMF_WRONG_STATE;
    }
#else
    if
    (
        //todo: investigate usage of CL and AMF context simultaneously
        (doProcessingOnGpu && !m_pContextTAN->GetAMFContext())
        //||
        //(!doProcessingOnGpu && m_pContextTAN->GetAMFContext())
    )
    {
        //todo: investigate why incorrect behaviour

        // Attempting to do processing in a state not initialized with the context
        return AMF_WRONG_STATE;
    }
#endif

    AMFLock lock(&m_sect);

	AMF_RETURN_IF_FAILED(TANCreateMath(m_pContextTAN, &m_pMath, true));
	AMF_RETURN_IF_FAILED(m_pMath->Init());

    AMF_RETURN_IF_FALSE(responseLengthInSamples > 0, AMF_INVALID_ARG, L"lengthInSamples == 0");
    AMF_RETURN_IF_FALSE(bufferSizeInSamples > 0, AMF_INVALID_ARG, L"bufferSizeInSamples == 0");
    AMF_RETURN_IF_FALSE(channels > 0, AMF_INVALID_ARG, L"channels == 0");
#if defined (_M_X64)
    AMF_RETURN_IF_FALSE(!doProcessingOnGpu || __popcnt64(bufferSizeInSamples) == 1, AMF_INVALID_ARG,
                        L"bufferSizeInSamples must be power of 2");
#else
    #if defined (_MSC_VER)
    AMF_RETURN_IF_FALSE(!doProcessingOnGpu || __popcnt(bufferSizeInSamples) == 1, AMF_INVALID_ARG,
    #else
    AMF_RETURN_IF_FALSE(!doProcessingOnGpu || __builtin_popcountll(bufferSizeInSamples) == 1, AMF_INVALID_ARG,
    #endif
        L"bufferSizeInSamples must be power of 2");
#endif
    AMF_RETURN_IF_FALSE(!m_initialized, AMF_ALREADY_INITIALIZED, L"Already initialized");
    AMF_RETURN_IF_FALSE((NULL != m_pContextTAN), AMF_WRONG_STATE,
                        L"Cannot initialize after termination");

	m_bUseProcessFinalize = (convolutionMethod & TAN_CONVOLUTION_METHOD_USE_PROCESS_FINALIZE) != 0;
	m_eConvolutionMethod = convolutionMethod = TAN_CONVOLUTION_METHOD(convolutionMethod & ~TAN_CONVOLUTION_METHOD_USE_PROCESS_FINALIZE);

	// Substitute methods not implemented on CPU:
	if (!doProcessingOnGpu) {
		switch (convolutionMethod) {
		case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED:
		case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED:
		case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:
			m_eConvolutionMethod = convolutionMethod = TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM;
			break;
		case TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED:
		case TAN_CONVOLUTION_METHOD_FFT_NONUNIFORM_PARTITIONED:
			m_eConvolutionMethod = convolutionMethod = TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM;
			break;
        default:
            //return AMF_UNEXPECTED;
            break;
		}
	}

	AMF_RETURN_IF_FALSE(convolutionMethod < TAN_CONVOLUTION_METHOD_FFT_NONUNIFORM_PARTITIONED, AMF_NOT_SUPPORTED,
		L"convolutionMethod isn't supported");

    m_doProcessOnGpu = doProcessingOnGpu;

    AMF_RESULT res = AMF_OK;

    m_eConvolutionMethod = convolutionMethod;
    m_iLengthInSamples = responseLengthInSamples;
    m_iBufferSizeInSamples = bufferSizeInSamples;
    m_iChannels = channels;

    if(doProcessingOnGpu )
    {
		bool kernelLoaded =


#ifndef TAN_NO_OPENCL

            GetOclKernel(
                m_pKernelCrossfade,
                m_pProcContextAMF,
                m_pContextTAN->GetOpenCLGeneralQueue(),
                "crossfade",
                Crossfading_Str,
                CrossfadingCount,
                "crossfade",
                ""

                );

#else
		    GetOclKernel(
                mKernelCrossfade,
                m_pContextTAN->GetAMFGeneralQueue(),

                "crossfade",
                Crossfading_Str,
                CrossfadingCount,
                "crossfade",

                "",
				TANContextImplPtr(m_pContextTAN)->GetFactory()
                );
#endif

        AMF_RETURN_IF_FALSE(kernelLoaded, AMF_FAIL, L"Failed to compile crossfade kernel");
    }

    amf_int64 tmp = 0;
    GetProperty(TAN_OUTPUT_MEMORY_TYPE, &tmp);
    m_eOutputMemoryType = (AMF_MEMORY_TYPE)tmp;

    // Initialize TAN FFT objects.
    if (convolutionMethod == TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD)
    {
        AMF_RETURN_IF_FAILED(TANCreateFFT(m_pContextTAN, &m_pUpdateTanFft, false));
        AMF_RETURN_IF_FAILED(m_pUpdateTanFft->Init());
        if (m_pProcContextAMF == m_pUpdateContextAMF)
        {
            m_pTanFft = m_pUpdateTanFft;
        }
        else {
            AMF_RETURN_IF_FAILED(TANCreateFFT(m_pContextTAN, &m_pTanFft , true));
            AMF_RETURN_IF_FAILED(m_pTanFft->Init());
        }
    }

	if (convolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM || TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM)
	{
		AMF_RETURN_IF_FAILED(TANCreateFFT(m_pContextTAN, &m_pUpdateTanFft, false));
		AMF_RETURN_IF_FAILED(m_pUpdateTanFft->Init());
		if (m_pProcContextAMF == m_pUpdateContextAMF)
		{
			m_pTanFft = m_pUpdateTanFft;
		}
		else {
			AMF_RETURN_IF_FAILED(TANCreateFFT(m_pContextTAN, &m_pTanFft, true));
			AMF_RETURN_IF_FAILED(m_pTanFft->Init());
		}
	}

    amf_uint32 len = 1;
	m_log2bsz = 0;
	while (len <  bufferSizeInSamples) {
		len <<= 1;
		++m_log2bsz;
	}

    int log2len = 0;
	len = 1;
	// use next bigger power of 2:
    while (len < responseLengthInSamples){
        len <<= 1;
        ++log2len;
    }

    if (m_initialized){
        deallocateBuffers();
    }

	m_RunningChannels = m_iChannels;
    m_length = len;
    m_log2len = log2len;

    AMF_RETURN_IF_FAILED(
        allocateBuffers()
        );

    m_initialized = true;

    m_updThread.Init();
    m_updThread.Start();

#ifdef USE_TAIL_THREAD
	m_tailThread.Init();
	m_tailThread.Start();
#endif

    /*
    for(int i=0; i<3; ++i)
    {
        ovlAddFilterState * state = &m_FilterState[i];

        float **filter = state->m_internalFilter;
        float **overlap = state->m_internalOverlap;

        //PrintFloatArray(
            "::ovlAddProcess->filter[0] - init",
            filter[0],
            64
            );
        //PrintFloatArray(
            "::ovlAddProcess->filter[1] - init",
            filter[1],
            64
            );

        //PrintFloatArray(
            "::ovlAddProcess->overlap",
            overlap[0],
            64
            );
        //PrintFloatArray(
            "::ovlAddProcess->overlap",
            overlap[1],
            64
            );
    }
    */

    return AMF_OK;
}

//-------------------------------------------------------------------------------------------------
AMF_RESULT amf::TANConvolutionImpl::Flush(amf_uint32 filterStateId, amf_uint32 channelId)
{
	// If we flush the current set, we need to synchronize with process().
	if (filterStateId == m_idxFilter)
	{
		m_sectProcess.Lock();
		if (m_eConvolutionMethod != TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD &&
			m_eConvolutionMethod != TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM &&
			m_eConvolutionMethod != TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM &&
			m_eConvolutionMethod != TAN_CONVOLUTION_METHOD_TIME_DOMAIN &&
			m_eConvolutionMethod != TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED
			) {
			graal::CGraalConv* pGraalConv = (graal::CGraalConv*)m_graal_conv;
			AMF_RETURN_IF_FAILED(pGraalConv->finishProcess(), L"Sync failed");
		}
	}

	if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD)
	{
        m_FilterState[filterStateId].FlushOverlap(channelId);
	}
	else if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM)
	{
		auto pFilterState = m_upFilterState[filterStateId];
		float **overlap = pFilterState->m_Overlap;
		memset(overlap[channelId], 0, m_length * sizeof(float));

		int nParts = (1 << m_log2len) / (1 << m_log2bsz);

		int BZ = 4; // buffer size scale factor
		int EX = 0;
		if (m_TransformType == TRANSFORMTYPE_FFTCOMPLEX) {
			BZ = 4;
			EX = 0;
		}
		else if (m_TransformType == TRANSFORMTYPE_FFTREAL) {
			BZ = 2;
			EX = PARTITION_PAD_FFTREAL;
		}
		else if (m_TransformType == TRANSFORMTYPE_FFTREAL_PLANAR) {
			BZ = 2;
			EX = PARTITION_PAD_FFTREAL_PLANAR;
		}

		int bufLen = 2 * (BZ * m_length + EX*nParts);
		memset(pFilterState->m_DataPartitions[channelId], 0, sizeof(float)*bufLen);

	}
	else if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM)
	{
		auto pFilterState = m_upFilterState[filterStateId];
		float **overlap = pFilterState->m_Overlap;
		memset(overlap[channelId], 0, m_length * sizeof(float));

		int nParts = (1 << m_log2len) / (1 << m_log2bsz);
		nParts /= m_2ndBufSizeMultiple; //NU

		int BZ = 2; // buffer size scale factor
		int EX = PARTITION_PAD_FFTREAL_PLANAR;

		int bufLen = 2 * (BZ * m_length + EX*nParts);
		memset(pFilterState->m_DataPartitions[channelId], 0, sizeof(float)*bufLen);
	}
	else if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_TIME_DOMAIN)
	{

		m_tdFilterState[filterStateId]->m_sampHistPos[channelId] = 0;
		memset(m_tdFilterState[filterStateId]->m_SampleHistory[channelId], 0, m_length * sizeof(float));
	}
	else if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED)
	{
        THROW_NOT_IMPLEMENTED;

        return AMF_NOT_IMPLEMENTED;

		/*
        // Flushing the non-uniform Graal object
		AMF_RETURN_IF_FALSE(m_nonUniformGraal.flush(channelId) == 0, AMF_UNEXPECTED, L"Flushing non-uniform convolution failed")
        */
	}
	else {
		// Flushing of Graal objects.
		graal::CGraalConv* pGraalConv = (graal::CGraalConv*)m_graal_conv;
		AMF_RETURN_IF_FALSE(pGraalConv->flush(channelId) == GRAAL_SUCCESS, AMF_UNEXPECTED,
			L"Flushing failed");
	}

	// If we flush the current set, we need to synchronize with process().
	if (filterStateId == m_idxFilter)
	{
		m_sectProcess.Unlock();
	}

    return AMF_OK;
}
//-------------------------------------------------------------------------------------------------
AMF_RESULT AMF_FAST_CALL TANConvolutionImpl::Crossfade(
    TANSampleBuffer & pBufferOutput,
    amf_size numOfSamplesToProcess,
	int curFadeSample,
	int fadeLength
)
{
    PrintDebug(__FUNCTION__);

    //auto crossfadeQueue = m_pContextTAN->GetGeneralQueue(); //todo: think about reason, kernel was created with general queue?
    auto crossfadeQueue = m_pContextTAN->GetConvQueue();

    if(pBufferOutput.IsComputeBuffer())
    {
		int index = 0;

#ifndef TAN_NO_OPENCL
        int status;
        amf_size global[3] = { numOfSamplesToProcess, m_iChannels, 0 };
        amf_size local[3] = { (numOfSamplesToProcess>256) ? 256 : numOfSamplesToProcess, 1, 0 };
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(m_pKernelCrossfade, index++, sizeof(cl_mem), &m_pCLXFadeMasterBuf[0]), L"Failed to set OpenCL argument");
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(m_pKernelCrossfade, index++, sizeof(cl_mem), &m_pCLXFadeMasterBuf[1]), L"Failed to set OpenCL argument");
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(m_pKernelCrossfade, index++, sizeof(int), &numOfSamplesToProcess), L"Failed to set OpenCL argument");
		status = clEnqueueNDRangeKernel(crossfadeQueue, m_pKernelCrossfade, 2, NULL, global, local, 0, NULL, NULL);
		if (status != CL_SUCCESS)
        {
            printf("Failed to enqueue OCL kernel");
            return AMF_FAIL;
        }

        for (amf_uint32 c = 0; c < m_iChannels; c++)
        {
            int status = clEnqueueCopyBuffer(
				crossfadeQueue,
                m_pCLXFadeMasterBuf[1], //todo, ivm: why [1]?
                pBufferOutput.GetCLBuffers()[c],
                c * m_iBufferSizeInSamples * sizeof(float),
                0,
                numOfSamplesToProcess * sizeof(float),
                0,
                NULL,
                NULL
                );

            CHECK_OPENCL_ERROR(status, "copy failed.");
        }

#else

        amf_size global[3] = { numOfSamplesToProcess, m_iChannels, 0 };
        amf_size local[3] = { (numOfSamplesToProcess>256) ? 256 : numOfSamplesToProcess, 1, 0 };

        AMF_RETURN_IF_FAILED(mKernelCrossfade->SetArgBuffer(index++, mAMFCLXFadeMasterBuffers[0], AMF_ARGUMENT_ACCESS_READWRITE));
        AMF_RETURN_IF_FAILED(mKernelCrossfade->SetArgBuffer(index++, mAMFCLXFadeMasterBuffers[1], AMF_ARGUMENT_ACCESS_READWRITE));
        AMF_RETURN_IF_FAILED(mKernelCrossfade->SetArgInt32(index++, numOfSamplesToProcess));

        AMF_RETURN_IF_FAILED(
            mKernelCrossfade->Enqueue(2, nullptr, global, local)
            );

        PrintAMFArray("aft mAMFCLXFadeMasterBuffers[0]", mAMFCLXFadeMasterBuffers[0], crossfadeQueue, 64);
        PrintAMFArray("aft mAMFCLXFadeMasterBuffers[1]", mAMFCLXFadeMasterBuffers[1], crossfadeQueue, 64);

        for (amf_uint32 c = 0; c < m_iChannels; c++)
        {
            AMF_RETURN_IF_CL_FAILED(
                crossfadeQueue->CopyBuffer(
                    mAMFCLXFadeMasterBuffers[1], //todo, ivm: why [1]?
                    c * m_iBufferSizeInSamples * sizeof(float),
                    numOfSamplesToProcess * sizeof(float),
                    pBufferOutput.GetAMFBuffersReadWrite()[c],
                    0
                    )
                );
        }

#endif
    }
    else
    {
        // CPU Implementation
		float step = 1.0 / float(fadeLength);
		for (int n = 0; n < m_iChannels; n++) {
			if (!m_availableChannels[n]){ // !available == running
                float *pFltOut = pBufferOutput.GetHostBuffers()[n];
                float *pFltFade = m_pXFadeSamples.GetHostBuffers()[n];
				int j = curFadeSample;
                for (amf_size i = 0; i < numOfSamplesToProcess; i++,j++){
					float w1, w2;
					w1 = step * j;
					w2 = step * (fadeLength - j);
					pFltOut[i] = pFltFade[i] * w1 + pFltOut[i] * w2;
                }
            }
        }
    }
    return AMF_OK;
}
//-------------------------------------------------------------------------------------------------
AMF_RESULT TANConvolutionImpl::allocateBuffers()
{
    //allocate stuff:
    //...
#ifndef TAN_NO_OPENCL
    cl_context context = m_pContextTAN->GetOpenCLContext();
#else
    auto context = m_pContextTAN->GetAMFContext();
#endif

    AMF_MEMORY_TYPE memoryType = AMF_MEMORY_UNKNOWN;

    // internal pointer arrays used to shuffle buffer order
    // no need to allocate actual data buffers here:
	if (m_doProcessOnGpu)
    {
#ifndef TAN_NO_OPENCL
        memoryType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;
#else
        auto compute = m_pProcContextAMF ? m_pProcContextAMF : m_pUpdateContextAMF;
        assert(compute);

        memoryType = compute->GetMemoryType();
#endif
    }
    else
    {
        memoryType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST;
    }

    m_internalInBufs.AllocateChannels(
        m_iChannels,
        memoryType
        );
    m_internalOutBufs.AllocateChannels(
        m_iChannels,
        memoryType
        );

    m_availableChannels = new bool[m_iChannels];
    m_flushedChannels = new bool[m_iChannels];

    for (amf_uint32 i = 0; i < m_iChannels; i++) {
        m_availableChannels[i] = false;
        m_flushedChannels[i] = false;
    }

    m_tailLeftOver = new int[m_iChannels];
    memset(m_tailLeftOver, 0, sizeof(int)*m_iChannels);
    m_silence = new float[m_length];
    memset(m_silence, 0, sizeof(float)*m_length);

    m_updateArgs.Alloc(m_iChannels);
    m_uploadArgs.Alloc(m_iChannels);
    m_copyArgs.Alloc(m_iChannels);
    m_accumulatedArgs.Alloc(m_iChannels);

    switch (m_eConvolutionMethod)
    {
    case TAN_CONVOLUTION_METHOD_TIME_DOMAIN:
        for (int i = 0; i < N_FILTER_STATES; i++)
        {
            m_tdFilterState[i] = new tdFilterState;
            m_tdFilterState[i]->m_Filter = new float *[m_iChannels];
            m_tdFilterState[i]->m_SampleHistory = new float *[m_iChannels];
			m_tdFilterState[i]->m_sampHistPos = new int[m_iChannels];
			m_tdFilterState[i]->firstNz = new int[m_iChannels];
			m_tdFilterState[i]->lastNz = new int[m_iChannels];

            if(context)
            {
#ifndef TAN_NO_OPENCL
                m_tdFilterState[i]->SetupCL(m_iChannels);
#else
                THROW_NOT_IMPLEMENTED;

                return AMF_NOT_IMPLEMENTED;
#endif
            }
			else
            {
#ifndef TAN_NO_OPENCL
				m_tdFilterState[i]->DeallocateCL();
#else
                THROW_NOT_IMPLEMENTED;

                return AMF_NOT_IMPLEMENTED;
#endif
			}

        	m_tdInternalFilterState[i] = new tdFilterState;
            m_tdInternalFilterState[i]->SetupHost(m_iChannels);

			if (context != nullptr )
            {
#ifndef TAN_NO_OPENCL
                m_tdInternalFilterState[i]->SetupCL(m_iChannels);
#else
                m_tdInternalFilterState[i]->SetupAMF(m_iChannels);
#endif
			}
        }

        for (amf_uint32 n = 0; n < m_iChannels; n++)
        {
            for (int i = 0; i < N_FILTER_STATES; i++)
            {
                m_tdFilterState[i]->SetupHostData(n, m_length);

                if (context != nullptr)
                {
#ifndef TAN_NO_OPENCL
                    m_tdFilterState[i]->SetupCLData(context, n, m_length);
#else
                    m_tdFilterState[i]->SetupAMFData(context, memoryType, n, m_length);
#endif
                }

				m_tdInternalFilterState[i]->FreeHostData(n);

                if (context != nullptr)
				{
#ifndef TAN_NO_OPENCL
                    m_tdInternalFilterState[i]->FreeCLData(n);
#else
                    m_tdInternalFilterState[i]->FreeAMFData(n);
#endif
				}
            }
        }
        break;

    case TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD:
        m_OutSamples = new float *[m_iChannels];

        m_ovlAddLocalInBuffs.resize(m_iChannels);
        m_ovlAddLocalOutBuffs.resize(m_iChannels);

        // allocate state data for ovlAddProcess:
        for (int i = 0; i < N_FILTER_STATES; i++)
        {
            m_FilterState[i].Setup(m_iChannels);
        }

        for (amf_uint32 n = 0; n < m_iChannels; n++)
        {
            m_ovlAddLocalInBuffs[n].resize(m_length, 0);
            m_ovlAddLocalOutBuffs[n].resize(m_iBufferSizeInSamples, 0);

            m_OutSamples[n] = new float[2 * m_length];
            memset(m_OutSamples[n], 0, (2 * m_length) * sizeof(float));

            for (int i = 0; i < N_FILTER_STATES; i++)
            {
                m_FilterState[i].SetupFilter(n, 2 * m_length);

                if(i == 0)
                {
                    m_FilterState[i].SetupOverlap(n, m_length);
                }
                else
                {
                    m_FilterState[i].ReferOverlap(n, m_FilterState[0]);
                }
            }
        }

        for(int filter = 0; filter < N_FILTER_STATES; ++filter)
        {
            for(int channel = 0; channel < m_iChannels; ++channel)
            {
                m_FilterState[filter].DebugPrint(
                    ES + __FUNCTION__ + std::to_string(__LINE__) + " ["
                   + std::to_string(filter) + "] ["
                   + std::to_string(channel) + "]",
                    channel
                    );
            }
        }

        break;

	case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM:
	case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM:
	{
		int nParts = (1 << m_log2len) / (1 << m_log2bsz);
		nParts /= m_2ndBufSizeMultiple; //NU

		int BZ = 2; // buffer size scale factor
		int EX = PARTITION_PAD_FFTREAL_PLANAR;

		int partLen = m_2ndBufSizeMultiple*(1 << (m_log2bsz + 1)) + EX;

		m_dataRowLength = m_iChannels * 2 * partLen * sizeof(float);
		//To Do: for GPU round up to next 256...
		// To Do : clGetDEviceInfo ....CL_DEVICE_MEM_BASE_ADDR_ALIGN  ...clCreateSubBuffer error if not aligned :(
		if (m_doProcessOnGpu)
        {
			amf_uint32 align = 0;

#ifndef TAN_NO_OPENCL
			cl_device_id devIds[2];
			size_t nDevs;

			clGetContextInfo(context, CL_CONTEXT_DEVICES, sizeof(devIds), devIds, &nDevs);
			clGetDeviceInfo(devIds[0], CL_DEVICE_MEM_BASE_ADDR_ALIGN, sizeof(align), &align, NULL);
#else
            THROW_NOT_IMPLEMENTED;

            return AMF_NOT_IMPLEMENTED;

            /*if(AMF_OK == context->GetDevice(
#ifndef ENABLE_METAL
                amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
                amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
                )->GetProperty(
                CL_DEVICE_MEM_BASE_ADDR_ALIGN,
                &align
                ))
            {
				if (align > 0) {
				m_dataRowLength = ((m_dataRowLength + align) / align) * align;
			}*/
#endif

            if(align > 0)
            {
				m_dataRowLength = ((m_dataRowLength + align) / align) * align;
			}
		}

		int ret = 0;
		m_OutSamples = new float *[m_iChannels];
		m_OutSamplesXFade = new float *[m_iChannels];
		m_NUTailAccumulator = new float *[m_iChannels];
		m_NUTailSaved = new float *[m_iChannels];

		m_ovlAddLocalInBuffs.resize(m_iChannels);
		m_ovlAddLocalOutBuffs.resize(m_iChannels);

		float ** pParts = new float *[m_iChannels];
		float ** piParts = new float *[m_iChannels];
		float ** subParts = new float *[m_iChannels];
		float **workBuffer = new float *[m_iChannels];
		//cl_mem clOutput = nullptr;

		for (int i = 0; i < N_FILTER_STATES; i++) {
			m_nupFilterState[i] = new _ovlNonUniformPartitionFilterState;
			m_nupFilterState[i]->m_Filter = new float *[m_iChannels];
			m_nupFilterState[i]->m_Overlap = new float *[m_iChannels];
			m_nupFilterState[i]->m_internalFilter = new float *[m_iChannels];
			m_nupFilterState[i]->m_internalOverlap = new float *[m_iChannels];
			m_nupFilterState[i]->m_internalDataPartitions = piParts;
			m_nupFilterState[i]->m_DataPartitions = pParts;
			m_nupFilterState[i]->m_SubPartitions = subParts;
			m_nupFilterState[i]->m_workBuffer = workBuffer;
		}

		if (context != nullptr) {
			int bufLen = 2 * (BZ * m_length + EX*nParts);
			int outLen = 2 * (BZ * m_iBufferSizeInSamples + EX);

			//clOutput = clCreateBuffer(context, CL_MEM_READ_WRITE, m_iChannels * outLen * sizeof(float), nullptr, &ret);

		} //context != nullptr



		int bufLen = 2 * (BZ * m_length + EX*nParts);

		m_FilterTD = new float *[m_iChannels];

		for (int n = 0; n < m_iChannels; n++) {
			m_FilterTD[n] = new float[bufLen * sizeof(float)];
			memset(m_FilterTD[n], 0, bufLen * sizeof(float));
		}

		//Use aligned malloc for m_Filter and m_DataPartitions to speed up AV256 im PLanarMultiplyAccumulate...

        mNUPSize = bufLen;
        mNUPSize2 = partLen;

		for (amf_uint32 n = 0; n < m_iChannels; n++)
        {
			m_ovlAddLocalInBuffs[n].resize(m_length, 0);
			m_ovlAddLocalOutBuffs[n].resize(m_iBufferSizeInSamples, 0);

			m_OutSamples[n] = (float *)_mm_malloc(2 * m_length * sizeof(float),32);// new float[2 * m_length];
			memset(m_OutSamples[n], 0, (2 * m_length) * sizeof(float));

			m_OutSamplesXFade[n] = (float *)_mm_malloc( 2 * m_length * sizeof(float),32);// new float[2 * m_length];
			memset(m_OutSamplesXFade[n], 0, (2 * m_length) * sizeof(float));

			m_NUTailAccumulator[n] = (float *)_mm_malloc(2 * m_length * sizeof(float), 32);// new float[2 * m_length];
			memset(m_NUTailAccumulator[n], 0, (2 * m_length) * sizeof(float));

			m_NUTailSaved[n] = (float *)_mm_malloc(2 * m_length * sizeof(float), 32);// new float[2 * m_length];
			memset(m_NUTailSaved[n], 0, (2 * m_length) * sizeof(float));

			for (int i = 0; i < N_FILTER_STATES; i++) {
				m_nupFilterState[i]->m_Filter[n] = (float *)_mm_malloc( bufLen * sizeof(float), 32);// new float[bufLen];
				memset(m_nupFilterState[i]->m_Filter[n], 0, bufLen * sizeof(float));

				m_nupFilterState[i]->m_workBuffer[n] = (float *)_mm_malloc( partLen * sizeof(float), 32);
				memset(m_nupFilterState[i]->m_workBuffer[n], 0, partLen * sizeof(float));

				m_nupFilterState[i]->m_internalFilter[n] = m_nupFilterState[i]->m_Filter[n];

				if (i == 0) {
					m_nupFilterState[i]->m_Overlap[n] = new float[m_length];
					memset(m_nupFilterState[i]->m_Overlap[n], 0, m_length * sizeof(float));

					m_nupFilterState[0]->m_DataPartitions[n] = (float *)_mm_malloc( bufLen * sizeof(float), 32);// new float[bufLen];
					memset(m_nupFilterState[0]->m_DataPartitions[n], 0, bufLen * sizeof(float));

					m_nupFilterState[0]->m_SubPartitions[n] = (float *)_mm_malloc( partLen * sizeof(float), 32);// new float[bufLen];
					memset(m_nupFilterState[0]->m_SubPartitions[n], 0, partLen * sizeof(float));
				}

				else {
					m_nupFilterState[i]->m_Overlap[n] = m_nupFilterState[0]->m_Overlap[n];
				}
			}
		}

	}
	break;

    case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:
    {
        graal::CGraalConv*graalConv = nullptr;


        if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED)
        {
            graalConv = new graal::CGraalConv_clFFT(
#ifdef TAN_NO_OPENCL
                    TANContextImplPtr(m_pContextTAN)->GetFactory()
#endif
                    );
        }
        else
        {
            graalConv = new graal::CGraalConv(
#ifdef TAN_NO_OPENCL
                    TANContextImplPtr(m_pContextTAN)->GetFactory()
#endif
                    );
        }

        bool isPartitionedMethod =
            m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED ||
            m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED;

        int ret = graalConv->initializeConv(
            m_pContextTAN,
            m_pProcContextAMF,
            m_pUpdateContextAMF,
            m_iChannels,
            (int)m_length,
            (int)m_iBufferSizeInSamples,
            N_FILTER_STATES,
            isPartitionedMethod ? graal::ALG_UNIFORMED : graal::ALG_UNI_HEAD_TAIL
            );

        if(ret == GRAAL_EXPECTED_FAILURE)
        {
            return AMF_OPENCL_FAILED;
        }
        else if(ret == GRAAL_NOT_ENOUGH_GPU_MEM)
        {
            return AMF_OUT_OF_MEMORY;
        }
        else if(ret != GRAAL_SUCCESS)
        {
            return AMF_UNEXPECTED;
        }

        m_s_versions[0] = new int[m_iChannels];
        m_s_versions[1] = new int[m_iChannels];
        m_s_channels = new int[m_iChannels];

        m_n_delays_onconv_switch = isPartitionedMethod ? 0 : 2;
        m_graal_conv = graalConv;
    }
    break;

	case TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED:
	{
        THROW_NOT_IMPLEMENTED;

        return AMF_FAIL;

		/*
        m_nonUniformGraal.Init(m_pContextTAN, m_pProcContextAMF, m_pUpdateContextAMF, m_iChannels, m_iLengthInSamples, m_iBufferSizeInSamples, N_FILTER_STATES);
		*/
        m_s_channels = new int[m_iChannels];
	}
	break;

    default:
        AMF_RETURN_IF_FALSE(false, AMF_NOT_SUPPORTED, L"Convolution method not supported");
    }

	//if (m_doProcessOnGpu && m_eConvolutionMethod != TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED) // The non-uniform partitioned convolution runs crossfade internally and does not need TAN's OCL crossfade buffers
	if (m_doProcessOnGpu)
	{
        // allocate crossfade buffers on GPU memory
        for (int bufIdx = 0; bufIdx < 2; bufIdx++)
        {
            /*ivm: unneeded?
#ifndef TAN_NO_OPENCL
            mFadeSubbufers[bufIdx].AllocateChannels(
                m_iChannels,
                amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
                );
#else
            mFadeSubbufers[bufIdx].AllocateChannels(
                m_iChannels,
                mMemoryType
                );
#endif
            */

            // First create a big unified buffer
            amf_size singleBufSize = sizeof(float)*m_iBufferSizeInSamples;

            //auto crossfadeQueue = m_pContextTAN->GetGeneralQueue(); //todo: think about reason, kernel was created with general queue?
            auto crossfadeQueue = m_pContextTAN->GetConvQueue();

#ifndef TAN_NO_OPENCL
            cl_int clErr(CL_SUCCESS);
            m_pCLXFadeMasterBuf[bufIdx] = clCreateBuffer(
                m_pContextTAN->GetOpenCLContext(),
                CL_MEM_READ_WRITE,
                singleBufSize * m_iChannels,
                nullptr,
                &clErr
                );

            AMF_RETURN_IF_FALSE((clErr == CL_SUCCESS), AMF_FAIL, L"Could not create OpenCL buffer\n")

            mFadeSubbufers[bufIdx].AllocateChannels(
                m_iChannels,
                amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
                );
            //cl_mem *subBuffers(nullptr);

            // Then split the big buffer into small contiguous subbuffers
            for (amf_uint32 i = 0; i < m_iChannels; i++)
            {
                cl_buffer_region region;
                region.origin = i*singleBufSize;
                region.size = singleBufSize;

                mFadeSubbufers[bufIdx].GetCLBuffersReadWrite()[i] = clCreateSubBuffer(
                    m_pCLXFadeMasterBuf[bufIdx],
                    CL_MEM_READ_WRITE,
                    CL_BUFFER_CREATE_TYPE_REGION,
                    &region,
                    &clErr
                    );
                AMF_RETURN_IF_FALSE((clErr == CL_SUCCESS), AMF_FAIL, L"Could not create OpenCL subbuffer\n")
            }

            mFadeSubbufers[bufIdx].MarkBuffersAllocated();
#else
            amf::AMFBufferPtr buffer;

            AMF_RETURN_IF_FAILED(
                m_pContextTAN->GetAMFContext()->AllocBuffer(
                    crossfadeQueue->GetMemoryType(),
                    singleBufSize * m_iChannels,
                    &buffer //&mAMFCLXFadeMasterBuffers[bufIdx]
                    )
                );
            mAMFCLXFadeMasterBuffers[bufIdx] = amf::AMFBufferExPtr(buffer);
            AMF_RETURN_IF_FALSE(mAMFCLXFadeMasterBuffers[bufIdx].GetPtr() != nullptr, AMF_NOT_SUPPORTED);

            mFadeSubbufers[bufIdx].AllocateChannels(
                m_iChannels,
                crossfadeQueue->GetMemoryType()
                );

            //Then split the big buffer into small contiguous subbuffers
            for(amf_uint32 i = 0; i < m_iChannels; i++)
            {
                AMF_RETURN_IF_FAILED(
                    mAMFCLXFadeMasterBuffers[bufIdx]->CreateSubBuffer(
                        &mFadeSubbufers[bufIdx].GetAMFBuffersReadWrite()[i],
                        i * singleBufSize,
                        singleBufSize
                        )
                    );
            }

            mFadeSubbufers[bufIdx].MarkBuffersAllocated();
#endif
        }
    }

    // allocate crossfade buffers on host memory
    m_pXFadeSamples.AllocateChannels(
        m_iChannels,
        amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST
        );

    for (amf_uint32 i = 0; i < m_iChannels; i++)
    {
        m_pXFadeSamples.AllocateHostBuffers(i, 4 * m_iBufferSizeInSamples + 2);
    }

    return AMF_OK;
}

AMF_RESULT TANConvolutionImpl::deallocateBuffers()
{
	for (amf_uint32 n = 0; m_OutSamples && n < m_iChannels; n++)
    {
		if (m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM ||
			m_eConvolutionMethod == TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM) {
			_mm_free(m_OutSamples[n]);
			_mm_free(m_OutSamplesXFade[n]);
			_mm_free(m_NUTailAccumulator[n]);
			_mm_free(m_NUTailSaved[n]);
		}
		else
        {
			SAFE_ARR_DELETE(m_OutSamples[n]);
			if (m_OutSamplesXFade != NULL)
				SAFE_ARR_DELETE(m_OutSamplesXFade[n]);
		}
	}
	SAFE_ARR_DELETE(m_OutSamples);
	SAFE_ARR_DELETE(m_OutSamplesXFade);

    if(m_internalOutBufs.IsSet() && m_internalOutBufs.IsBuffersAllocated())
    {
        m_internalOutBufs.DeallocateBuffers();
    }
	m_internalOutBufs.Release();

    if(m_internalInBufs.IsSet() && m_internalInBufs.IsBuffersAllocated())
    {
        m_internalInBufs.DeallocateBuffers();
    }
    m_internalInBufs.Release();

    m_pXFadeSamples.DeallocateBuffers();
    m_pXFadeSamples.Release();

    if (m_doProcessOnGpu)
    {
        for (amf_uint32 bufIdx = 0; bufIdx < 2; bufIdx++)
        {
#ifndef TAN_NO_OPENCL
            if(m_pCLXFadeMasterBuf[bufIdx] == nullptr)
            {
                continue;
            }

            DBG_CLRELEASE_MEMORYOBJECT(m_pCLXFadeMasterBuf[bufIdx]);

#else
            if(!mAMFCLXFadeMasterBuffers[bufIdx])
            {
                continue;
            }

            mAMFCLXFadeMasterBuffers[bufIdx] = nullptr;

#endif

            for (amf_uint32 n = 0; n < m_iChannels; n++)
            {
                if(mFadeSubbufers[bufIdx].IsSet())
                {
                    mFadeSubbufers[bufIdx].DeallocateBuffers();
                }

                mFadeSubbufers[bufIdx].Release();
            }
        }
    }

    SAFE_ARR_DELETE(m_availableChannels);
    SAFE_ARR_DELETE(m_flushedChannels);
    SAFE_ARR_DELETE(m_tailLeftOver);
    SAFE_ARR_DELETE(m_silence);

    switch (m_eConvolutionMethod) {
    case TAN_CONVOLUTION_METHOD_TIME_DOMAIN:
        // deallocate state data for ovlAddProcess:
        for (int i = 0; i < N_FILTER_STATES; i++) {
            for (amf_uint32 n = 0; n < m_iChannels; n++){
                if (m_tdFilterState[i]) {
                    if (m_tdFilterState[i]->m_Filter[n]){
                        SAFE_ARR_DELETE((m_tdFilterState[i])->m_Filter[n]);
                    }
                    if ( m_tdFilterState[i]->m_SampleHistory[n]) {
                        SAFE_ARR_DELETE((m_tdFilterState[i])->m_SampleHistory[n]);
                    }

                    //if (m_pContextTAN->GetOpenCLContext()!=nullptr)
                    {
#ifndef TAN_NO_OPENCL
                        m_tdFilterState[i]->FreeCLData(n);
#else
                        m_tdFilterState[i]->FreeAMFData(n);
#endif
                    }
                }
            }

#ifndef TAN_NO_OPENCL
                        m_tdFilterState[i]->DeallocateCL();
#else
                        m_tdFilterState[i]->DeallocateAMF();
#endif
       }
       break;

    case TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD:

        m_ovlAddLocalInBuffs.clear();
        m_ovlAddLocalOutBuffs.clear();

        for(int i = 0; i < N_FILTER_STATES; i++)
        {
            m_FilterState[i].Release();
        }
        break;
	case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM:
	case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM:
	{
		int nParts = (1 << m_log2len) / (1 << m_log2bsz);
		nParts /= m_2ndBufSizeMultiple; //NU
		// deallocate state data for ovlUPProcess:
		for (amf_uint32 n = 0; n < m_iChannels; n++) {

			for (int i = 0; i < N_FILTER_STATES; i++) {
				if (m_nupFilterState[i] && ((_ovlUniformPartitionFilterState *)m_nupFilterState[i])->m_Filter[n]) {
					_mm_free(m_nupFilterState[i]->m_Filter[n]);

					if (i == 0) {
						SAFE_ARR_DELETE(((_ovlUniformPartitionFilterState *)m_nupFilterState[i])->m_Overlap[n]);
						_mm_free(m_nupFilterState[i]->m_DataPartitions[n]);
					}
					m_nupFilterState[i]->m_Overlap[n] = NULL;
					m_nupFilterState[i]->m_DataPartitions[n] = NULL;

				}
			}
		}

		m_ovlAddLocalInBuffs.clear();
		m_ovlAddLocalOutBuffs.clear();

		SAFE_ARR_DELETE(m_FilterTD);
		SAFE_ARR_DELETE(m_nupFilterState[0]->m_DataPartitions);
		SAFE_ARR_DELETE(m_nupFilterState[0]->m_SubPartitions);

		for (int i = 0; m_nupFilterState[i] && i < N_FILTER_STATES; i++) {
			SAFE_ARR_DELETE(((_ovlUniformPartitionFilterState *)m_nupFilterState[i])->m_Filter);
			SAFE_ARR_DELETE(((_ovlUniformPartitionFilterState *)m_nupFilterState[i])->m_Overlap);
			SAFE_ARR_DELETE(((_ovlUniformPartitionFilterState *)m_nupFilterState[i])->m_internalFilter);
			SAFE_ARR_DELETE(((_ovlUniformPartitionFilterState *)m_nupFilterState[i])->m_internalOverlap);
			SAFE_ARR_DELETE(m_nupFilterState[i]);
		}
	}
	break;

    case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:
    {
        if (m_graal_conv)
        {
            delete (graal::CGraalConv *)m_graal_conv;
            m_graal_conv = nullptr;

            SAFE_ARR_DELETE(m_s_versions[0]);
            SAFE_ARR_DELETE(m_s_versions[1]);
            SAFE_ARR_DELETE(m_s_channels);

            m_graal_conv = 0;
        }

    }
    break;

    default:
        break;
    }

    m_onconv_switch_delay_counter = 0;
    return AMF_OK;
}

#ifdef USE_TAIL_THREAD

void TANConvolutionImpl::TailThreadProc(AMFThread *pThread)
{
	// ToDo legacy - fix this
	//m_tailThreadHandle = GetCurrentThread();
	//int tId = GetThreadId(m_tailThreadHandle);
	//tId = GetCurrentThreadId();
	//SetThreadPriority(m_tailThreadHandle, THREAD_PRIORITY_TIME_CRITICAL);

	do {
		// Wait for the time to start processing.
        PrintDebug("2730 m_TailDoneEvent.SetEvent()");
		m_TailDoneEvent.SetEvent();

		m_runTailEvent.Lock();
		//Sleep(10); //hack
		{
			//AMFLock lock(&m_sectUpdate);
			//AMFLock lockAccum(&m_sectAccum);

			ovlUPProcessTail();

			//procp(5);
			//m_procReadyForNewResponsesEvent.SetEvent();
		}
		if (pThread->StopRequested()) {
			break;
		}
	} while (!pThread->StopRequested());
	//hack
	static bool finished = pThread->IsRunning();

}

#endif

void TANConvolutionImpl::UpdateThreadProc(AMFThread *pThread)
{
    //todo: legacy debug code?
    //m_updThreadHandle = GetCurrentThread();
    //int tId = GetThreadId(m_updThreadHandle);
    //tId = GetCurrentThreadId();

    do
    {
        PrintDebug("2766 m_procReadyForNewResponsesEvent.Lock()... .SetEvent()");
        // Wait for the time to start processing.
        m_procReadyForNewResponsesEvent.Lock();
        PrintDebug("2769 m_procReadyForNewResponsesEvent.Locked() .SetEvent()");

        //hack
        if (pThread->StopRequested()) {
            break;
        }

        // Do processing if there is something ready.
        if (m_accumulatedArgs.updatesCnt == 0 ) {
            continue;
        }

        // Restart accumulation.
        {
            AMFLock lock(&m_sectAccum);

            m_updateArgs.Pack(m_accumulatedArgs, m_iChannels);
            m_copyArgs.Negate(m_accumulatedArgs, m_iChannels, m_idxFilter, m_idxUpdateFilter);
            m_accumulatedArgs.Clear(m_iChannels);
        }

        // Start processing.
        AMF_RESULT ret = AMF_OK;
        switch (m_eConvolutionMethod) {
            case TAN_CONVOLUTION_METHOD_TIME_DOMAIN:
            {
                float **filter = ((ovlAddFilterState *)m_tdFilterState[m_idxUpdateFilter])->m_Filter;


                // Copy data to the new slot, as this channel can be still processed (user doesn't
                // pass Stop flag to Process() method) and we may start doing cross-fading.
                float **const ppOldFilter =
                    ((ovlAddFilterState *)m_tdFilterState[m_idxFilter])->m_Filter;
                for (amf_uint32 argId = 0; argId < m_copyArgs.updatesCnt; argId++) {
                    const amf_uint32 channelId = m_copyArgs.channels[argId];
                    memcpy(filter[channelId], ppOldFilter[channelId], 2 * m_length * sizeof(float));
                }
            }
            break;
            case TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD:
            {
                AMFLock syncLock(&m_sectUpdate);

                for(int filter = 0; filter < N_FILTER_STATES; ++filter)
                {
                    for(int channel = 0; channel < m_iChannels; ++channel)
                    {
                        m_FilterState[filter].DebugPrint(
                            ES + __FUNCTION__ + std::to_string(__LINE__) + " ["
                           + std::to_string(filter) + "] ["
                           + std::to_string(channel) + "]",
                            channel
                            );
                    }
                }

                float **filter = m_FilterState[m_idxUpdateFilter].m_Filter;
                float **overlap = m_FilterState[m_idxUpdateFilter].m_Overlap;

                RETURN_IF_FAILED(ret = m_pUpdateTanFft->Transform(
                    TAN_FFT_TRANSFORM_DIRECTION_FORWARD,
                    m_log2len,
                    m_updateArgs.updatesCnt,
                    m_updateArgs.responses,
                    m_updateArgs.responses
                    )
                    );

                for(int filter = 0; filter < N_FILTER_STATES; ++filter)
                {
                    for(int channel = 0; channel < m_iChannels; ++channel)
                    {
                        m_FilterState[filter].DebugPrint(
                            ES + __FUNCTION__ + std::to_string(__LINE__) + " ["
                            + std::to_string(filter) + "] ["
                            + std::to_string(channel) + "]",
                            channel
                            );
                    }
                }

                //// Copy data to the new slot, as this channel can be still processed (user doesn't
                //// pass Stop flag to Process() method) and we may start doing cross-fading.
                float **const ppOldFilter = m_FilterState[m_idxFilter].m_Filter;
                float **const ppOldOverlap = m_FilterState[m_idxFilter].m_Overlap;

                for(amf_uint32 argId = 0; argId < m_copyArgs.updatesCnt; argId++)
                {
                    const amf_uint32 channelId = m_copyArgs.channels[argId];

                    memcpy(filter[channelId], ppOldFilter[channelId], 2 * m_length * sizeof(float));
                    memcpy(overlap[channelId], ppOldOverlap[channelId], m_length * sizeof(float));
                }

                for(int filter = 0; filter < N_FILTER_STATES; ++filter)
                {
                    for(int channel = 0; channel < m_iChannels; ++channel)
                    {
                        m_FilterState[filter].DebugPrint(
                            ES + __FUNCTION__ + std::to_string(__LINE__) + " ["
                           + std::to_string(filter) + "] ["
                           + std::to_string(channel) + "]",
                            channel
                            );
                    }
                }
            }
            break;

			case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM:
			case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM:
			{
				AMFLock syncLock(&m_sectUpdate);

				// copy in TD IR
				for (int n = 0; n < m_iChannels; n++) {
					memcpy(m_nupFilterState[m_idxUpdateFilter]->m_Filter[n], m_FilterTD[n], m_length * sizeof(float));
				}

				float **filter = ((_ovlUniformPartitionFilterState *)m_nupFilterState[m_idxUpdateFilter])->m_Filter;
				float **overlap = ((_ovlUniformPartitionFilterState *)m_nupFilterState[m_idxUpdateFilter])->m_Overlap;

				float **filterParts = new float*[m_iChannels]; //leak

				int nParts = (1 << m_log2len) / (1 << m_log2bsz);
				nParts /= m_2ndBufSizeMultiple; //NU

				int pad = 0;
				TAN_FFT_TRANSFORM_DIRECTION fwdDir = TAN_FFT_R2C_TRANSFORM_DIRECTION_FORWARD;
				switch (m_TransformType) {
				case TRANSFORMTYPE_FFTREAL:
					pad = PARTITION_PAD_FFTREAL;
					fwdDir = TAN_FFT_R2C_TRANSFORM_DIRECTION_FORWARD;
					break;
				case TRANSFORMTYPE_FFTREAL_PLANAR:
					pad = PARTITION_PAD_FFTREAL_PLANAR;
					fwdDir = TAN_FFT_R2C_PLANAR_TRANSFORM_DIRECTION_FORWARD;
					break;
				}


				int iBuffSizeNU = m_iBufferSizeInSamples*m_2ndBufSizeMultiple;
				// expand filter
				for (int i = nParts - 1; i >= 0; i--) {
					float *pIn;
					float *pOut;
					for (int chan = 0; chan < m_iChannels; chan++) {
						pIn = filter[chan] + i *  iBuffSizeNU;
						pOut = filter[chan] + i * (2 * iBuffSizeNU + pad);
						memcpy(pOut, pIn, sizeof(float) * iBuffSizeNU);
						memset(pOut + iBuffSizeNU, 0, sizeof(float) * (pad + iBuffSizeNU));

					}
				}

				for (int i = 0; i < nParts; i++) {
					for (int chan = 0; chan < m_iChannels; chan++) {
						filterParts[chan] = filter[chan] + i * (2 * iBuffSizeNU + pad);
					}

					int log2FFTLen = m_log2bsz;
					for (int n = m_2ndBufSizeMultiple; n > 0; n = n / 2) {
						++log2FFTLen;
					}

					RETURN_IF_FAILED(ret = m_pUpdateTanFft->Transform(
						fwdDir,
						log2FFTLen, m_iChannels,
						filterParts, filterParts));

				}

				delete [] filterParts; //


									//// Copy data to the new slot, as this channel can be still processed (user doesn't
									//// pass Stop flag to Process() method) and we may start doing cross-fading.
				float **const ppOldFilter =
					((_ovlUniformPartitionFilterState *)m_nupFilterState[m_idxFilter])->m_Filter;
				float **const ppOldOverlap =
					((_ovlUniformPartitionFilterState *)m_nupFilterState[m_idxFilter])->m_Overlap;

				for (amf_uint32 argId = 0; argId < m_copyArgs.updatesCnt; argId++) {
					const amf_uint32 channelId = m_copyArgs.channels[argId];
					memcpy(filter[channelId], ppOldFilter[channelId], m_length * sizeof(float));
					memcpy(overlap[channelId], ppOldOverlap[channelId], m_length * sizeof(float));
				}


			}
			break;

            case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED:
            case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED:
            case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:
            {
                AMFLock syncLock(&m_sectUpdate);

                graal::CGraalConv*graalConv = (graal::CGraalConv*)m_graal_conv;
                RETURN_IF_FALSE(graalConv->updateConv(m_updateArgs.updatesCnt,
                                                      m_updateArgs.versions,
                                                      m_updateArgs.channels,
                                                      m_updateArgs.lens, true) == GRAAL_SUCCESS,
													//m_updateArgs.lens, false) == GRAAL_SUCCESS,
					ret, AMF_UNEXPECTED);

                // Copy data to the new slot, as this channel can be still processed (user doesn't
                // pass Stop flag to Process() method) and we may start doing cross-fading.
                ret = graalConv->copyResponses(
                    m_copyArgs.updatesCnt,
                    (uint*)m_copyArgs.prevVersions,
                    (uint*)m_copyArgs.versions,
                    (uint*)m_copyArgs.channels
                    );
                RETURN_IF_FAILED(ret);
            }
            break;

			case TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED:
			{
                printf("Not implemented\n");

                THROW_NOT_IMPLEMENTED;

				/*
                AMFLock syncLock(&m_sectUpdate);
				RETURN_IF_FALSE(m_nonUniformGraal.updateConv(m_updateArgs.updatesCnt, m_updateArgs.versions[0], m_updateArgs.channels) == GRAAL_SUCCESS, ret, AMF_UNEXPECTED);
				RETURN_IF_FALSE(m_nonUniformGraal.copyResponses(m_copyArgs.updatesCnt, m_copyArgs.prevVersions[0], m_copyArgs.versions[0], m_copyArgs.channels) == GRAAL_SUCCESS, ret, AMF_UNEXPECTED);
                */
			}
			break;

            default:
                RETURN_IF_FALSE(false, ret, AMF_NOT_IMPLEMENTED);
        }

        PrintDebug("3005 m_updateFinishedProcessing.SetEvent()");
        m_updateFinishedProcessing.SetEvent();

        continue;

ErrorHandling:

        PrintDebug("3006 m_updateFinishedProcessing.SetEvent()");
        m_updateFinishedProcessing.SetEvent();

    } while (!pThread->StopRequested());

    //hack
    static bool finished = pThread->IsRunning();
}

AMF_RESULT TANConvolutionImpl::VectorComplexMul(float *vA, float *vB, float *out, int count)
{
    //PrintFloatArray(">VectorComplexMul vA", vA, count);
    //PrintFloatArray(">VectorComplexMul vB", vB, count);
    //PrintFloatArray(">VectorComplexMul vO", out, count);

    for (int i = 0; i < count; i++){
        int j;
        float ar, ai, br, bi;
        j = (i << 1);
        ar = vA[j];
        ai = vA[j + 1];
        br = vB[j];
        bi = vB[j + 1];
        out[j] = ar*br - ai*bi;
        out[j + 1] = ar*bi + ai*br;
    }

    //PrintFloatArray("<VectorComplexMul vA", vA, count);
    //PrintFloatArray("<VectorComplexMul vB", vB, count);
    //PrintFloatArray("<VectorComplexMul vO", out, count);

    return AMF_OK;
}

AMF_RESULT TANConvolutionImpl::ovlAddProcess(
    amf_size &              numberOfSamplesProcessed,
    ovlAddFilterState *     state,
    const TANSampleBuffer & inputData,
    TANSampleBuffer &       outputData,
    amf_size                nSamples,
    amf_uint32              n_channels,
    bool                    advanceOverlap
)
{
    numberOfSamplesProcessed = 0;

    if(inputData.GetType() != AMF_MEMORY_HOST)
    {
        return AMF_OK;
    }

    float* const * output = nullptr;
    std::vector<float *> outputExpanded(m_iChannels, nullptr);

    if(outputData.IsHost())
    {
        output = outputData.GetHostBuffers();
    }
    else if(outputData.IsComputeBuffer())
    {
        if (!m_doProcessOnGpu)
        {
            return AMF_OK;
        }

        //output = m_ovlAddLocalOutBuffs;
        for(int channel(0); channel < m_iChannels; ++channel)
        {
            outputExpanded[channel] = m_ovlAddLocalOutBuffs[channel].data();
        }
        output = outputExpanded.data();
    }

    float **filter = state->m_internalFilter;
    float **overlap = state->m_internalOverlap;

    // we process in bufSize blocks
    if (nSamples < m_iBufferSizeInSamples)
    {
        return AMF_OK;
    }

    // use fixed overlap size:
    nSamples = m_iBufferSizeInSamples;

    // Convert to complex numbers.
    for (amf_uint32 iChan = 0; iChan < n_channels; iChan++)
    {
        // get next block of data, expand into real part of complex values:
        for (int k = 0; k < nSamples; k++)
        {
            m_OutSamples[iChan][2 * k] = inputData.GetHostBuffers()[iChan][k];
            m_OutSamples[iChan][2 * k + 1] = 0.0;
        }

        // zero pad:
        std::memset(
            &m_OutSamples[iChan][2 * nSamples],
            0,
            2 * (m_length - nSamples) * sizeof(m_OutSamples[0][0])
            );
    }

    AMF_RETURN_IF_FAILED(
        m_pTanFft->Transform(
            TAN_FFT_TRANSFORM_DIRECTION_FORWARD,
            m_log2len,
            m_iChannels,
            m_OutSamples,
            m_OutSamples
            )
        );

    for (amf_uint32 iChan = 0; iChan < n_channels; iChan++)
    {
        VectorComplexMul(m_OutSamples[iChan], filter[iChan], m_OutSamples[iChan], m_length);
    }

    AMF_RETURN_IF_FAILED(
        m_pTanFft->Transform(
            TAN_FFT_TRANSFORM_DIRECTION_BACKWARD,
            m_log2len,
            m_iChannels,
            m_OutSamples,
            m_OutSamples
            )
        );

    for(amf_uint32 iChannel = 0; iChannel < n_channels; iChannel++)
    {
        if(advanceOverlap)
        {
            for (amf_size id = 0; id < m_length; id++)
            {
                overlap[iChannel][id] = m_OutSamples[iChannel][id * 2]
                    +
                    (id + nSamples >= m_length ? 0 : overlap[iChannel][id + nSamples]);
            }

            memcpy(output[iChannel], overlap[iChannel], nSamples * sizeof(float));
        }
        else
        {
            for (int i = 0; i < nSamples; i++)
            {
                output[iChannel][i] = m_OutSamples[iChannel][i * 2] + overlap[iChannel][i + nSamples];
            }
        }
    }

    if(outputData.IsComputeBuffer())
    {
#ifndef TAN_NO_OPENCL
        // move samples to the OCL output buffers
        cl_command_queue convQ = m_pContextTAN->GetOpenCLConvQueue();

        for(amf_uint32 iChan = 0; iChan < n_channels; iChan++)
        {
            auto result(
                clEnqueueWriteBuffer(
                    convQ,
                    outputData.GetCLBuffers()[iChan],
                    CL_TRUE,
                    0,
                    nSamples * sizeof(float),
                    m_ovlAddLocalOutBuffs[iChan].data(),
                    0,
                    NULL,
                    NULL
                    )
                );

            AMF_RETURN_IF_CL_FAILED(result, L"Failed to write to OvlAdd output buffers");
        }
#else
        // move samples to the OCL output buffers
        auto convQ = m_pContextTAN->GetAMFConvQueue();

        for(amf_uint32 iChan = 0; iChan < n_channels; iChan++)
        {
            AMF_RETURN_IF_FAILED(
                convQ->CopyBufferFromHost(
                    m_ovlAddLocalOutBuffs[iChan].data(),
                    nSamples * sizeof(float),
                    outputData.GetAMFBuffersReadWrite()[iChan],
                    0,
                    true
                    )
                );
        }
#endif
    }

    numberOfSamplesProcessed = nSamples;

    //for (amf_uint32 iChan = 0; iChan < n_channels; iChan++){
    //    for (amf_size id = 0; id < nSamples; id++)
    //    {
    //        outputData[iChan][id] = m_OutSamples[iChan][id * 2] + overlap[iChan][id];
    //    }

    //    ZeroMemory(overlap[iChan],  nSamples * sizeof(float));

    //    for (amf_size id = 0; id < m_length - nSamples; id++)
    //    {
    //        overlap[iChan][id] += m_OutSamples[iChan][(id + nSamples) * 2];
    //    }

    //}

    return AMF_OK;
}


amf_size TANConvolutionImpl::ovlNUPProcess(
	ovlNonUniformPartitionFilterState *state,
	const TANSampleBuffer & inputData,
	TANSampleBuffer &       outputData,
	amf_size nSamples,
	amf_uint32 n_channels,
	bool advanceOverlap,
	bool useXFadeAccumulator
)
{
	amf_size ret = 0;
	if (inputData.GetType() != AMF_MEMORY_HOST)
	{
		return 0;
	}

	//hack
	if (m_doProcessOnGpu) {
		//return ovlNUPProcessGPU(state, inputData, outputData, nSamples, n_channels, advanceOverlap, useXFadeAccumulator);
		return -1;
	}
	else {
		ret = ovlNUPProcessCPU(state, inputData, outputData, nSamples, n_channels, advanceOverlap, useXFadeAccumulator);
	}
	if (!m_bUseProcessFinalize) {
		// app isn't going to call finalize, so do it here:
		ProcessFinalize();
	}
	return ret;
}


// non uniform partitioned convolution, on CPU
// assumes TRANSFORMTYPE_FFTREAL_PLANAR

amf_size TANConvolutionImpl::ovlNUPProcessCPU(
	ovlNonUniformPartitionFilterState * state,
	const TANSampleBuffer &             inputData,
	TANSampleBuffer &                   outputData,
	amf_size                            nSamples,
	amf_uint32                          n_channels,
	bool                                advanceOverlap,
	bool                                useXFadeAccumulator
)
{
	float* const* output = outputData.GetHostBuffers();

    if(outputData.IsComputeBuffer())
	{
		if (!m_doProcessOnGpu)
		{
			return 0;
		}

		//output = m_ovlAddLocalOutBuffs;
        std::vector<float *> outputExpanded(m_iChannels, nullptr);

        for(int channel(0); channel < m_iChannels; ++channel)
        {
            outputExpanded[channel] = m_ovlAddLocalOutBuffs[channel].data();
        }

        output = outputExpanded.data();
	}

    //PrintReducedFloatArray("ovlNU in0", inputData.GetHostBuffers()[0], nSamples * sizeof(float));
    //PrintReducedFloatArray("ovlNU in1", inputData.GetHostBuffers()[1], nSamples * sizeof(float));

	float **filter = state->m_internalFilter;
	float **dataParts = new float *[n_channels]; //leak
	float **filterParts = new float *[n_channels]; //leak
	float **overlap = state->m_internalOverlap;
	float **subParts = state->m_SubPartitions; //leak
	float **workBuffer = state->m_workBuffer;

    //PrintReducedFloatArray("ovlNU filter", filter[0], mNUPSize * sizeof(float));
    //PrintReducedFloatArray("ovlNU overlap", overlap[0], mNUPSize2 * sizeof(float));
    //PrintReducedFloatArray("ovlNU subParts", subParts[0], mNUPSize2 * sizeof(float));
    //PrintReducedFloatArray("ovlNU workBuffer", workBuffer[0], mNUPSize2 * sizeof(float));

	m_nupTailState = state;

	// we process in bufSize blocks
	if (nSamples < m_iBufferSizeInSamples)
		return 0;
	// use fixed overlap size:
	nSamples = m_iBufferSizeInSamples;

	int nParts = (1 << m_log2len) / (1 << m_log2bsz);
	nParts /= m_2ndBufSizeMultiple; //NU


	int iBuffSizeNU = m_iBufferSizeInSamples*m_2ndBufSizeMultiple;

	int curPart = (m_currentDataPartition - 1 + m_2ndBufSizeMultiple*nParts) % (m_2ndBufSizeMultiple*nParts);
	if (advanceOverlap) {
		m_currentDataPartition = curPart;
	}

	m_2ndBufCurrentSubBuf = (m_2ndBufSizeMultiple*nParts - curPart - 1) % m_2ndBufSizeMultiple;
	curPart = curPart / m_2ndBufSizeMultiple;

	int log2FFTLen = m_log2bsz;
	for (int n = m_2ndBufSizeMultiple; n > 0; n = n / 2) {
		++log2FFTLen;
	}

	float **outSamples = m_OutSamples;
	if (useXFadeAccumulator) {
		outSamples = m_OutSamplesXFade;
	}


	//if (m_bUseProcessFinalize) {

	int pad = 0;
	TAN_FFT_TRANSFORM_DIRECTION fwdDir = TAN_FFT_R2C_TRANSFORM_DIRECTION_FORWARD;
	TAN_FFT_TRANSFORM_DIRECTION bwdDir = TAN_FFT_C2R_TRANSFORM_DIRECTION_BACKWARD;
	switch (m_TransformType) {
	case TRANSFORMTYPE_FFTREAL:
		pad = PARTITION_PAD_FFTREAL;
		fwdDir = TAN_FFT_R2C_TRANSFORM_DIRECTION_FORWARD;
		bwdDir = TAN_FFT_C2R_TRANSFORM_DIRECTION_BACKWARD;
		break;
	case TRANSFORMTYPE_FFTREAL_PLANAR:
		pad = PARTITION_PAD_FFTREAL_PLANAR;
		fwdDir = TAN_FFT_R2C_PLANAR_TRANSFORM_DIRECTION_FORWARD;
		bwdDir = TAN_FFT_C2R_PLANAR_TRANSFORM_DIRECTION_BACKWARD;
		break;
	}


	// Align Data and Filter partitions:
	for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
		filterParts[iChan] = filter[iChan];
		dataParts[iChan] = state->m_internalDataPartitions[iChan] + curPart * (2 * iBuffSizeNU + pad);
		memset(dataParts[iChan], 0, sizeof(float) * (2 * m_iBufferSizeInSamples + pad));
		//memcpy(dataParts[iChan], inputData.GetHostBuffers()[iChan], nSamples * sizeof(float));

		if (m_2ndBufCurrentSubBuf == 0)
			memset(subParts[iChan], 0, sizeof(float) * (2 * iBuffSizeNU + pad));

		//// need another buffer to accumulate m_2ndBufSizeMultiple sub bufs....
		memcpy(subParts[iChan] + m_2ndBufCurrentSubBuf*m_iBufferSizeInSamples, inputData.GetHostBuffers()[iChan], nSamples * sizeof(float));

		memcpy(dataParts[iChan], subParts[iChan], sizeof(float) * (2 * iBuffSizeNU + pad));
	}

    //PrintReducedFloatArray("ovlNU dataParts0", dataParts[0], sizeof(float) * (2 * m_iBufferSizeInSamples + pad));
    //PrintReducedFloatArray("ovlNU dataParts1", dataParts[1], sizeof(float) * (2 * m_iBufferSizeInSamples + pad));
    //PrintReducedFloatArray("ovlNU filterParts0", filterParts[0], mNUPSize * sizeof(float));
    //PrintReducedFloatArray("ovlNU filterParts1", filterParts[1], mNUPSize * sizeof(float));
    //PrintReducedFloatArray("ovlNU outSamples0", outSamples[0], nSamples * sizeof(float));
    //PrintReducedFloatArray("ovlNU outSamples1", outSamples[1], nSamples * sizeof(float));

	// transform real data to complex:
	AMF_RETURN_IF_FAILED(m_pTanFft->Transform(fwdDir, log2FFTLen, n_channels,
		dataParts, dataParts));

	switch (m_TransformType) {
	case TRANSFORMTYPE_FFTREAL_PLANAR:
		m_pMath->PlanarComplexMultiplyAccumulate(dataParts, filterParts, outSamples, n_channels, iBuffSizeNU + 8, iBuffSizeNU + 8);
		break;
	case TRANSFORMTYPE_FFTREAL:
#ifdef USE_IPP
		m_pMath->IPPComplexMultiplyAccumulate(dataParts, filterParts, outSamples, workBuffer, n_channels, iBuffSizeNU);
#else
		m_pMath->ComplexMultiplyAccumulate(dataParts, filterParts, outSamples, n_channels, iBuffSizeNU + pad);
#endif
		break;
	}

	AMF_RETURN_IF_FAILED(m_pTanFft->Transform(bwdDir, log2FFTLen, n_channels,
		outSamples, outSamples));

	for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
		for (int i = 0; i < nSamples; i++) {
			output[iChan][i] = outSamples[iChan][i + m_2ndBufCurrentSubBuf*nSamples] + overlap[iChan][i + m_2ndBufCurrentSubBuf*nSamples];
		}

		if (advanceOverlap && m_2ndBufCurrentSubBuf == (m_2ndBufSizeMultiple - 1)) {
			float *fpD, *fpA, *fpB;
			fpD = &overlap[iChan][0];
			fpA = &outSamples[iChan][m_2ndBufSizeMultiple*nSamples];
			for (int i = 0; i < 2 * m_2ndBufSizeMultiple* nSamples; i++) {
				*fpD++ = *fpA++;
			}
		}
	}




	//for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
	//	if (advanceOverlap) {
	//		float *fpD, *fpA, *fpB;
	//		fpD = &overlap[iChan][0];
	//		fpA = &outSamples[iChan][0];
	//		fpB = &overlap[iChan][nSamples];
	//		for (int i = 0; i < 2 * nSamples; i++) {
	//			*fpD++ = *fpA++ + *fpB++;
	//		}
	//		memcpy(output[iChan], overlap[iChan], nSamples * sizeof(float));
	//	}
	//	else {
	//		for (int i = 0; i < nSamples; i++) {
	//			output[iChan][i] = outSamples[iChan][i] + overlap[iChan][i + nSamples];
	//		}
	//	}
	//}
	//}

    //PrintReducedFloatArray("ovlNU out0", outputData.GetHostBuffers()[0], nSamples * sizeof(float));
    //PrintReducedFloatArray("ovlNU out1", outputData.GetHostBuffers()[1], nSamples * sizeof(float));

	return nSamples;
}

int TANConvolutionImpl::ovlNUPProcessTail(_ovlNonUniformPartitionFilterState *state, bool useXFadeAccumulator) {
	float **outSamples = m_OutSamples;
	if (useXFadeAccumulator) {
		outSamples = m_OutSamplesXFade;
	}

	if (m_RunningChannels == 0)
		return 0;

	int n_channels = m_RunningChannels; // m_iChannels;
	float **dataParts = new float *[n_channels];
	float **filterParts = new float *[n_channels];
	//_ovlUniformPartitionFilterState *state = m_upTailState; // m_upFilterState[m_idxFilter];
	if (state == NULL) {
		state = m_nupTailState;
	}

	float **filter = state->m_internalFilter;
	//float **filter = ((_ovlUniformPartitionFilterState *)state)->m_Filter;

	int nParts = (1 << m_log2len) / (1 << m_log2bsz);
	nParts /= m_2ndBufSizeMultiple; //NU

	int iBuffSizeNU = m_iBufferSizeInSamples*m_2ndBufSizeMultiple;

	//for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
	//	memset(outSamples[iChan], 0, sizeof(float)*(2 * iBuffSizeNU + PARTITION_PAD_FFTREAL_PLANAR));
	//}
	//int curPart = ((m_currentDataPartition - 1 + m_2ndBufSizeMultiple) / m_2ndBufSizeMultiple + nParts) % (nParts);
	//for (int i = 1; i < nParts;) {
	//		for (int chan = 0; chan < n_channels; chan++) {
	//		filterParts[chan] = filter[chan] + i * (2 * iBuffSizeNU + PARTITION_PAD_FFTREAL_PLANAR);
	//		dataParts[chan] = state->m_internalDataPartitions[chan] + curPart * (2 * iBuffSizeNU + PARTITION_PAD_FFTREAL_PLANAR);
	//	}

	//	m_pMath->PlanarComplexMultiplyAccumulate(dataParts, filterParts, outSamples, n_channels, iBuffSizeNU + 8, iBuffSizeNU + 8);
	//	curPart = (curPart + 1 + nParts) % nParts;
	//	++i;
	//}

	int pad = 0;
	switch (m_TransformType) {
	case TRANSFORMTYPE_FFTREAL:
		pad = PARTITION_PAD_FFTREAL;
		break;
	case TRANSFORMTYPE_FFTREAL_PLANAR:
		pad = PARTITION_PAD_FFTREAL_PLANAR;
		break;
	}


	if (m_2ndBufCurrentSubBuf == 0) {
		for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
			memset(m_NUTailAccumulator[iChan], 0, sizeof(float)*(2 * iBuffSizeNU + pad));
		}
	}

	int curPart = m_currentDataPartition / m_2ndBufSizeMultiple;

	for (int i = 1 + (nParts / m_2ndBufSizeMultiple)*(m_2ndBufCurrentSubBuf); i < (nParts / m_2ndBufSizeMultiple)*(1 + m_2ndBufCurrentSubBuf); i++) {
		for (int chan = 0; chan < n_channels; chan++) {
			filterParts[chan] = filter[chan] + i * (2 * iBuffSizeNU + pad);
			dataParts[chan] = state->m_internalDataPartitions[chan] + curPart * (2 * iBuffSizeNU + pad);
		}

		//#ifdef USE_IPP
		//		m_pMath->IPPComplexMultiplyAccumulate(dataParts, filterParts, m_NUTailAccumulator, state->m_workBuffer, n_channels, iBuffSizeNU + 8);
		//#else
		//		m_pMath->PlanarComplexMultiplyAccumulate(dataParts, filterParts, m_NUTailAccumulator, n_channels, iBuffSizeNU + 8, iBuffSizeNU + 8);
		//#endif
		switch (m_TransformType) {
		case TRANSFORMTYPE_FFTREAL_PLANAR:
			m_pMath->PlanarComplexMultiplyAccumulate(dataParts, filterParts, m_NUTailAccumulator, n_channels, iBuffSizeNU + 8, iBuffSizeNU + 8);
			break;
		case TRANSFORMTYPE_FFTREAL:
#ifdef USE_IPP
			m_pMath->IPPComplexMultiplyAccumulate(dataParts, filterParts, m_NUTailAccumulator, state->m_workBuffer, n_channels, iBuffSizeNU + 8);
#else
			m_pMath->ComplexMultiplyAccumulate(dataParts, filterParts, m_NUTailAccumulator, n_channels, iBuffSizeNU + pad);
#endif
			break;
		}


		curPart = (curPart + 1 + nParts) % nParts;
	}

	if (m_2ndBufCurrentSubBuf == (m_2ndBufSizeMultiple - 1)) {
		for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
			//memset(outSamples[iChan], 0, sizeof(float)*(2 * iBuffSizeNU + PARTITION_PAD_FFTREAL_PLANAR));
			memcpy(m_NUTailSaved[iChan], m_NUTailAccumulator[iChan], sizeof(float)*(2 * iBuffSizeNU + pad));
		}
	}

	for (amf_uint32 iChan = 0; iChan < n_channels; iChan++) {
		//memset(outSamples[iChan], 0, sizeof(float)*(2 * iBuffSizeNU + PARTITION_PAD_FFTREAL_PLANAR));
		memcpy(outSamples[iChan], m_NUTailSaved[iChan], sizeof(float)*(2 * iBuffSizeNU + pad));
	}

	delete [] dataParts;
	delete [] filterParts;

	//m_2ndBufCurrentSubBuf = (m_2ndBufCurrentSubBuf + 1) % m_2ndBufSizeMultiple;
	return 0;
}



amf_size TANConvolutionImpl::ovlTDProcess(
    tdFilterState *state,
    float **inputData,
    float **outputData,
    amf_size nSamples,
    amf_uint32 n_channels
    )
{
    float **filter = state->m_Filter;// ??? move inside ovlTimeDomain
    float **sampleHistory = state->m_SampleHistory; // ??? move inside ovlTimeDomain
    int *sampHistPos = state->m_sampHistPos;// ??? move inside ovlTimeDomain
    if (nSamples > m_iBufferSizeInSamples)
        nSamples = m_iBufferSizeInSamples;

    for (amf_uint32 iChan = 0; iChan < n_channels; iChan++)
    {
#ifndef TAN_NO_OPENCL
        ovlTimeDomain(state, iChan, filter[iChan], 0, m_length, inputData[iChan], outputData[iChan], sampHistPos[iChan], nSamples, m_length);
#else
        THROW_NOT_IMPLEMENTED;

        return 0;
#endif
        sampHistPos[iChan] += nSamples;
    }

    return nSamples;
}

// host memory version
AMF_RESULT TANConvolutionImpl::ovlTimeDomain(
    tdFilterState *state,
    int iChan,
    float *resp,
    amf_uint32 firstNonZero,
    amf_uint32 lastNonZero,
    float *in,
    float *out,
    amf_uint32 bufPos,
    amf_size datalength,
    amf_size convlength)
{
    THROW_NOT_IMPLEMENTED;

    return AMF_NOT_IMPLEMENTED;
    /*
#ifndef TAN_NO_OPENCL

    cl_context context = m_pContextTAN->GetOpenCLContext();

    float *histBuf = state->m_SampleHistory[iChan]; // ??? move inside ovlTimeDomain

    if (context == nullptr)
    {
        ovlTimeDomainCPU(resp, firstNonZero, lastNonZero, in, out, histBuf, bufPos, datalength, convlength);
    }
    else
    {
        cl_command_queue cmdQueue = m_pContextTAN->GetOpenCLGeneralQueue();
        // build Time Domain kernel if necessary:
        if (m_TimeDomainKernel == nullptr){
            char dllPath[MAX_PATH + 1];

        //TODO: use getModuleFileName

#ifdef _WIN32
            GetModuleFileNameA(NULL, dllPath, MAX_PATH);
#else
            //todo: verify
            ssize_t contentSize = readlink("/proc/self/exe", dllPath, MAX_PATH);
            if(contentSize != -1)
            {
                dllPath[contentSize] = '\0';
            }

            //todo: macosx version!
#endif
            printf("\n\nTODO: Investigate!!!\n\n");

            char *pslash = strrchr(dllPath, '\\');
            if (pslash){
                *pslash = '\0';
            }
			bool OCLERR =
				GetOclKernel(m_TimeDomainKernel, m_pProcContextAMF, m_pContextTAN->GetOpenCLGeneralQueue(), "TimeDomainConvolution", TimeDomainConvolution_Str, TimeDomainConvolutionCount, "TimeDomainConvolution", "");
			if (!OCLERR){ printf("Failed to create OCL Kernel"); return AMF_FAIL;}
        }

        //clCreateBuffer(context

        bufPos = bufPos % convlength;
        int len1, len2;
        len2 = 0;
        len1 = datalength;
        if (bufPos + len1 > convlength){
            len1 = convlength - bufPos;
            len2 = datalength - len1;
        }
        // allocate internal cl_mem buffers...

        //clEnqueueWriteBuffer(cmdQueue, histBuf, 1, bufPos*sizeof(float), len1*sizeof(float),
        bufPos = bufPos % convlength;

        //// circular buffer....
        for (int i = 0; i < datalength; i++){
            histBuf[(bufPos + i) % convlength] = in[i];
        }
        const int pat = 0;
		// To Do write only changed data. Improve efficiency: Use map? Circular list of sub-buffers?
		//clEnqueueWriteBuffer(cmdQueue, state->m_clFilter[iChan], 1, 0, convlength*sizeof(float), resp, 0, nullptr, nullptr);
		clEnqueueWriteBuffer(cmdQueue, state->m_clFilter[iChan], 1, firstNonZero*sizeof(float), (lastNonZero - firstNonZero)*sizeof(float), resp + firstNonZero, 0, nullptr, nullptr);
		//clEnqueueWriteBuffer(cmdQueue, state->m_clSampleHistory[iChan], 1, 0, convlength*sizeof(float), histBuf, 0, nullptr, nullptr);
		clEnqueueWriteBuffer(cmdQueue, state->m_clSampleHistory[iChan], 1, bufPos*sizeof(float), len1*sizeof(float), histBuf + bufPos, 0, nullptr, nullptr);
		clEnqueueWriteBuffer(cmdQueue, state->m_clSampleHistory[iChan], 1, 0, len2*sizeof(float), histBuf, 0, nullptr, nullptr);

		FixedEnqueueFillBuffer(..., cmdQueue, state->m_clTemp[iChan], (const void *)&pat, sizeof(pat), 0, datalength*sizeof(float));

		ovlTimeDomainGPU(state->m_clFilter[iChan], firstNonZero, lastNonZero, state->m_clTemp[iChan], state->m_clSampleHistory[iChan], bufPos, datalength, convlength);

		cl_int status = 0;
		clEnqueueReadBuffer(cmdQueue, state->m_clTemp[iChan], 1, 0, datalength*sizeof(float), out, 0, nullptr, nullptr);
		FixedEnqueueFillBuffer(..., cmdQueue, state->m_clFilter[iChan], (const void *)&pat, sizeof(pat), firstNonZero*sizeof(float), (lastNonZero - firstNonZero)*sizeof(float));
    }

    return AMF_OK;
#else
    THROW_NOT_IMPLEMENTED;

    return AMF_NOT_IMPLEMENTED;
#endif
    */
}

// CPU implementation
AMF_RESULT TANConvolutionImpl::ovlTimeDomainCPU(
    float *resp,
    amf_uint32 firstNonZero,
    amf_uint32 lastNonZero,
    float *in,
    float *out,
    float *histBuf,
    amf_uint32 bufPos,
    amf_size datalength,
    amf_size convlength)
{
    bufPos = bufPos % convlength;

    // circular buffer....
    for (int i = 0; i < datalength; i++){
        histBuf[(bufPos + i) % convlength] = in[i];
    }

    for (int j = 0; j < datalength; j++){
        out[j] = 0.0;
        for (int k = firstNonZero; k < lastNonZero; k++){
            out[j] += histBuf[(bufPos + j - k + convlength) % convlength] * resp[k];
        }
    }

    return AMF_OK;
}

#ifndef TAN_NO_OPENCL

// GPU implementation
AMF_RESULT TANConvolutionImpl::ovlTimeDomainGPU(
    cl_mem resp,
    amf_uint32 firstNonZero,
    amf_uint32 lastNonZero,
    cl_mem out,
    cl_mem histBuf,
    amf_uint32 bufPos,
    amf_size datalength,
    amf_size convlength)
{
    cl_context context = m_pContextTAN->GetOpenCLContext();
    cl_command_queue cmdQueue = m_pContextTAN->GetOpenCLGeneralQueue();

    /*
    bufPos = bufPos % convlength;
    int len1, len2;
    len2 = 0;
    len1 = datalength;
    if (bufPos + len1 > convlength){
        len1 = convlength - bufPos;
        len2 = datalength - len1;
    }
    clEnqueueWriteBuffer(cmdQueue, histBuf, 1, bufPos*sizeof(float), len1*sizeof(float),
    */

        // allocate cl_mem (if not already)
        // copy data to cl_mem [ use subbuffers, map only updated part of histBuf ???]

        // run ther Time Domain kernel:

        cl_uint index = 0;
        cl_int err = CL_SUCCESS;
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(cl_mem), &histBuf);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(int), &convlength);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(int), &bufPos);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(int), &datalength);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(int), &firstNonZero);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(int), &lastNonZero);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(cl_mem), &resp);
        err = clSetKernelArg(m_TimeDomainKernel, index++, sizeof(cl_mem), &out);

        //...
        amf_size global[3] = { datalength, 0, 0 };
        amf_size local[3] = {1, 0, 0 };


        err = clEnqueueNDRangeKernel(cmdQueue, m_TimeDomainKernel, 1, NULL, global, local, 0, NULL, NULL);

    return AMF_OK;
}

#else

// GPU implementation
AMF_RESULT TANConvolutionImpl::ovlTimeDomainGPU(
    AMFBuffer * resp,
    amf_uint32 firstNonZero,
    amf_uint32 lastNonZero,
    AMFBuffer * out,
    AMFBuffer * histBuf,
    amf_uint32 bufPos,
    amf_size datalength,
    amf_size convlength)
{
    THROW_NOT_IMPLEMENTED;

    return AMF_NOT_IMPLEMENTED;
}
#endif

AMF_RESULT TANConvolutionImpl::ProcessInternal(
    int                     idx,
    const TANSampleBuffer & pInputData,
    TANSampleBuffer &       pOutputData,
    amf_size                nSamples,
    // Masks of flags from enum
    // TAN_CONVOLUTION_CHANNEL_FLAG.
    const amf_uint32        flagMasks[],
    amf_size *              pNumOfSamplesProcessed,
    int                     ocl_prev_input,
    int                     ocl_advance_time,
    int                     ocl_skip_stage,
    int                     ocl_crossfade_state
)
{
    //ToDo handle audio buffers in GPU memory:
    if (pInputData.GetType() != AMF_MEMORY_HOST)
    {
        return AMF_NOT_IMPLEMENTED;
    }

    int ret = GRAAL_SUCCESS;

    if (pNumOfSamplesProcessed)
    {
        *pNumOfSamplesProcessed = 0;
    }

    // update available channel list
    for (amf_uint32 channelId = 0; channelId < m_iChannels; channelId++)
    {
        if (!flagMasks)
        {
            m_availableChannels[channelId] = false;
            m_tailLeftOver[channelId] = m_length;
        }
        else if(flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT)
        {
            if(flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_FLUSH_STREAM)
            {
                m_availableChannels[channelId] = true;
                m_tailLeftOver[channelId] = 0;
            }
            else if (m_tailLeftOver[channelId] > 0)
            {
                m_availableChannels[channelId] = false;

                if (ocl_advance_time)
                {
                    m_tailLeftOver[channelId] -= static_cast<int>(nSamples);
                }

                if (m_tailLeftOver[channelId] <= 0)
                {
                    // Stopping a stream here we need to flush the convolutor.
                    AMF_RETURN_IF_FAILED(Flush(idx, channelId), L"Flush failed");
                }
            }
            else
            {
                m_availableChannels[channelId] = true;
                m_tailLeftOver[channelId] = 0;
            }
        }
        else
        {
            m_availableChannels[channelId] = false;
            m_tailLeftOver[channelId] = m_length;
        }

        if (flagMasks && flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_FLUSH_STREAM)
        {
            // if the channel has not been flushed flush it
            if (!m_flushedChannels[channelId])
            {
                AMF_RETURN_IF_FAILED(Flush(idx, channelId), L"Flush failed");
            }

            m_flushedChannels[channelId] = true;
        }
        else
        {
            m_flushedChannels[channelId] = false;
        }
    }

    // copy valid channel buffer pointers to internal list:
    int idxInt = 0;

    for (amf_uint32 channelId = 0; channelId < m_iChannels; channelId++)
    {
        if (!m_availableChannels[channelId]) // !available == running
        {
            if(m_internalInBufs.IsSet() && !m_internalInBufs.IsHost())
            {
                m_internalInBufs.Release();
            }

            if(!m_internalInBufs.IsSet())
            {
                m_internalInBufs.AllocateChannels(
                    m_iChannels,
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST
                    );
            }
            //else
            //{
            //    assert(m_internalInBufs.GetType() == amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST);
            //}

            if(!m_internalOutBufs.IsSet())
            {
                m_internalOutBufs.AllocateChannels(
                    m_iChannels,
                    pOutputData.GetType()
                    );
            }
            assert(m_internalOutBufs.GetType() == pOutputData.GetType());

            if(pOutputData.IsComputeBuffer())
            {
#ifndef TAN_NO_OPENCL
                m_internalOutBufs.ReferCLBuffers(idxInt, pOutputData.GetCLBuffers()[channelId]);
#else
                m_internalOutBufs.ReferAMFBuffers(idxInt, pOutputData.GetAMFBuffers()[channelId]);
#endif
            }
            else if(pOutputData.IsHost())
            {
                //m_internalInBufs.GetHostBuffers()[channelId] = m_silence;
		        //hack
		        m_internalOutBufs.ReferHostBuffer(idxInt, pOutputData.GetHostBuffers()[channelId]);
            }

            if (flagMasks && flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT)
            {
                m_internalInBufs.ReferHostBuffer(idxInt, m_silence);
            }
            else
            {
                assert(pInputData.IsHost());

                m_internalInBufs.ReferHostBuffer(idxInt, pInputData.GetHostBuffers()[channelId]);
            }

            if (!m_availableChannels[channelId])
            { // !available == running
            //if (flagMasks && flagMasks[channelId] & TAN_CONVOLUTION_CHANNEL_FLAG_STOP_INPUT)
            //{
            //	;//	m_internalInBufs.GetHostBuffers()[idxInt] =  m_silence;
            //}
            //else {
            //	;//    m_internalInBufs.GetHostBuffers()[idxInt] = pInputData.GetHostBuffers()[channelId];
            //}
                ++idxInt;
            }
		}
	}

    //PrintReducedFloatArray("in0", m_internalInBufs.GetHostBuffers()[0], sizeof(float) * nSamples);
    //PrintReducedFloatArray("in1", m_internalInBufs.GetHostBuffers()[1], sizeof(float) * nSamples);

    int n_channels = m_RunningChannels = idxInt; //hack
	if (!(n_channels > 0))
    {
        return AMF_WRONG_STATE;
    }
	//AMF_RETURN_IF_FALSE(n_channels > 0, AMF_WRONG_STATE, L"No active channels found");

    switch (m_eConvolutionMethod)
    {
    case TAN_CONVOLUTION_METHOD_TIME_DOMAIN:
    {
        THROW_NOT_IMPLEMENTED;

        return AMF_NOT_IMPLEMENTED;

        /*
        for (amf_uint32 channelId = 0, chIdInt = 0;
            channelId < static_cast<amf_uint32>(m_iChannels); channelId++)
        {
            // skip processing of stopped channels
            if (!m_availableChannels[channelId]) { // !available == running
                m_tdInternalFilterState[idx]->m_Filter[chIdInt] = m_tdFilterState[idx]->m_Filter[channelId];
#ifndef TAN_NO_OPENCL
				if (m_pContextTAN->GetOpenCLContext()!=nullptr)
				{
                    m_tdInternalFilterState[idx]->m_clFilter[chIdInt] = m_tdFilterState[idx]->m_clFilter[channelId];
                    m_tdInternalFilterState[idx]->m_clSampleHistory[chIdInt] = m_tdFilterState[idx]->m_clSampleHistory[channelId];
                    m_tdInternalFilterState[idx]->m_clTemp[chIdInt] = m_tdFilterState[idx]->m_clTemp[channelId];
				}
#else
                THROW_NOT_IMPLEMENTED;

                return AMF_NOT_IMPLEMENTED;
#endif

				m_tdInternalFilterState[idx]->m_SampleHistory[chIdInt] = m_tdFilterState[idx]->m_SampleHistory[channelId];
				m_tdInternalFilterState[idx]->m_sampHistPos[chIdInt] = m_tdFilterState[idx]->m_sampHistPos[channelId];
				m_tdInternalFilterState[idx]->firstNz[chIdInt] = m_tdFilterState[idx]->firstNz[channelId];
				m_tdInternalFilterState[idx]->lastNz[chIdInt] = m_tdFilterState[idx]->lastNz[channelId];

                ++chIdInt;
            }
        }

        amf_size numOfSamplesProcessed = ovlTDProcess(
            m_tdInternalFilterState[idx],
            m_internalInBufs.GetHostBuffers(),
            m_internalOutBufs.GetHostBuffers(),
            static_cast<int>(nSamples),
            n_channels
            );

        if(pNumOfSamplesProcessed)
        {
            *pNumOfSamplesProcessed = numOfSamplesProcessed;
        }

        return AMF_OK;
        */
    }
    break;

    case TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD:
    {
        ovlAddFilterState * state = &m_FilterState[idx];
        float **filter = state->m_Filter;
        float **overlap = state->m_Overlap;
        float **ifilter = state->m_internalFilter;
        float **ioverlap = state->m_internalOverlap;

        // copy valid state pointers to internal list:
        for
        (
            amf_uint32 channelId = 0, idxInt = 0;
            channelId < m_iChannels;
            channelId++
        )
        {
            // skip processing of stopped channels
            if (!m_availableChannels[channelId]) // !available == running
            {
                ifilter[idxInt] = filter[channelId];
                ioverlap[idxInt] = overlap[channelId];
                ++idxInt;
            }
        }

        amf_size numOfSamplesProcessed(0);

        AMF_RETURN_IF_FAILED(
            ovlAddProcess(
                numOfSamplesProcessed,
                state,
                m_internalInBufs,
                m_internalOutBufs,
                static_cast<int>(nSamples),
                n_channels,
                ocl_advance_time
                )
            );

        if(pNumOfSamplesProcessed)
        {
            *pNumOfSamplesProcessed = numOfSamplesProcessed;
        }

        //PrintReducedFloatArray("out0", m_internalOutBufs.GetHostBuffers()[0], sizeof(float) * nSamples);
        //PrintReducedFloatArray("out1", m_internalOutBufs.GetHostBuffers()[1], sizeof(float) * nSamples);

        return AMF_OK;
    }

	case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_UNIFORM:
	case TAN_CONVOLUTION_METHOD_FFT_PARTITIONED_NONUNIFORM:
	{
		_ovlNonUniformPartitionFilterState * state = m_nupFilterState[idx];
		float **filter = state->m_Filter;
		float **overlap = state->m_Overlap;
		float **ifilter = state->m_internalFilter;
		float **ioverlap = state->m_internalOverlap;
		float **dataParts = state->m_DataPartitions;
		float **idataParts = state->m_internalDataPartitions;

        //PrintReducedFloatArray("filter", filter[0], mNUPSize * sizeof(float));
        //PrintReducedFloatArray("overlap", overlap[0], m_length * sizeof(float));
		PrintReducedFloatArray("ifilter", ifilter[0], mNUPSize * sizeof(float));
		PrintReducedFloatArray("ioverlap", ioverlap[0], m_length * sizeof(float));
        //PrintReducedFloatArray("dataParts", dataParts[0], mNUPSize * sizeof(float));
		PrintReducedFloatArray("idataParts", idataParts[0], mNUPSize * sizeof(float));

        //PrintReducedFloatArray("filter", filter[1], mNUPSize * sizeof(float));
        //PrintReducedFloatArray("overlap", overlap[1], m_length * sizeof(float));
        //PrintReducedFloatArray("ifilter", overlap[1], mNUPSize * sizeof(float));
        //PrintReducedFloatArray("ioverlap", ioverlap[1], m_length * sizeof(float));
        //PrintReducedFloatArray("dataParts", dataParts[1], mNUPSize * sizeof(float));
        //PrintReducedFloatArray("idataParts", idataParts[1], mNUPSize * sizeof(float));

        // copy valid state pointers to internal list:
		for (amf_uint32 channelId = 0, idxInt = 0;
			channelId < static_cast<amf_uint32>(m_iChannels); channelId++)
		{
			// skip processing of stopped channels
			//hack
			idataParts[channelId] = dataParts[channelId];
			ioverlap[channelId] = overlap[channelId];
			ifilter[channelId] = filter[channelId];
			if (!m_availableChannels[channelId])
			{ // !available == running
				ifilter[idxInt] = filter[channelId];
				ioverlap[idxInt] = overlap[channelId];
				idataParts[idxInt] = dataParts[channelId];
				++idxInt;
			}
		}

		amf_size numOfSamplesProcessed = ovlNUPProcess(
            state,
            m_internalInBufs,
            m_internalOutBufs,
            static_cast<int>(nSamples),
			n_channels,
            ocl_advance_time,
            ocl_crossfade_state
            );

		if (pNumOfSamplesProcessed)
		{
			*pNumOfSamplesProcessed = numOfSamplesProcessed;
		}

        //PrintReducedFloatArray("out0", m_internalOutBufs.GetHostBuffers()[0], sizeof(float) * nSamples);
        //PrintReducedFloatArray("out1", m_internalOutBufs.GetHostBuffers()[1], sizeof(float) * nSamples);

		return AMF_OK;
	}
	break;

    case TAN_CONVOLUTION_METHOD_FFT_UNIFORM_PARTITIONED:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_PARTITIONED:
    case TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL:

        THROW_NOT_IMPLEMENTED;

        return AMF_NOT_IMPLEMENTED;
    /*
    {
        //AMFLock syncLock(&m_sectUpdate);

        graal::CGraalConv*graalConv = (graal::CGraalConv*)m_graal_conv;
        int idxInt = 0;
        for (amf_uint32 channelId = 0; channelId < static_cast<amf_uint32>(m_iChannels);
             channelId++)
        {
            if (!m_availableChannels[channelId]) { // !available == running
                m_s_versions[m_param_buf_idx][idxInt] = idx;
                m_s_channels[idxInt] = channelId;
                idxInt++;
            }
        }
        int n_channels = idxInt;
        if (nSamples < m_iBufferSizeInSamples)
        {
            // This case is only handled for host memory input/output cl_mem input or output will not be padded and will return error
            if (pInputData.GetType() != AMF_MEMORY_HOST || pOutputData.GetType() != AMF_MEMORY_HOST)
            {
                return AMF_NOT_IMPLEMENTED;
            }

            float **inp = (float**)malloc(2 * (sizeof(float*) + m_length * sizeof(float)) * n_channels);
            float **out = (float**)((char*)inp + (sizeof(float*) + m_length * sizeof(float)) * n_channels);
            memset(inp, 0, 2 * (sizeof(float*) + m_length * sizeof(float)) * n_channels);
            for (int n = 0; n < n_channels; n++)
            {
                inp[n] = (float*)(inp + n_channels) + n*m_length;
                out[n] = (float*)(out + n_channels) + n*m_length;
                memcpy(inp[n], m_internalInBufs.GetHostBuffers()[n], nSamples * sizeof(float));

            }
            ret = graalConv->process(n_channels,
                m_s_versions[m_param_buf_idx],
                m_s_channels,
                inp,
                out,
                ocl_prev_input,
                ocl_advance_time,
                ocl_skip_stage
                );
            if (ret != GRAAL_SUCCESS)
            {
                return AMF_UNEXPECTED;
            }

            for (int n = 0; n < n_channels; n++)
            {
                memcpy(m_internalOutBufs.GetHostBuffers()[n], out[n], nSamples * sizeof(float));
            }
            free(inp);

        }
        else
        {
            if (pOutputData.GetType() == AMF_MEMORY_HOST)
            {
                ret = graalConv->process(
                    n_channels,
                    m_s_versions[m_param_buf_idx],
                    m_s_channels,
                    m_internalInBufs.GetHostBuffersReadWrite(),
                    m_internalOutBufs.GetHostBuffersReadWrite(),
                    ocl_prev_input,
                    ocl_advance_time,
                    ocl_skip_stage,
                    ocl_crossfade_state
                    );
            }
            else
            {
#ifndef TAN_NO_OPENCL
                ret = graalConv->process(
                    n_channels,
                    m_s_versions[m_param_buf_idx],
                    m_s_channels,
                    m_internalInBufs.GetHostBuffers(),
                    m_internalOutBufs.GetCLBuffers(),
                    ocl_prev_input,
                    ocl_advance_time,
                    ocl_skip_stage,
                    ocl_crossfade_state
                    );
#else
                size_t channels(m_internalOutBufs.GetChannelsCount());
                std::vector<cl_mem> buffers(channels);

                for(size_t channel(0); channel < channels; ++channel)
                {
                    buffers[channel] = cl_mem(m_internalOutBufs.GetAMFBuffers()[channel]->GetNative());
                }

                ret = graalConv->process(
                    n_channels,
                    m_s_versions[m_param_buf_idx],
                    m_s_channels,
                    m_internalInBufs.GetHostBuffers(),
                    &buffers.front(),
                    ocl_prev_input,
                    ocl_advance_time,
                    ocl_skip_stage,
                    ocl_crossfade_state
                    );
#endif
            }

            if (ret != GRAAL_SUCCESS)
            {
                return AMF_UNEXPECTED;
            }
        }
        m_param_buf_idx = (m_param_buf_idx + 1) % PARAM_BUF_COUNT;
    }
    */
    break;

	case TAN_CONVOLUTION_METHOD_FHT_NONUNIFORM_PARTITIONED:
	{
		int idxInt = 0;
		for (amf_uint32 channelId = 0; channelId < static_cast<amf_uint32>(m_iChannels);
			channelId++)
		{
			if (!m_availableChannels[channelId])
			{ // !available == running
				m_s_channels[idxInt] = channelId;
				idxInt++;
			}
		}
		int n_channels = idxInt;

		if (pOutputData.GetType() == AMF_MEMORY_HOST)
		{
            THROW_NOT_IMPLEMENTED;

            return AMF_FAIL;

            /*
            m_nonUniformGraal.process(m_internalInBufs.GetHostBuffers(), m_internalOutBufs.GetHostBuffers(), nullptr, nSamples, n_channels, m_s_channels, idx, m_startNonUniformConvXFade);
            */

			m_startNonUniformConvXFade = false;
		}
		else
        {
			if(pOutputData.IsComputeBuffer())
			{
                THROW_NOT_IMPLEMENTED;

                return AMF_FAIL;

                /*
				m_nonUniformGraal.process(m_internalInBufs.GetHostBuffers(), m_internalOutBufs.GetHostBuffers(), m_internalOutBufs.GetCLBuffers(), nSamples, n_channels, m_s_channels, idx, m_startNonUniformConvXFade);
				*/

                m_startNonUniformConvXFade = false;
			}
        }
	}
	break;

    default:
        AMF_RETURN_IF_FALSE(false, AMF_NOT_IMPLEMENTED);
    }

    if (pNumOfSamplesProcessed)
    {
        *pNumOfSamplesProcessed = nSamples;
    }

    //PrintReducedFloatArray("out0", m_internalOutBufs.GetHostBuffers()[0], sizeof(float) * nSamples);
    //PrintReducedFloatArray("out1", m_internalOutBufs.GetHostBuffers()[1], sizeof(float) * nSamples);

    return AMF_OK;
}
