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
#include "Audio3DOpenCL.h"

#include "TrueAudioVR.h"
#include "GpuUtils.h"
#include "Debug.h"
#include "cpucaps.h"

#include "public/common/TraceAdapter.h"
#include "public/common/AMFFactory.h"

#include <time.h>
#include <stdio.h>
#include <CL/cl.h>

#if defined(_WIN32)
    #include "../common/WASAPIPlayer.h"
#else
	#if !defined(__MACOSX) && !defined(__APPLE__)
		#include "../common/AlsaPlayer.h"
	#endif
#endif

#ifdef ENABLE_PORTAUDIO
	#include "../common/PortPlayer.h"
#endif

#include <immintrin.h>
#include <cmath>
#include <cstring>
#include <chrono>
#include <algorithm>

//to format debug outputs
/**/
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
/**/

bool Audio3DOpenCL::useIntrinsics = true; // InstructionSet::AVX() && InstructionSet::FMA();

#ifndef ERROR_MESSAGE

#ifdef _WIN32
#define ERROR_MESSAGE(message) ::MessageBoxA(0, #message, "Error", MB_OK)
#else
#define ERROR_MESSAGE(message) __asm__("int3"); std::cerr << "Error: " << message << std::endl
#endif

#endif

#define SAFE_DELETE_ARR(x) if (x) { delete[] x; x = nullptr; }

unsigned Audio3DOpenCL::processThreadProc(void * ptr)
{
    Audio3DOpenCL *pAudio3D = static_cast<Audio3DOpenCL*>(ptr);

    return pAudio3D->ProcessProc();
}

unsigned Audio3DOpenCL::updateThreadProc(void * ptr)
{
    Audio3DOpenCL *pAudio3D = static_cast<Audio3DOpenCL*>(ptr);

    return pAudio3D->UpdateProc();
}

Audio3DOpenCL::Audio3DOpenCL()
{
}

Audio3DOpenCL::~Audio3DOpenCL()
{
    Close();
}

void Audio3DOpenCL::Close()
{
    for (int i = 0; i < MAX_SOURCES*2; i++)
    {
        if (mOCLResponses[i] == NULL) continue;
        clReleaseMemObject(mOCLResponses[i]);
        mOCLResponses[i] = NULL;
    }

    for (int i = 0; i < mWavFiles.size() * 2; i++)
    {
        if (mOutputCLBufs[i] == NULL) continue;
        clReleaseMemObject(mOutputCLBufs[i]);
        mOutputCLBufs[i] = NULL;
    }

    if (mOutputMainCLbuf != NULL)
    {
        clReleaseMemObject(mOutputMainCLbuf);
        mOutputMainCLbuf = NULL;
    }
    for (int i = 0; i < 2; i++)
    {
        if (mOutputMixCLBufs[i] == NULL) continue;
        clReleaseMemObject(mOutputMixCLBufs[i]);
        mOutputMixCLBufs[i] = NULL;
    }
    if (mOutputShortBuf)
    {
        clReleaseMemObject(mOutputShortBuf);
        mOutputShortBuf = NULL;
    }
    mUseClMemBufs = false;

    // release smart pointers:
    mFft.Release();
    mConvolution.Release();
    mConverter.Release();
    mMixer.Release();
    mTANRoomContext.Release();
    mTANConvolutionContext.Release();

    /*
    why this called here too? queues was released inside convolution processor first!
    if (mCmdQueue1 != NULL){
        clReleaseCommandQueue(mCmdQueue1);
    }
    if (mCmdQueue2 != NULL){
        clReleaseCommandQueue(mCmdQueue2);
    }
    if (mCmdQueue3 != NULL && mCmdQueue3 != mCmdQueue2){
        clReleaseCommandQueue(mCmdQueue3);
    }
    */

    mCmdQueue1 = NULL;
    mCmdQueue2 = NULL;
    mCmdQueue3 = NULL;

    mWavFiles.resize(0);
}

AMF_RESULT Audio3DOpenCL::InitObjects()
{
    auto factory = g_AMFFactory.GetFactory();

    AMF_RETURN_IF_FAILED(TANCreateContext(TAN_FULL_VERSION, &mTANConvolutionContext, factory), L"TANCreateContext mTANConvolutionContext failed");
    AMF_RETURN_IF_FAILED(TANCreateContext(TAN_FULL_VERSION, &mTANRoomContext, factory), L"TANCreateContext mTANRoomContext failed");

    // Allocate RT-Queues
    {
        mCmdQueue1 = mCmdQueue2 = mCmdQueue3 = nullptr;

        int32_t flagsQ1 = 0;
        int32_t flagsQ2 = 0;

        //CL convolution on GPU
        if(mComputeConvolution && mComputeOverGpuConvolution)
        {
    #ifdef RTQ_ENABLED

    #define QUEUE_MEDIUM_PRIORITY                   0x00010000

    #define QUEUE_REAL_TIME_COMPUTE_UNITS           0x00020000
            if (useHPr_Conv){
                flagsQ1 = QUEUE_MEDIUM_PRIORITY;
            }

    else if (useRTQ_Conv){
                flagsQ1 = QUEUE_REAL_TIME_COMPUTE_UNITS | cuRes_Conv;
            }

            if (useHPr_IRGen){
                flagsQ2 = QUEUE_MEDIUM_PRIORITY;
            }

            else if (useRTQ_IRGen){
                flagsQ2 = QUEUE_REAL_TIME_COMPUTE_UNITS | cuRes_IRGen;
            }
    #endif // RTQ_ENABLED

            CreateGpuCommandQueues(mComputeDeviceIndexConvolution, flagsQ1, &mCmdQueue1, flagsQ2, &mCmdQueue2);

            //CL room on GPU
            if(mComputeRoom && mComputeOverGpuRoom && (mComputeDeviceIndexConvolution == mComputeDeviceIndexRoom))
            {
                mCmdQueue3 = mCmdQueue2;
            }
        }

        //CL convolution on CPU
        else if(mComputeConvolution && !mComputeOverGpuConvolution)
        {
#ifdef RTQ_ENABLED
            // For " core "reservation" on CPU" -ToDo test and enable
            if (cuRes_Conv > 0 && cuRes_IRGen > 0)
            {
                cl_int err = CreateCommandQueuesWithCUcount(mComputeDeviceIndexConvolution, &mCmdQueue1, &mCmdQueue2, cuRes_Conv, cuRes_IRGen);
            }
            else
            {
#endif
                CreateCpuCommandQueues(mComputeDeviceIndexConvolution, 0, &mCmdQueue1, 0, &mCmdQueue2);
#ifdef RTQ_ENABLED
            }
#endif

            //CL room on CPU
            if(mComputeRoom && !mComputeOverGpuRoom && (mComputeDeviceIndexConvolution == mComputeDeviceIndexRoom))
            {
                mCmdQueue3 = mCmdQueue2;
            }
        }

        clRetainCommandQueue(mCmdQueue1);
        clRetainCommandQueue(mCmdQueue2);

        //room queue not yet created
        if(!mCmdQueue3 && mComputeRoom)
        {
            //CL over GPU
            if(mComputeOverGpuRoom)
            {
                CreateGpuCommandQueues(mComputeDeviceIndexRoom, 0, &mCmdQueue3, 0, nullptr);
            }
            //CL over CPU
            else
            {
                CreateCpuCommandQueues(mComputeDeviceIndexRoom, 0, &mCmdQueue3, 0, nullptr);
            }
        }
    }

    //convolution over OpenCL
    if(mComputeConvolution)
    {
        AMF_RETURN_IF_FAILED(mTANConvolutionContext->InitOpenCL(mCmdQueue1, mCmdQueue2));
    }

    //room processing over OpenCL
    if(mComputeRoom)
    {
        AMF_RETURN_IF_FAILED(mTANRoomContext->InitOpenCL(mCmdQueue3, mCmdQueue3));
    }

    AMF_RETURN_IF_FAILED(TANCreateConvolution(mTANConvolutionContext, &mConvolution));

    if(mComputeConvolution)
    {
        AMF_RETURN_IF_FAILED(
            mConvolution->InitGpu(
                mConvolutionMethod,
                mFFTLength,
                mBufferSizeInSamples,
                mWavFiles.size() * STEREO_CHANNELS_COUNT
                )
            );
    }
    else
    {
        AMF_RETURN_IF_FAILED(
            mConvolution->InitCpu(
                mConvolutionMethod,
                mFFTLength,
                mBufferSizeInSamples,
                mWavFiles.size() * STEREO_CHANNELS_COUNT
                )
            );
    }

    AMF_RETURN_IF_FAILED(TANCreateConverter(mTANRoomContext, &mConverter));
    AMF_RETURN_IF_FAILED(mConverter->Init());

    AMF_RETURN_IF_FAILED(TANCreateMixer(mTANRoomContext, &mMixer));
	AMF_RETURN_IF_FAILED(mMixer->Init(mBufferSizeInSamples, mWavFiles.size()));

    AMF_RETURN_IF_FAILED(TANCreateFFT(mTANRoomContext, &mFft));
    AMF_RETURN_IF_FAILED(mFft->Init());

    //CL over GPU for both Convolution and Room processing
    if(mComputeConvolution && mComputeRoom)
    {
        cl_context context_IR;
        cl_context context_Conv;

        clGetCommandQueueInfo(mCmdQueue3, CL_QUEUE_CONTEXT, sizeof(cl_context), &context_IR, NULL);
        clGetCommandQueueInfo(mCmdQueue2, CL_QUEUE_CONTEXT, sizeof(cl_context), &context_Conv, NULL);

        if(context_IR == context_Conv)
        {
            for(int i = 0; i < mWavFiles.size() * 2; i++)
            {
                cl_int status = 0;
                mOCLResponses[i] = clCreateBuffer(context_IR, CL_MEM_READ_WRITE, mFFTLength * sizeof(float), NULL, &status);
            }

            //HACK out for test
            mUseClMemBufs = true;
        }

        // Initialize CL output buffers,
        cl_int clErr;

        // First create a big cl_mem buffer then create small sub-buffers from it
        mOutputMainCLbuf = clCreateBuffer(
            mTANConvolutionContext->GetOpenCLContext(),
            CL_MEM_READ_WRITE,
            mBufferSizeInBytes * mWavFiles.size() * STEREO_CHANNELS_COUNT,
            nullptr,
            &clErr
            );

        if(clErr != CL_SUCCESS)
        {
            std::cerr << "Could not create OpenCL buffer" << std::endl;

            return AMF_FAIL;
        }

        for(amf_uint32 i = 0; i < mWavFiles.size() * 2; i++)
        {
            cl_buffer_region region;
            region.origin = i * mBufferSizeInBytes;
            region.size = mBufferSizeInBytes;
            mOutputCLBufs[i] = clCreateSubBuffer(
                mOutputMainCLbuf, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region, &clErr);

            if (clErr != CL_SUCCESS)
            {
                std::cerr << "Could not create OpenCL subBuffer" << std::endl;

                return AMF_FAIL;
            }

            float zero = 0.0;
            clErr = clEnqueueFillBuffer(mCmdQueue1, mOutputCLBufs[i], &zero, sizeof(zero), 0, region.size, 0, NULL, NULL);
            if (clErr != CL_SUCCESS)
            {
                std::cerr << "Could not fill OpenCL subBuffer" << std::endl;

                return AMF_FAIL;
            }
        }

        for (int idx = 0; idx < 2; idx++)
        {
            mOutputMixCLBufs[idx] = clCreateBuffer(
                mTANConvolutionContext->GetOpenCLContext(),
                CL_MEM_READ_WRITE,
                mBufferSizeInBytes,
                nullptr,
                &clErr
                );

            if (clErr != CL_SUCCESS)
            {
                std::cerr << "Could not create OpenCL buffer" << std::endl;

                return AMF_FAIL;
            }

            if (clErr != CL_SUCCESS)
            {
                std::cerr << "Could not create OpenCL buffer" << std::endl;

                return AMF_FAIL;
            }
        }

        // The output short buffer stores the final (after mixing) left and right channels interleaved as short samples
        // The short buffer size is equal to sizeof(short)*2*m_bufSize/sizeof(float) which is equal to m_bufSize
        mOutputShortBuf = clCreateBuffer(
            mTANConvolutionContext->GetOpenCLContext(),
            CL_MEM_READ_WRITE,
            mBufferSizeInBytes,
            nullptr,
            &clErr
            );
    }

#ifdef _WIN32
    HMODULE TanVrDll;
    TanVrDll = LoadLibraryA("TrueAudioVR.dll");
    typedef int  (WINAPI *CREATEVR)(AmdTrueAudioVR **taVR, TANContextPtr pContext, TANFFTPtr pFft, cl_command_queue cmdQueue, float samplesPerSecond, int convolutionLength);
    CREATEVR CreateAmdTrueAudioVR = nullptr;

    CreateAmdTrueAudioVR = (CREATEVR)GetProcAddress(TanVrDll, "CreateAmdTrueAudioVR");
#endif

    AmdTrueAudioVR *trueAudioVR(nullptr);
    auto result = CreateAmdTrueAudioVR(
        &trueAudioVR,
        mTANRoomContext,
        mFft,
        mCmdQueue3,
        FILTER_SAMPLE_RATE, //todo: other frequencies?
        mFFTLength
        );
    mTrueAudioVR.reset(trueAudioVR);

    AMF_RETURN_IF_FAILED(result);
    AMF_RETURN_IF_FALSE(nullptr != mTrueAudioVR.get(), AMF_FAIL, L"CreateAmdTrueAudioVR failed");

    //todo: must check mComputeRoom nor mComputeOverGpuRoom?
    std::cout << std::endl << "Check this!" << std::endl;
    if(mComputeOverGpuRoom)
    {
        mTrueAudioVR->SetExecutionMode(AmdTrueAudioVR::GPU);
    }
    else
    {
        mTrueAudioVR->SetExecutionMode(AmdTrueAudioVR::CPU);
    }

    std::cout
        << "Room: " << room.width<< "fm W x " << room.length << "fm L x " << room.height << "fm H"
        << std::endl;

    // head model:
    mTrueAudioVR->generateSimpleHeadRelatedTransform(ears.hrtf, ears.earSpacing);

    //To Do use gpu mem responses
    for (int idx = 0; idx < mWavFiles.size(); idx++){
        if (mUseClMemBufs) {
            mTrueAudioVR->generateRoomResponse(room, sources[idx], ears, FILTER_SAMPLE_RATE, mFFTLength, mOCLResponses[idx * 2], mOCLResponses[idx * 2 + 1], GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM, 50);
        }
        else {
            mTrueAudioVR->generateRoomResponse(room, sources[idx], ears, FILTER_SAMPLE_RATE, mFFTLength, mResponses[idx * 2], mResponses[idx * 2 + 1], GENROOM_LIMIT_BOUNCES, 50);
        }
    }

    if (mUseClMemBufs)
    {
        AMF_RETURN_IF_FAILED(mConvolution->UpdateResponseTD(mOCLResponses, mFFTLength, nullptr, IR_UPDATE_MODE));
    }
    else
    {
        AMF_RETURN_IF_FAILED(mConvolution->UpdateResponseTD(mResponses, mFFTLength, nullptr, IR_UPDATE_MODE));
    }

    std::cout << "Playback started" << std::endl;

    mRunning = true;

    return AMF_OK;
}

bool Audio3DOpenCL::Run()
{
    // start main processing thread:
    //m_hProcessThread = (HANDLE)_beginthreadex(0, 10000000, processThreadProc, this, 0, 0);
    //RETURN_IF_FALSE(m_hProcessThread != (HANDLE)-1);
    mProcessThread = std::thread(processThreadProc, this);

    // wait for processing thread to start:
    while (!mRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // start update processing thread:
    //m_hUpdateThread = (HANDLE)_beginthreadex(0, 10000000, updateThreadProc, this, 0, 0);
    //RETURN_IF_FALSE(m_hUpdateThread != (HANDLE)-1);
    mUpdateThread = std::thread(updateThreadProc, this);

    mUpdateParams = true;
    mStop = false;

    return true;
}

void Audio3DOpenCL::Stop()
{
    mStop = true;

    mProcessThread.WaitCloseInfinite();
    mUpdateThread.WaitCloseInfinite();

    Close();
}

AMF_RESULT Audio3DOpenCL::Process(int16_t *pOut, int16_t *pChan[MAX_SOURCES], uint32_t sampleCountBytes)
{
    uint32_t sampleCount = sampleCountBytes / (sizeof(int16_t) * STEREO_CHANNELS_COUNT);

    PrintShortArray("::Process input[0]", pChan[0], STEREO_CHANNELS_COUNT * sampleCount * sizeof(int16_t), STEREO_CHANNELS_COUNT * sampleCount);

    // Read from the files
    for (int idx = 0; idx < mWavFiles.size(); idx++) {
        for (int chan = 0; chan < 2; chan++){
            // The way sources in inputFloatBufs are ordered is: Even indexed elements for left channels, odd indexed ones for right,
            // this ordering matches with the way impulse responses are generated and indexed to be convolved with the sources.
            AMF_RETURN_IF_FAILED(
                mConverter->Convert(
                    pChan[idx] + chan,
                    2,
                    sampleCount,
                    mInputFloatBufs[idx * 2 + chan],
                    1,
                    1.f
                    )
                );

            PrintFloatArray("::Process, after Convert", mInputFloatBufs[idx * 2 + chan], sampleCount * sizeof(float));
        }
    }

    if(mComputedOutputPipeline)
    {
        // OCL device memory objects are passed to the TANConvolution->Process method.
        // Mixing and short conversion is done on GPU.

        AMF_RETURN_IF_FAILED(mConvolution->Process(mInputFloatBufs, mOutputCLBufs, sampleCount, nullptr, nullptr));

        PrintCLArray("::Convolution->Process[0]", mOutputCLBufs[0], mCmdQueue1, sampleCount * sizeof(float));
        PrintCLArray("::Convolution->Process[1]", mOutputCLBufs[1], mCmdQueue1, sampleCount * sizeof(float));

        cl_mem outputCLBufLeft[MAX_SOURCES];
        cl_mem outputCLBufRight[MAX_SOURCES];

        for (int src = 0; src < MAX_SOURCES; src++)
        {
            outputCLBufLeft[src] = mOutputCLBufs[src*2 ];// Even indexed channels for left ear input
            outputCLBufRight[src] = mOutputCLBufs[src*2+1];// Odd indexed channels for right ear input
        }

        PrintCLArray("::outputBufLeft", outputCLBufLeft[0], mCmdQueue1, sampleCount * sizeof(float));
        PrintCLArray("::outputBufRight", outputCLBufRight[0], mCmdQueue1, sampleCount * sizeof(float));

        AMF_RETURN_IF_FAILED(mMixer->Mix(outputCLBufLeft, mOutputMixCLBufs[0]));
        AMF_RETURN_IF_FAILED(mMixer->Mix(outputCLBufRight, mOutputMixCLBufs[1]));

        PrintCLArray("::Mixer->Mix[0]", mOutputMixCLBufs[0], mCmdQueue1, sampleCount * sizeof(float));
        PrintCLArray("::Mixer->Mix[1]", mOutputMixCLBufs[1], mCmdQueue1, sampleCount * sizeof(float));

        auto ret = mConverter->Convert(mOutputMixCLBufs[0], 1, 0, TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortBuf, 2, 0, TAN_SAMPLE_TYPE_SHORT, sampleCount, 1.f);
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);

        ret = mConverter->Convert(mOutputMixCLBufs[1], 1, 0, TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortBuf, 2, 1, TAN_SAMPLE_TYPE_SHORT, sampleCount, 1.f);
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);

        PrintCLArray("::Converter->Convert[0]", mOutputMixCLBufs[0], mCmdQueue1, sampleCount * sizeof(float));
        PrintCLArray("::mConverter->Convert[1]", mOutputMixCLBufs[1], mCmdQueue1, sampleCount * sizeof(float));

        AMF_RETURN_IF_CL_FAILED(clEnqueueReadBuffer(mTANConvolutionContext->GetOpenCLConvQueue(), mOutputShortBuf, CL_TRUE,
             0, sampleCountBytes, pOut, NULL, NULL, NULL));
    }
    else
    {   // Host memory pointers are passed to the TANConvolution->Process method
        // Mixing and short conversion are still performed on CPU.

        AMF_RETURN_IF_FAILED(mConvolution->Process(mInputFloatBufs, mOutputFloatBufs, sampleCount,
            nullptr, nullptr));

        float * outputFloatBufLeft[MAX_SOURCES];
        float * outputFloatBufRight[MAX_SOURCES];

        for (int src = 0; src < MAX_SOURCES; src++)
        {
            outputFloatBufLeft[src] = mOutputFloatBufs[src * 2];// Even indexed channels for left ear input
            outputFloatBufRight[src] = mOutputFloatBufs[src * 2 + 1];// Odd indexed channels for right ear input
        }

        AMF_RETURN_IF_FAILED(mMixer->Mix(outputFloatBufLeft, mOutputMixFloatBufs[0]));
        AMF_RETURN_IF_FAILED(mMixer->Mix(outputFloatBufRight, mOutputMixFloatBufs[1]));

        auto ret = mConverter->Convert(mOutputMixFloatBufs[0], 1, sampleCount, pOut, 2, 1.f);
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);

        ret = mConverter->Convert(mOutputMixFloatBufs[1], 1, sampleCount, pOut + 1, 2, 1.f);
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);
    }

    PrintShortArray("::Process, out[0]", pOut, sampleCount * sizeof(float));
    PrintShortArray("::Process, out[1]", pOut + 1, sampleCount * sizeof(float));

#if 0// Old code: Crossfade, Mixing and Conversion on CPU

    for (int idx = 0; idx < mWavFiles.size(); idx++) {
        for (int chan = 0; chan < 2; chan++){
            RETURN_IF_FAILED(mConverter->Convert(pChan[idx] + chan, 2, sampleCount,
                inputFloatBufs[idx*2 + chan], 1, 1.f));
        }
    }

    RETURN_IF_FAILED(mConvolution->Process(inputFloatBufs, outputFloatBufs, sampleCount,
                                             nullptr, nullptr));

    // ToDo:  use limiter...


    for (int idx = 2; idx < 2 * mWavFiles.size(); idx += 2) {
        int k = 0;
        int n = sampleCount;
        while (n >= 8 && useIntrinsics){
            register __m256 *outL, *outR, *inL, *inR;
            outL = (__m256 *)&outputFloatBufs[0][k];
            outR = (__m256 *)&outputFloatBufs[1][k];
            inL = (__m256 *)&outputFloatBufs[idx][k];
            inR = (__m256 *)&outputFloatBufs[idx + 1][k];

            *outL = _mm256_add_ps(*outL, *inL);
            *outR = _mm256_add_ps(*outR, *inR);
            k += 8;
            n -= 8;
        }
        while(n > 0) {
            outputFloatBufs[0][k] += outputFloatBufs[idx][k];
            outputFloatBufs[1][k] += outputFloatBufs[idx + 1][k];
            k++;
            n--;
        }
    }


    AMF_RESULT ret = mConverter->Convert(outputFloatBufs[0], 1, sampleCount, pOut, 2, 1.f);
    RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);

    ret = mConverter->Convert(outputFloatBufs[1], 1, sampleCount, pOut + 1, 2, 1.f);
    RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);
#endif

    static int counter(0);

    auto info = ES + "Process " + std::to_string(counter) + " ===============================================";

    PrintDebug(info);

    if(++counter == 2)
    {
        //assert(false);
    }

    return AMF_OK;
}

int Audio3DOpenCL::ProcessProc()
{
    //uint32_t bytesRecorded(0);

    std::vector<uint8_t> recordBuffer(STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * sizeof(int16_t));
    std::vector<uint8_t> extractBuffer(STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * sizeof(int16_t));
    std::vector<int16_t> outputBuffer(STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE);

    //FifoBuffer recFifo(STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * sizeof(int16_t));
    Fifo recordFifo;
    recordFifo.Reset(STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * mWavFiles[0].GetSampleSizeInBytes());

    int16_t *pWaves[MAX_SOURCES] = {nullptr};
    int16_t *pWaveStarts[MAX_SOURCES] = {nullptr};

    uint32_t waveSizesInBytes[MAX_SOURCES] = {0};
    uint32_t waveBytesPlayed[MAX_SOURCES] = {0};

    auto buffers2Play = mMaxSamplesCount / mBufferSizeInSamples; //at least 1 block
    uint32_t buffersPlayed(0);
    uint32_t buffersPerSecond = FILTER_SAMPLE_RATE / mBufferSizeInSamples;
    uint32_t deltaTimeInMs = 1000 / buffersPerSecond;

    for(int file = 0; file < mWavFiles.size(); ++file)
    {
        int16_t *data16Bit((int16_t *)&(mWavFiles[file].Data[0]));

        pWaveStarts[file] = pWaves[file] = data16Bit;
        waveSizesInBytes[file] = mWavFiles[file].ChannelsCount * mWavFiles[file].SamplesCount * (mWavFiles[file].BitsPerSample / 8); // stereo short samples
    }

    while(!mUpdated && !mStop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto *processed = &mStereoProcessedBuffer.front();

    double previousTimerValue(0.0);
    bool firstFrame(true);

    while(!mStop)
    {
        uint32_t bytes2Play(0);

        if(mSrc1EnableMic)
        {
            while
            (
                !mStop
                &&
                (recordFifo.GetQueueSize() < mBufferSizeInBytes)
            )
            {
                // get some more:
                uint32_t recordedBytes = mPlayer->Record(
                    &recordBuffer.front(),
                    recordBuffer.size()
                    );

                recordFifo.Write(
                    &recordBuffer.front(),
                    recordedBytes
                    );

                //Sleep(5);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                //std::this_thread::sleep_for(std::chrono::milliseconds(0));
            }

            if(!mStop)
            {
                bytes2Play = uint32_t(std::min(recordFifo.GetQueueSize(), size_t(mBufferSizeInBytes)));

                recordFifo.Read(
                    &extractBuffer.front(),
                    bytes2Play
                    );

                // [temporarily] duck the wave audio for mic audio:
                int16_t *wavPtrTemp = pWaves[0];
                pWaves[0] = (int16_t*)&extractBuffer.front();
                Process(&outputBuffer.front(), pWaves, bytes2Play);
                pWaves[0] = wavPtrTemp;

                //to test
                /** /
                mPlayer->Play(
                    &extractBuffer.front(),
                    bytes2Extract,
                    false
                    );
                /**/
            }
        }
        else
        {
            //todo: this is not correct for a common case, add size calculation
            bytes2Play = mBufferSizeInBytes;

            Process(&outputBuffer.front(), pWaves, mBufferSizeInBytes);
        }

        //continue;
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        //memcpy(processed, &outputBuffer.front(), bytes2Play);

        uint32_t bytesTotalPlayed(0);
        uint8_t *outputBufferData = (uint8_t *)&outputBuffer.front();

        while(bytes2Play && !mStop)
        {
            //if(!mRealtimeTimer.IsStarted())
            {
                //mRealtimeTimer.Start();
                //firstFrame = true;
            }

            //double timerValue(firstFrame ? 0.0 : mRealtimeTimer.Sample());

            if
            (
                firstFrame
                ||
                true
                //((timerValue - previousTimerValue) * 1000 > 0.7 * deltaTimeInMs)
            )
            {
                auto bytesPlayed = mPlayer->Play(outputBufferData, bytes2Play, false);
                bytesTotalPlayed += bytesPlayed;

                outputBufferData += bytesPlayed;
                bytes2Play -= bytesPlayed;

                //previousTimerValue = timerValue;
                firstFrame = false;
            }
            else
            {
                //std::this_thread::sleep_for(std::chrono::milliseconds(2));
                std::this_thread::sleep_for(std::chrono::milliseconds(0));
            }
        }

        for(int fileIndex = 0; fileIndex < mWavFiles.size(); ++fileIndex)
        {
            waveBytesPlayed[fileIndex] += bytesTotalPlayed;

            //todo: eliminate size gaps
            //size of wav are not exactly rounded to mBufferSizeInBytes
            if(waveBytesPlayed[fileIndex] + mBufferSizeInBytes < waveSizesInBytes[fileIndex])
            {
                pWaves[fileIndex] += (mBufferSizeInBytes / sizeof(int16_t));
                /*std::cout
                  << "pl " << bytesTotalPlayed
                  << " fr " << mWavFiles[fileIndex].Data.size()
                  << std::endl;*/
            }
            else
            {
                //play wav again
                pWaves[fileIndex] = pWaveStarts[fileIndex];
                waveBytesPlayed[fileIndex] = 0;
            }
        }

        if(processed - &mStereoProcessedBuffer.front() + (mBufferSizeInBytes / sizeof(int16_t)) > mMaxSamplesCount)
        {
            processed = &mStereoProcessedBuffer.front();
        }
        else
        {
            processed += (mBufferSizeInBytes / sizeof(int16_t));
        }

        /*///compute current sample position for each stream:
        for (int i = 0; i < mWavFiles.size(); i++)
        {
            m_samplePos[i] = (pWaves[i] - pWaveStarts[i]) / sizeof(short);
        }*/

        //std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::this_thread::sleep_for(std::chrono::milliseconds(0));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    /*
    if(!WriteWaveFileS(
        "RoomAcousticsRun.wav",
        FILTER_SAMPLE_RATE,
        2,
        16,
        //mWavFiles[0].SamplesCount, //why not a max?
        mMaxSamplesCount,
        &mStereoProcessedBuffer.front()
        ))
    {
        puts("unable to write RoomAcousticsRun.wav");
    }
    else
    {
        puts("wrote output to RoomAcousticsRun.wav");
    }*/

    mRunning = false;

    return 0;
}

/*int64_t Audio3DOpenCL::getCurrentPosition(int streamIdx)
{
    return m_samplePos[streamIdx];
}*/

int Audio3DOpenCL::UpdateProc()
{
    while(mRunning && !mStop)
    {
        while(!mUpdateParams && mRunning && !mStop)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        for(int idx = 0; !mStop && (idx < mWavFiles.size()); idx++)
        {
            if(mTrackHeadPos[idx])
            {
                sources[idx].speakerX = ears.headX;
                sources[idx].speakerY = ears.headY;
                sources[idx].speakerZ = ears.headZ;
            }

            if(mUseClMemBufs)
            {
                mTrueAudioVR->generateRoomResponse(
                    room,
                    sources[idx],
                    ears,
                    FILTER_SAMPLE_RATE,
                    mFFTLength,
                    mOCLResponses[idx * 2],
                    mOCLResponses[idx * 2 + 1],
                    GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM,
                    50
                    );
            }
            else
            {
                memset(mResponses[idx * 2], 0, sizeof(float )* mFFTLength);
                memset(mResponses[idx * 2 + 1], 0, sizeof(float) * mFFTLength);

                mTrueAudioVR->generateRoomResponse(
                    room,
                    sources[idx],
                    ears,
                    FILTER_SAMPLE_RATE,
                    mFFTLength,
                    mResponses[idx * 2],
                    mResponses[idx * 2 + 1],
                    GENROOM_LIMIT_BOUNCES,
                    50
                    );
            }
        }

        AMF_RESULT ret = AMF_OK;

        while(mRunning && !mStop)
        {
            if(mUseClMemBufs)
            {
                ret = mConvolution->UpdateResponseTD(mOCLResponses, mFFTLength, nullptr, IR_UPDATE_MODE);
            }
            else
            {
                ret = mConvolution->UpdateResponseTD(mResponses, mFFTLength, nullptr, IR_UPDATE_MODE);
            }

            if(ret != AMF_INPUT_FULL)
            {
                break;
            }
        }

        //todo: investigate about AMF_BUSY
        AMF_RETURN_IF_FAILED(ret);

        mUpdated = true;

        //Sleep(20);
        mUpdateParams = false;
    }

    return 0;
}
