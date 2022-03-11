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
#include "public/common/AMFFactory.h"

#ifndef TAN_NO_OPENCL
  #include "OCLHelper.h" //for FixedEnqueueFillBuffer
#endif

//#include <time.h>
//#include <stdio.h>

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

//#include <immintrin.h>
//#include <cmath>
//#include <cstring>
//#include <chrono>
//#include <algorithm>

//to format debug outputs
//#include <fstream>
//#include <sstream>
//#include <iostream>
//#include <iomanip>

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
            mConvolutionViaCompute ? factory : nullptr
            ),
        L"TANCreateContext mTANConvolutionContext failed"
        );
    AMF_RETURN_IF_FAILED(
        TANCreateContext(
            TAN_FULL_VERSION,
            &mTANRoomContext,
            mRoomViaCompute ? factory : nullptr
            ),
        L"TANCreateContext mTANRoomContext failed"
        );

    if(mFFTType.length())
    {
        mTANConvolutionContext->SetProperty(L"TAN_FFT_TYPE", mFFTType.c_str());
        mTANRoomContext->SetProperty(L"TAN_FFT_TYPE", mFFTType.c_str());
    }

    // Allocate computes
    {
        int32_t flagsQ1 = 0;
        int32_t flagsQ2 = 0;

        //CL convolution on GPU
        if(mConvolutionViaCompute && mConvolutionComputeOnGPU)
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

#ifndef TAN_NO_OPENCL
            AMF_RETURN_IF_FAILED(CreateGpuCommandQueues(mConvolutionComputeDeviceIndex, flagsQ1, &mCmdQueue1, flagsQ2, &mCmdQueue2));
#else
            AMF_RETURN_IF_FAILED(CreateGpuCommandQueues(mConvolutionComputeDeviceIndex, flagsQ1, &mCompute1, flagsQ2, &mCompute2, &mContext12));
#endif

            //CL room on GPU
            if(mRoomViaCompute && mRoomComputeOnGPU && (mConvolutionComputeDeviceIndex == mRoomComputeDeviceIndex))
            {
#ifndef TAN_NO_OPENCL
                mCmdQueue3 = mCmdQueue2;
#else
                mContext3 = mContext12;
                mCompute3 = mCompute2;
#endif
            }
        }

        //CL convolution on CPU
        else if(mConvolutionViaCompute && !mConvolutionComputeOnGPU)
        {
#ifdef RTQ_ENABLED
            // For " core "reservation" on CPU" -ToDo test and enable
            if (mCuRes_Conv > 0 && mCuRes_Room > 0)
            {
#ifndef TAN_NO_OPENCL
                AMF_RETURN_IF_FAILED(
                    CreateCommandQueuesWithCUcount(mConvolutionComputeDeviceIndex, &mCmdQueue1, &mCmdQueue2, mCuRes_Conv, mCuRes_Room),
                    L"CreateCommandQueuesWithCUcount failed"
                    );
#else
                AMF_RETURN_IF_FAILED(
                    CreateCommandQueuesWithCUcount(mConvolutionComputeDeviceIndex, &mCompute1, &mCompute2, mCuRes_Conv, mCuRes_Room),
                    L"CreateCommandQueuesWithCUcount failed"
                    );
#endif
            }
            else
            {
#endif

#ifndef TAN_NO_OPENCL
                AMF_RETURN_IF_FAILED(
                    CreateCpuCommandQueues(mConvolutionComputeDeviceIndex, 0, &mCmdQueue1, 0, &mCmdQueue2),
                    L"CreateCommandQueuesWithCUcount failed"
                    );
#else
                AMF_RETURN_IF_FAILED(
                    CreateCpuCommandQueues(mConvolutionComputeDeviceIndex, 0, &mCompute1, 0, &mCompute2, &mContext12),
                    L"CreateCommandQueuesWithCUcount failed"
                    );
#endif

#ifdef RTQ_ENABLED
            }
#endif

            //CL room on CPU
            if(mRoomViaCompute && !mRoomComputeOnGPU && (mConvolutionComputeDeviceIndex == mRoomComputeDeviceIndex))
            {
#ifndef TAN_NO_OPENCL
                mCmdQueue3 = mCmdQueue2;
#else
                mContext3 = mContext12;
                mCompute3 = mCompute2;
#endif
            }
        }

        //room queue not yet created
        if(
            mRoomViaCompute &&
#ifndef TAN_NO_OPENCL
            !mCmdQueue3
#else
            !mCompute3
#endif
        )
        {
            //CL over GPU
            if(mRoomComputeOnGPU)
            {
#ifndef TAN_NO_OPENCL
                AMF_RETURN_IF_FAILED(CreateGpuCommandQueues(mRoomComputeDeviceIndex, 0, &mCmdQueue3, 0, nullptr));
#else
                AMF_RETURN_IF_FAILED(CreateGpuCommandQueues(mRoomComputeDeviceIndex, 0, &mCompute3, 0, nullptr, &mContext3));
#endif
            }
            //CL over CPU
            else
            {
#ifndef TAN_NO_OPENCL
                AMF_RETURN_IF_FAILED(CreateCpuCommandQueues(mRoomComputeDeviceIndex, 0, &mCmdQueue3, 0, nullptr));
#else
                AMF_RETURN_IF_FAILED(CreateCpuCommandQueues(mRoomComputeDeviceIndex, 0, &mCompute3, 0, nullptr, &mContext3));
#endif
            }
        }
    }

#ifndef TAN_NO_OPENCL
    if(mCmdQueue1)
    {
        clGetCommandQueueInfo(mCmdQueue1, CL_QUEUE_CONTEXT, sizeof(cl_context), &mContext12, nullptr);
    }

    if(mCmdQueue3)
    {
        clGetCommandQueueInfo(mCmdQueue3, CL_QUEUE_CONTEXT, sizeof(cl_context), &mContext3, nullptr);
    }
#endif

    //convolution via compute
    if(mConvolutionViaCompute)
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(mTANConvolutionContext->InitOpenCL(mCmdQueue1, mCmdQueue2));
#else
        AMF_RETURN_IF_FAILED(
            mTANConvolutionContext->InitAMF(
                mContext12,
                mCompute1,
                mContext12,
                mCompute2
                ),
            L"mTANConvolutionContext->InitAMF failed"
            );
#endif
    }

    //room processing via compute
    if(mRoomViaCompute)
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(mTANRoomContext->InitOpenCL(mCmdQueue3, mCmdQueue3));
#else
        AMF_RETURN_IF_FAILED(
            mTANRoomContext->InitAMF(
                mContext3,
                mCompute3,
                mContext3,
                mCompute3
                ),
            L"mTANRoomContext->InitAMF failed"
            );
#endif
    }

    AMF_RETURN_IF_FAILED(TANCreateConvolution(mTANConvolutionContext, &mConvolution), L"TANCreateConvolution failed");

    if(mConvolutionViaCompute)
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
    else //if(mConvolutionComputeOnGPU)
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
    if(mConvolutionViaCompute && mRoomViaCompute)
    {
        if(mContext12 == mContext3)
        {
#ifndef TAN_NO_OPENCL
            std::vector<cl_uchar> clear(mFFTLength * sizeof(float), 0);
#endif

            for(int i = 0; i < mWavFiles.size() * 2; i++)
            {
#ifndef TAN_NO_OPENCL
                cl_int status = 0;
                mOCLResponses[i] = clCreateBuffer(
                    mContext3,
                    CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                    mFFTLength * sizeof(float),
                    clear.data(),
                    &status
                    );
                AMF_RETURN_IF_CL_FAILED(status);

#else
                AMF_RETURN_IF_FAILED(
                    mContext3->AllocBuffer(
                        mCompute3->GetMemoryType(),
                        mFFTLength * sizeof(float),
                        &mAMFResponses[i]
                        )
                    );

                //amf implementation needs custom cleaning!
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

                //to later use them like AMFBuffer **
                mAMFResponsesInterfaces[i] = mAMFResponses[i];
#endif
                //PrintCLArray("mOCLResponses", mOCLResponses[i], mCmdQueue1, 64);
                //PrintAMFArrayReduced("mOCLResponses", mAMFResponses[i], mCompute1, mAMFResponses[i]->GetSize());
            }

            //HACK out for test
            mUseComputeBuffers = true;
        }

        // Initialize compute output buffers

        // First create a big compute buffer then create small sub-buffers from it
#ifndef TAN_NO_OPENCL
        cl_int clErr(0);

        mOutputMainCLbuf = clCreateBuffer(
            mTANConvolutionContext->GetOpenCLContext(),
            CL_MEM_READ_WRITE,
            mBufferSizeInBytes * mWavFiles.size() * STEREO_CHANNELS_COUNT,
            nullptr,
            &clErr
            );
        AMF_RETURN_IF_CL_FAILED(clErr, L"Could not create OpenCL buffer");

#else
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
#endif

        for(amf_uint32 i = 0; i < mWavFiles.size() * 2; i++)
        {
#ifndef TAN_NO_OPENCL
            cl_buffer_region region = {0};
            region.origin = i * mBufferSizeInBytes;
            region.size = mBufferSizeInBytes;

            mOutputCLBufs[i] = clCreateSubBuffer(
                mOutputMainCLbuf,
                CL_MEM_READ_WRITE,
                CL_BUFFER_CREATE_TYPE_REGION,
                &region,
                &clErr
                );

            AMF_RETURN_IF_CL_FAILED(clErr, L"Could not create OpenCL subBuffer");

            float zero = 0.0;
            clErr = FixedEnqueueFillBuffer(mContext12, mCmdQueue1, mOutputCLBufs[i], &zero, sizeof(zero), 0, region.size);
            AMF_RETURN_IF_CL_FAILED(clErr, L"Could not fill OpenCL subBuffer");
#else
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
#endif
        }

#ifndef TAN_NO_OPENCL
        std::vector<cl_uchar> clear(mBufferSizeInBytes, 0);
#endif

        for (int idx = 0; idx < 2; idx++)
        {
#ifndef TAN_NO_OPENCL
            mOutputMixCLBufs[idx] = clCreateBuffer(
                mTANConvolutionContext->GetOpenCLContext(),
                CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                mBufferSizeInBytes,
                clear.data(),
                &clErr
                );
            AMF_RETURN_IF_CL_FAILED(clErr, L"Could not create OpenCL buffer");
#else
            AMF_RETURN_IF_FAILED(
                mContext12->AllocBuffer(
                    mCompute1->GetMemoryType(),
                    mBufferSizeInBytes,
                    &mOutputMixAMFBuffers[idx]
                    ),
                    L"Could not create OpenCL buffer"
                    );

            mOutputMixAMFBuffersInterfaces[idx] = mOutputMixAMFBuffers[idx];

            //amf buffer needs to be filled manually
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
#endif
        }

        // The output short buffer stores the final (after mixing) left and right channels interleaved as short samples
        // The short buffer size is equal to sizeof(short)*2*m_bufSize/sizeof(float) which is equal to m_bufSize
#ifndef TAN_NO_OPENCL
        mOutputShortCLBuf = clCreateBuffer(
            mTANConvolutionContext->GetOpenCLContext(),
            CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
            mBufferSizeInBytes,
            clear.data(),
            &clErr
            );
#else
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
#endif
    }

#ifdef _WIN32
    HMODULE TanVrDll = LoadLibraryA("TrueAudioVR.dll");

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
#ifndef TAN_NO_OPENCL
        mCmdQueue3,
#else
        mCompute3,
#endif
        FILTER_SAMPLE_RATE, //todo: other frequencies?
        mFFTLength
#ifdef TAN_NO_OPENCL
		, factory
#endif
        );
    mTrueAudioVR.reset(trueAudioVR);

    AMF_RETURN_IF_FAILED(result);
    AMF_RETURN_IF_FALSE(nullptr != mTrueAudioVR.get(), AMF_FAIL, L"CreateAmdTrueAudioVR failed");

    //todo: must check mRoomViaCompute nor mRoomComputeOnGPU?
    std::cout << std::endl << "ToDo: check this!" << std::endl;
    if(mRoomComputeOnGPU)
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
#ifndef TAN_NO_OPENCL
            AMF_RETURN_IF_FAILED(
                mTrueAudioVR->generateRoomResponse(mRoom, mSources[idx], mEars, FILTER_SAMPLE_RATE, mFFTLength, mOCLResponses[idx * 2], mOCLResponses[idx * 2 + 1], GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM, 50)
                );
#else
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
#endif
        }
        else
        {
#ifndef TAN_NO_OPENCL
            AMF_RETURN_IF_FAILED(
                mTrueAudioVR->generateRoomResponse(mRoom, mSources[idx], mEars, FILTER_SAMPLE_RATE, mFFTLength, mResponses[idx * 2], mResponses[idx * 2 + 1], GENROOM_LIMIT_BOUNCES, 50)
                );
#else
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
#endif
        }
    }

    if(mUseComputeBuffers)
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(mConvolution->UpdateResponseTD(mOCLResponses, mFFTLength, nullptr, TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY));
#else
        AMF_RETURN_IF_FAILED(
            mConvolution->UpdateResponseTD(
                mAMFResponsesInterfaces,
                mFFTLength,
                nullptr,
                TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                )
            );
#endif
    }
    else
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(mConvolution->UpdateResponseTD(mResponses, mFFTLength, nullptr, TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY));
#else
        AMF_RETURN_IF_FAILED(
            mConvolution->UpdateResponseTD(
                mResponses,
                mFFTLength,
                nullptr,
                TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                )
            );
#endif
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

    PrintArrayReduced("::Process input[0]", pChan[0], STEREO_CHANNELS_COUNT * sampleCount * sizeof(int16_t));
    PrintArrayReduced("::Process input[1]", pChan[1], STEREO_CHANNELS_COUNT * sampleCount * sizeof(int16_t));

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

            PrintArrayReduced("::Process, after Convert", mInputFloatBufs[idx * 2 + chan], sampleCount * sizeof(float));
        }
    }

    if(mComputedOutputPipeline)
    {
        // OCL device memory objects are passed to the TANConvolution->Process method.
        // Mixing and short conversion is done on GPU.
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(mConvolution->Process(mInputFloatBufs, mOutputCLBufs, sampleCount, nullptr, nullptr));

        PrintCLArrayReduced("aft Prc[0]", mOutputCLBufs[0], mCmdQueue1, sampleCount * sizeof(float));
        PrintCLArrayReduced("aft Prc[1]", mOutputCLBufs[1], mCmdQueue1, sampleCount * sizeof(float));
#else
        AMF_RETURN_IF_FAILED(
            mConvolution->Process(
                mInputFloatBufs,
                mOutputAMFBuffersInterfaces,
                sampleCount,
                nullptr,
                nullptr
                )
            );
        PrintAMFArrayReduced("aft Prc[0]", mOutputAMFBuffersInterfaces[0], mCompute1, mOutputAMFBuffersInterfaces[0]->GetSize());
        PrintAMFArrayReduced("aft Prc[1]", mOutputAMFBuffersInterfaces[1], mCompute1, mOutputAMFBuffersInterfaces[1]->GetSize());
#endif

#ifndef TAN_NO_OPENCL
        cl_mem outputCLBufLeft[MAX_SOURCES];
        cl_mem outputCLBufRight[MAX_SOURCES];

        for (int src = 0; src < MAX_SOURCES; src++)
        {
            outputCLBufLeft[src] = mOutputCLBufs[src * 2];// Even indexed channels for left ear input
            outputCLBufRight[src] = mOutputCLBufs[src * 2 + 1];// Odd indexed channels for right ear input
        }
#else
        AMFBuffer *outputAMFBufferLeft[MAX_SOURCES] = {nullptr};
        AMFBuffer *outputAMFBufferRight[MAX_SOURCES] = {nullptr};

        for(int src = 0; src < MAX_SOURCES; src++)
        {
            outputAMFBufferLeft[src] = mOutputAMFBuffers[src * 2];// Even indexed channels for left ear input
            outputAMFBufferRight[src] = mOutputAMFBuffers[src * 2 + 1];// Odd indexed channels for right ear input
        }
#endif

        //PrintAMFArray("::outputBufLeft", outputAMFBufferLeft[0], mCompute1, sampleCount * sizeof(float));
        //PrintAMFArray("::outputBufRight", outputAMFBufferRight[0], mCompute1, sampleCount * sizeof(float));

        //PrintAMFArray("::Mixer->MixB[0]", mOutputMixAMFBuffersInterfaces[0], mCompute1, sampleCount * sizeof(float));
        //PrintAMFArray("::Mixer->MixB[1]", mOutputMixAMFBuffersInterfaces[1], mCompute1, sampleCount * sizeof(float));

#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(mMixer->Mix(outputCLBufLeft, mOutputMixCLBufs[0]));
        AMF_RETURN_IF_FAILED(mMixer->Mix(outputCLBufRight, mOutputMixCLBufs[1]));

        PrintCLArrayReduced("aft Mix[0]", mOutputMixCLBufs[0], mCmdQueue1, sampleCount * sizeof(float));
        PrintCLArrayReduced("aft Mix[1]", mOutputMixCLBufs[1], mCmdQueue1, sampleCount * sizeof(float));

#else
        AMF_RETURN_IF_FAILED(mMixer->Mix((AMFBuffer **)outputAMFBufferLeft, mOutputMixAMFBuffersInterfaces[0]));
        AMF_RETURN_IF_FAILED(mMixer->Mix((AMFBuffer **)outputAMFBufferRight, mOutputMixAMFBuffersInterfaces[1]));

        PrintAMFArrayReduced("aft Mix[0]", mOutputMixAMFBuffersInterfaces[0], mCompute1, sampleCount * sizeof(float));
        PrintAMFArrayReduced("aft Mix[1]", mOutputMixAMFBuffersInterfaces[1], mCompute1, sampleCount * sizeof(float));
#endif

#ifndef TAN_NO_OPENCL
        auto ret = mConverter->Convert(
            mOutputMixCLBufs[0],
            1, 0,
            TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortCLBuf,
            2, 0,
            TAN_SAMPLE_TYPE_SHORT,
            sampleCount,
            1.f
            );
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);
        PrintCLArrayReduced("aft Convert[0]", mOutputShortCLBuf, mCmdQueue1, sampleCount * sizeof(short));

        ret = mConverter->Convert(
            mOutputMixCLBufs[1],
            1, 0,
            TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortCLBuf,
            2, 1,
            TAN_SAMPLE_TYPE_SHORT,
            sampleCount,
            1.f
            );
        AMF_RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);
        PrintCLArrayReduced("aft Convert[1]", mOutputShortCLBuf, mCmdQueue1, sampleCount * sizeof(short));

#else
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
        PrintAMFArrayReduced("aft Convert[0]", mOutputShortAMFBuffer, mCompute1, sampleCount * sizeof(short));

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
        PrintAMFArrayReduced("aft Convert[1]", mOutputShortAMFBuffer, mCompute1, sampleCount * sizeof(short));
#endif

#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueReadBuffer(
                mTANConvolutionContext->GetConvQueue(),
                mOutputShortCLBuf,
                CL_TRUE,
                0,
                sampleCountBytes,
                pOut,
                NULL,
                NULL,
                NULL
                )
            );
#else
        AMF_RETURN_IF_FAILED(
            mTANConvolutionContext->GetConvQueue()->CopyBufferToHost(
                mOutputShortAMFBuffer,
                0,
                sampleCountBytes,
                pOut,
                true
                )
            );
#endif
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

        PrintArrayReduced("aft Prc[0]", mOutputFloatBufs[0], mFFTLength * sizeof(float));
        PrintArrayReduced("aft Prc[1]", mOutputFloatBufs[1], mFFTLength * sizeof(float));

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
        while (n >= 8 && mUseIntrinsics){
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

    PrintArrayReduced("::Process, out[0]", pOut, sampleCount * sizeof(float));
    PrintArrayReduced("::Process, out[1]", pOut + 1, sampleCount * sizeof(float));

    static int counter(0);

    if(!counter)
    {
        mUpdateParams = true;
    }

    auto info = ES + "Process " + std::to_string(counter) + " ===============================================";

    PrintDebug(info);
    ++counter;

    if(counter == 3)
    {
        int i = 0;
        ++i;
        assert(false);
    }

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
    size_t actualRestartsNumber = 0;

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
            //todo: this is not correct for common case, add size calculation
            bytes2Play = mBufferSizeInBytes;

            Process(&outputBuffer.front(), pWaves, mBufferSizeInBytes);
        }

        std::memcpy(processed, &outputBuffer.front(), bytes2Play);

        //continue;
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        std::copy_n(outputBuffer.data(), bytes2Play, processed);

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

                //processed

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
            }
            else
            {
                //play wav again
                pWaves[fileIndex] = pWaveStarts[fileIndex];
                waveBytesPlayed[fileIndex] = 0;

                actualRestartsNumber++;
            }
        }

        if (actualRestartsNumber == 0 && mShouldOutputRecording && !mOutputFile.empty())
        {
            WriteWaveFileS(mOutputFile.c_str(), FILTER_SAMPLE_RATE, STEREO_CHANNELS_COUNT,
                16, mMaxSamplesCount, &mStereoProcessedBuffer.front());
        }

        if (mRestartsNumber > 0 && actualRestartsNumber == mRestartsNumber)
        {
            saveRestartCount(actualRestartsNumber);
            Stop();
            break;
        }
        else if(processed - &mStereoProcessedBuffer.front() + (mBufferSizeInBytes / sizeof(int16_t)) > mMaxSamplesCount)
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
#ifndef TAN_NO_OPENCL
                AMF_RETURN_IF_FAILED(
                    mTrueAudioVR->generateRoomResponse(
                        mRoom,
                        mSources[idx],
                        mEars,
                        FILTER_SAMPLE_RATE,
                        mFFTLength,
                        mOCLResponses[idx * 2],
                        mOCLResponses[idx * 2 + 1],
                        GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM,
                        50
                        )
                    );
#else
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
#endif
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
#ifndef TAN_NO_OPENCL
                ret = mConvolution->UpdateResponseTD(mOCLResponses, mFFTLength, nullptr, TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY);
#else
                ret = mConvolution->UpdateResponseTD(
                    mAMFResponsesInterfaces,
                    mFFTLength,
                    nullptr,
                    TAN_CONVOLUTION_OPERATION_FLAG::TAN_CONVOLUTION_OPERATION_FLAG_BLOCK_UNTIL_READY
                    );
#endif
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
