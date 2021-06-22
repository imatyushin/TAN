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
#include "Audio3DAMF.h"

#include "TrueAudioVR.h"
#include "GpuUtils.h"
#include "cpucaps.h"
#include "Exceptions.h"
#include "Debug.h"

#include "public/common/TraceAdapter.h"
#include "public/common/AMFFactoryHelper.h"

#include <time.h>
#include <stdio.h>

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
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

Audio3DAMF::Audio3DAMF()
{
}

Audio3DAMF::~Audio3DAMF()
{
}

void Audio3DAMF::Close()
{
    IAudio3D::Close();
}

AMF_RESULT Audio3DAMF::InitObjects()
{
    auto factory = g_AMFFactory.GetFactory();

    AMF_RETURN_IF_FAILED(
        TANCreateContext(
            TAN_FULL_VERSION,
            &mTANConvolutionContext,
            mComputeConvolution ? factory : nullptr
            ),
        L"TANCreateContext mTANConvolutionContext failed"
        );
    AMF_RETURN_IF_FAILED(
        TANCreateContext(
            TAN_FULL_VERSION,
            &mTANRoomContext,
            mComputeRoom ? factory : nullptr
            ),
        L"TANCreateContext mTANRoomContext failed"
        );

    // Allocate computes
    {
        int32_t flagsQ1 = 0;
        int32_t flagsQ2 = 0;

        //CL convolution on GPU
        if(mComputeConvolution && mComputeOverGpuConvolution)
        {
    #ifdef RTQ_ENABLED

    #define QUEUE_MEDIUM_PRIORITY                   0x00010000

    #define QUEUE_REAL_TIME_COMPUTE_UNITS           0x00020000
            if (mUseHPr_Conv){
                flagsQ1 = QUEUE_MEDIUM_PRIORITY;
            }

    else if (mUseRTQ_Conv){
                flagsQ1 = QUEUE_REAL_TIME_COMPUTE_UNITS | mCuRes_Conv;
            }

            if (mUseHPr_Room){
                flagsQ2 = QUEUE_MEDIUM_PRIORITY;
            }

            else if (mUseRTQ_Room){
                flagsQ2 = QUEUE_REAL_TIME_COMPUTE_UNITS | mCuRes_Room;
            }
    #endif // RTQ_ENABLED

            CreateGpuCommandQueues(mComputeDeviceIndexConvolution, flagsQ1, &mCompute1, flagsQ2, &mCompute2, &mContext12);

            //CL room on GPU
            if(mComputeRoom && mComputeOverGpuRoom && (mComputeDeviceIndexConvolution == mComputeDeviceIndexRoom))
            {
                mContext3 = mContext12;
                mCompute3 = mCompute2;
            }
        }

        //CL convolution on CPU
        else if(mComputeConvolution && !mComputeOverGpuConvolution)
        {
#ifdef RTQ_ENABLED
            THROW_NOT_IMPLEMENTED;

            // For " core "reservation" on CPU" -ToDo test and enable
            if (mCuRes_Conv > 0 && mCuRes_Room > 0)
            {
                AMF_RETURN_IF_FALSE(
                    true == CreateCommandQueuesWithCUcount(mComputeDeviceIndexConvolution, &mCompute1, &mCompute2, mCuRes_Conv, mCuRes_Room),
                    AMF_FAIL,
                    L"CreateCommandQueuesWithCUcount failed"
                    );
            }
            else
            {
#endif
                AMF_RETURN_IF_FALSE(
                    true == CreateCpuCommandQueues(mComputeDeviceIndexConvolution, 0, &mCompute1, 0, &mCompute2, &mContext12),
                    AMF_FAIL,
                    L"CreateCommandQueuesWithCUcount failed"
                    );

#ifdef RTQ_ENABLED
            }
#endif

            //CL room on CPU
            if(mComputeRoom && !mComputeOverGpuRoom && (mComputeDeviceIndexConvolution == mComputeDeviceIndexRoom))
            {
                mContext3 = mContext12;
                mCompute3 = mCompute2;
            }
        }

        //room queue not yet created
        if(!mCompute3 && mComputeRoom)
        {
            //CL over GPU
            if(mComputeOverGpuRoom)
            {
                CreateGpuCommandQueues(mComputeDeviceIndexRoom, 0, &mCompute3, 0, nullptr, &mContext3);
            }
            //CL over CPU
            else
            {
                CreateCpuCommandQueues(mComputeDeviceIndexRoom, 0, &mCompute3, 0, nullptr, &mContext3);
            }
        }
    }

    //convolution via compute
    if(mComputeConvolution)
    {
        AMF_RETURN_IF_FAILED(
            mTANConvolutionContext->InitAMF(
                mContext12,
                mCompute1,
                mContext12,
                mCompute2
                ),
            L"mTANConvolutionContext->InitAMF failed"
            );
    }

    //room processing via compute
    if(mComputeRoom)
    {
        AMF_RETURN_IF_FAILED(
            mTANRoomContext->InitAMF(
                mContext3,
                mCompute3,
                mContext3,
                mCompute3
                ),
            L"mTANRoomContext->InitAMF failed"
            );
    }

    AMF_RETURN_IF_FAILED(TANCreateConvolution(mTANConvolutionContext, &mConvolution), L"TANCreateConvolution failed");

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
    //todo, ivm:, investigate
    else //if(mComputeOverGpuConvolution)
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
        if(mContext12 == mContext3)
        {
            for(int i = 0; i < mWavFiles.size() * 2; i++)
            {
                AMF_RETURN_IF_FAILED(
                    mContext12->AllocBuffer(
                        mCompute1->GetMemoryType(),
                        mFFTLength * sizeof(float),
                        &mAMFResponses[i]
                        )
                    );

                float zero = 0.0;
                AMF_RETURN_IF_FAILED(
                    mCompute1->FillBuffer(
                        mAMFResponses[i],
                        0,
                        mFFTLength * sizeof(float),
                        &zero,
                        sizeof(float)
                        )
                    );

                mAMFResponsesInterfaces[i] = mAMFResponses[i];

                PrintAMFArray("mOCLResponses", mAMFResponses[i], mCompute1, 64);
            }

            //HACK out for test
            mUseComputeBuffers = true;
        }

        // Initialize CL output buffers

        // First create a big cl_mem buffer then create small sub-buffers from it
        AMFBufferPtr buffer;
        AMF_RETURN_IF_FAILED(
            mContext12->AllocBuffer(
                mCompute1->GetMemoryType(),
                mBufferSizeInBytes * mWavFiles.size() * STEREO_CHANNELS_COUNT,
                &buffer
                ),
                L"Could not create OpenCL buffer"
                );
        mOutputMainAMFBuffer = AMFBufferExPtr(buffer);
        AMF_RETURN_IF_FALSE(mOutputMainAMFBuffer != nullptr, AMF_NOT_SUPPORTED);

        /**/
        for(amf_uint32 i = 0; i < mWavFiles.size() * 2; i++)
        {
            AMF_RETURN_IF_FAILED(
                mOutputMainAMFBuffer->CreateSubBuffer(
                    &mOutputAMFBuffers[i],
                    i * mBufferSizeInBytes,
                    mBufferSizeInBytes
                    )
                );

            assert(mOutputAMFBuffers[i]->GetNative());

            mOutputAMFBuffersInterfaces[i] = mOutputAMFBuffers[i];

            float zero = 0.0;
            AMF_RETURN_IF_FAILED(
                mCompute1->FillBuffer(
                    mOutputAMFBuffers[i],
                    0,
					mBufferSizeInBytes,
                    &zero,
                    sizeof(zero)
                    )
                );
        }
        /**/

        for (int idx = 0; idx < 2; idx++)
        {
            AMF_RETURN_IF_FAILED(
                mContext12->AllocBuffer(
                    mCompute1->GetMemoryType(),
                    mBufferSizeInBytes,
                    &mOutputMixAMFBuffers[idx]
                    ),
                    L"Could not create OpenCL buffer"
                    );

            mOutputMixAMFBuffersInterfaces[idx] = mOutputMixAMFBuffers[idx];

            unsigned char zero = 0.0;
            AMF_RETURN_IF_FAILED(
                mCompute1->FillBuffer(
                    mOutputMixAMFBuffers[idx],
                    0,
                    mBufferSizeInBytes,
                    &zero,
                    sizeof(zero)
                    )
                );
        }

        // The output short buffer stores the final (after mixing) left and right channels interleaved as short samples
        // The short buffer size is equal to sizeof(short)*2*m_bufSize/sizeof(float) which is equal to m_bufSize
        AMF_RETURN_IF_FAILED(
            mContext12->AllocBuffer(
                mCompute1->GetMemoryType(),
                mBufferSizeInBytes,
                &mOutputShortAMFBuffer
                ),
                L"Could not create OpenCL buffer"
                );

        unsigned char zero = 0.0;
        AMF_RETURN_IF_FAILED(
            mCompute1->FillBuffer(
                mOutputShortAMFBuffer,
                0,
                mBufferSizeInBytes,
                &zero,
                sizeof(zero)
                )
            );
    }

#ifdef _WIN32
    HMODULE TanVrDll;
    TanVrDll = LoadLibraryA("TrueAudioVR.dll");

#ifndef TAN_NO_OPENCL
    typedef int (WINAPI *       CREATEVR)(
        AmdTrueAudioVR **       taVR,
        const TANContextPtr &   pContext,
        const TANFFTPtr &       pFft,
        cl_command_queue        cmdQueue,
        float                   samplesPerSecond,
        int                     convolutionLength
        );
#else
	typedef int (WINAPI *       CREATEVR)(
		AmdTrueAudioVR **       taVR,
		const TANContextPtr &   pContext,
		const TANFFTPtr &       pFft,
		AMFCompute *            compute,
		float                   samplesPerSecond,
		int                     convolutionLength,
		amf::AMFFactory *       factory
		);
#endif

	CREATEVR CreateAmdTrueAudioVR = nullptr;

    CreateAmdTrueAudioVR = (CREATEVR)GetProcAddress(TanVrDll, "CreateAmdTrueAudioVR");

#endif

    AmdTrueAudioVR *trueAudioVR(nullptr);
    auto result = CreateAmdTrueAudioVR(
        &trueAudioVR,
        mTANRoomContext,
        mFft,
        mCompute3,
        FILTER_SAMPLE_RATE, //todo: other frequencies?
        mFFTLength,
		factory
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

    //todo: initialize here and check result
    //mTrueAudioVR->Initialize();

    std::cout
        << "Room: " << mRoom.width<< "fm W x " << mRoom.length << "fm L x " << mRoom.height << "fm H"
        << std::endl;

    // head model:
    mTrueAudioVR->generateSimpleHeadRelatedTransform(
        mEars.hrtf,
        mEars.earSpacing
        );

    //To Do use gpu mem responses
    for (int idx = 0; idx < mWavFiles.size(); idx++)
    {
        if(mUseComputeBuffers)
        {
            AMF_RETURN_IF_FAILED(
                mTrueAudioVR->generateRoomResponse(
                    mRoom,
                    mSources[idx],
                    mEars,
                    FILTER_SAMPLE_RATE,
                    mFFTLength,
                    mAMFResponses[idx * 2],
                    mAMFResponses[idx * 2 + 1],
                    GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM,
                    50
                    )
                );
        }
        else
        {
            AMF_RETURN_IF_FAILED(
                mTrueAudioVR->generateRoomResponse(
                    mRoom,
                    mSources[idx],
                    mEars,
                    FILTER_SAMPLE_RATE,
                    mFFTLength,
                    mResponses[idx * 2],
                    mResponses[idx * 2 + 1],
                    GENROOM_LIMIT_BOUNCES,
                    50
                    )
                );
        }
    }

    if(mUseComputeBuffers)
    {
        AMF_RETURN_IF_FAILED(
            mConvolution->UpdateResponseTD(
                mAMFResponsesInterfaces,
                mFFTLength,
                nullptr,
                TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                )
            );
    }
    else
    {
        AMF_RETURN_IF_FAILED(
            mConvolution->UpdateResponseTD(
                mResponses,
                mFFTLength,
                nullptr,
                TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                )
            );
    }

    std::cout << "Playback started" << std::endl;

    mRunning = true;

    return AMF_OK;
}

bool Audio3DAMF::Run()
{
    // start main processing thread:
    mProcessThread = std::thread(processThreadProc, this);

    // wait for processing thread to start:
    while (!mRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // start update processing thread:
    mUpdateThread = std::thread(updateThreadProc, this);

    mUpdateParams = true;
    mStop = false;

    return true;
}

void Audio3DAMF::Stop()
{
    mStop = true;

    mProcessThread.WaitCloseInfinite();
    mUpdateThread.WaitCloseInfinite();

    Close();
}

AMF_RESULT Audio3DAMF::Process(int16_t *pOut, int16_t *pChan[MAX_SOURCES], uint32_t sampleCountBytes)
{
    uint32_t sampleCount = sampleCountBytes / (sizeof(int16_t) * STEREO_CHANNELS_COUNT);

    //PrintShortArray("::Process input[0]", pChan[0], STEREO_CHANNELS_COUNT * sampleCount * sizeof(int16_t), STEREO_CHANNELS_COUNT * sampleCount);

    // Read from the files
    for (int idx = 0; idx < mWavFiles.size(); idx++)
    {
        for(int chan = 0; chan < 2; chan++)
        {
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

            //PrintFloatArray("::Process, after Convert", mInputFloatBufs[idx * 2 + chan], sampleCount * sizeof(float));
        }
    }

    if(mComputedOutputPipeline)
    {
        // OCL device memory objects are passed to the TANConvolution->Process method.
        // Mixing and short conversion is done on GPU.

        AMF_RETURN_IF_FAILED(
            mConvolution->Process(
                mInputFloatBufs,
                mOutputAMFBuffersInterfaces,
                sampleCount,
                nullptr,
                nullptr
                )
            );

        //PrintAMFArray("::Convolution->Process[0]", mOutputAMFBuffersInterfaces[0], mCompute1, sampleCount * sizeof(float));
        //PrintAMFArray("::Convolution->Process[1]", mOutputAMFBuffersInterfaces[1], mCompute1, sampleCount * sizeof(float));

        AMFBuffer *outputAMFBufferLeft[MAX_SOURCES] = {nullptr};
        AMFBuffer *outputAMFBufferRight[MAX_SOURCES] = {nullptr};

        for(int src = 0; src < MAX_SOURCES; src++)
        {
            outputAMFBufferLeft[src] = mOutputAMFBuffers[src * 2];// Even indexed channels for left ear input
            outputAMFBufferRight[src] = mOutputAMFBuffers[src * 2 + 1];// Odd indexed channels for right ear input
        }

        //PrintAMFArray("::outputBufLeft", outputAMFBufferLeft[0], mCompute1, sampleCount * sizeof(float));
        //PrintAMFArray("::outputBufRight", outputAMFBufferRight[0], mCompute1, sampleCount * sizeof(float));

        //PrintAMFArray("::Mixer->MixB[0]", mOutputMixAMFBuffersInterfaces[0], mCompute1, sampleCount * sizeof(float));
        //PrintAMFArray("::Mixer->MixB[1]", mOutputMixAMFBuffersInterfaces[1], mCompute1, sampleCount * sizeof(float));

        AMF_RETURN_IF_FAILED(mMixer->Mix((AMFBuffer **)outputAMFBufferLeft, mOutputMixAMFBuffersInterfaces[0]));
        AMF_RETURN_IF_FAILED(mMixer->Mix((AMFBuffer **)outputAMFBufferRight, mOutputMixAMFBuffersInterfaces[1]));

        //PrintAMFArray("::Mixer->Mix[0]", mOutputMixAMFBuffers[0], mCompute1, sampleCount * sizeof(float));
        //PrintAMFArray("::Mixer->Mix[1]", mOutputMixAMFBuffers[1], mCompute1, sampleCount * sizeof(float));

        auto amfResult = mConverter->Convert(
            mOutputMixAMFBuffers[0],
            1, 0,
            TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortAMFBuffer,
            2, 0,
            TAN_SAMPLE_TYPE_SHORT,
            sampleCount,
            1.f
            );
        AMF_RETURN_IF_FALSE(amfResult == AMF_OK || amfResult == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);
        //PrintAMFArray("::Converter->Convert[0]", mOutputShortAMFBuffer, mCompute1, 64);

        amfResult = mConverter->Convert(
            mOutputMixAMFBuffers[1],
            1, 0,
            TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortAMFBuffer,
            2, 1,
            TAN_SAMPLE_TYPE_SHORT,
            sampleCount,
            1.f
            );
        AMF_RETURN_IF_FALSE(amfResult == AMF_OK || amfResult == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);
        //PrintAMFArrayWithOffset("::Converter->Convert[1]", mOutputShortAMFBuffer, mCompute1, 64, sampleCount * sizeof(short));

        AMF_RETURN_IF_FAILED(
            mTANConvolutionContext->GetConvQueue()->CopyBufferToHost(
                mOutputShortAMFBuffer,
                0,
                sampleCountBytes,
                pOut,
                true
                )
            );
    }

    else
    {   // Host memory pointers are passed to the TANConvolution->Process method
        // Mixing and short conversion are still performed on CPU.

        AMF_RESULT ret(
            mConvolution->Process(
                mInputFloatBufs,
                mOutputFloatBufs,
                sampleCount,
                nullptr,
                nullptr
                )
            );

        //PrintFloatArray("::Convolution->Process[0]", mOutputFloatBufs[0], sampleCount * sizeof(float));
        //PrintFloatArray("::Convolution->Process[1]", mOutputFloatBufs[1], sampleCount * sizeof(float));

        AMF_RETURN_IF_FAILED(ret);

        float * outputFloatBufLeft[MAX_SOURCES] = {nullptr};
        float * outputFloatBufRight[MAX_SOURCES] = {nullptr};

        for (int src = 0; src < MAX_SOURCES; src++)
        {
            outputFloatBufLeft[src] = mOutputFloatBufs[src * 2];// Even indexed channels for left ear input
            outputFloatBufRight[src] = mOutputFloatBufs[src * 2 + 1];// Odd indexed channels for right ear input
        }

        ret = mMixer->Mix(outputFloatBufLeft, mOutputMixFloatBufs[0]);
        AMF_RETURN_IF_FAILED(ret);

        ret = mMixer->Mix(outputFloatBufRight, mOutputMixFloatBufs[1]);
        AMF_RETURN_IF_FAILED(ret);

        ret = mConverter->Convert(mOutputMixFloatBufs[0], 1, sampleCount, pOut, 2, 1.f);
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, ret);

        ret = mConverter->Convert(mOutputMixFloatBufs[1], 1, sampleCount, pOut + 1, 2, 1.f);
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, ret);
    }

    //PrintShortArray("::Process, out[0]", pOut, sampleCount * sizeof(float));
    //PrintShortArray("::Process, out[1]", pOut + 1, sampleCount * sizeof(float));

    static int counter(0);

    if(!counter)
    {
        mUpdateParams = true;
    }

    auto info = ES + "Process " + std::to_string(counter) + " ===============================================";

    PrintDebug(info);

    if(++counter == 2)
    {
        int i = 0;
        ++i;
        //assert(false);
    }

    //mCompute1->FinishQueue();
    //mCompute2->FlushQueue();
    //mCompute2->FinishQueue();
    //mCompute3->FlushQueue();
    //mCompute3->FinishQueue();

    return AMF_OK;
}

unsigned Audio3DAMF::processThreadProc(void * ptr)
{
    Audio3DAMF *pAudio3D = static_cast<Audio3DAMF*>(ptr);

    return pAudio3D->ProcessProc();
}

int Audio3DAMF::ProcessProc()
{
    //uint32_t bytesRecorded(0);

    //use vector nor array because array are not memory-aligned
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

    /*std::cout << std::endl << "a1 wav:" << std::endl;
    for(int i(0); i < 1024; ++i)
    {
        std::cout << mWavFiles[0].Data[i] << " ";
    }
    std::cout << std::endl;*/

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

    /*std::cout << std::endl << "a1 prepared:" << std::endl;
    for(int i(0); i < mBufferSizeInBytes; ++i)
    {
        std::cout << short(pWaves[0][i]) << " ";
    }
    std::cout << std::endl;*/

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
            //todo: this is not correct for common case, add size calculation
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
                /*
                std::cout << std::endl << "a1 out:" << std::endl;
                for(int i(0); i < mBufferSizeInBytes; ++i)
                {
                    std::cout << short(outputBufferData[i]) << " ";
                }
                std::cout << std::endl;
                */

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

        /* ///compute current sample position for each stream:
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

unsigned Audio3DAMF::updateThreadProc(void * ptr)
{
    Audio3DAMF *pAudio3D = static_cast<Audio3DAMF*>(ptr);

    return pAudio3D->UpdateProc();
}

int Audio3DAMF::UpdateProc()
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
                mSources[idx].speakerX = mEars.headX;
                mSources[idx].speakerY = mEars.headY;
                mSources[idx].speakerZ = mEars.headZ;
            }

            if(mUseComputeBuffers)
            {
                AMF_RETURN_IF_FAILED(
                    mTrueAudioVR->generateRoomResponse(
                        mRoom,
                        mSources[idx],
                        mEars,
                        FILTER_SAMPLE_RATE,
                        mFFTLength,
                        mAMFResponsesInterfaces[idx * 2],
                        mAMFResponsesInterfaces[idx * 2 + 1],
                        GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM,
                        50
                        )
                    );
            }
            else
            {
                memset(mResponses[idx * 2], 0, sizeof(float )* mFFTLength);
                memset(mResponses[idx * 2 + 1], 0, sizeof(float) * mFFTLength);

                AMF_RETURN_IF_FAILED(
                    mTrueAudioVR->generateRoomResponse(
                        mRoom,
                        mSources[idx],
                        mEars,
                        FILTER_SAMPLE_RATE,
                        mFFTLength,
                        mResponses[idx * 2],
                        mResponses[idx * 2 + 1],
                        GENROOM_LIMIT_BOUNCES,
                        50
                        )
                    );
            }
        }

        AMF_RESULT ret = AMF_OK;

        while(mRunning && !mStop)
        {
            if(mUseComputeBuffers)
            {
                ret = mConvolution->UpdateResponseTD(
                    mAMFResponsesInterfaces,
                    mFFTLength,
                    nullptr,
                    TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                    );
            }
            else
            {
                ret = mConvolution->UpdateResponseTD(
                    mResponses,
                    mFFTLength,
                    nullptr,
                    TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                    );
            }

            if(ret != AMF_INPUT_FULL)
            {
                break;
            }
        }

        AMF_RETURN_IF_FAILED(ret);

        mUpdated = true;

        //Sleep(20);
        mUpdateParams = false;
    }

    return 0;
}
