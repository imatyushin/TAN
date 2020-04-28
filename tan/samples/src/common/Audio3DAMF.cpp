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

AMF_RESULT Audio3DAMF::Init
(
	const std::string &     dllPath,
	const RoomDefinition &  roomDef,

    const std::vector<std::string> &
                            inFiles,

    bool                    useMicSource,
    const std::vector<bool> &
                            trackHeadPos,

	int                     fftLen,
	int                     bufferSizeInSamples,

    bool                    useAMFConvolution,
    bool                    useGPUConvolution,
    int                     deviceIndexConvolution,

#ifdef RTQ_ENABLED
	bool                    useHPr_Conv,
    bool                    useRTQ_Conv,
    int                     cuRes_Conv,
#endif

    bool                    useAMFRoom,
    bool                    useGPURoom,
    int                     deviceIndexRoom,

#ifdef RTQ_ENABLED
	bool                    useHPr_IRGen,
    bool                    useRTQ_IRGen,
    int                     cuRes_IRGen,
#endif

    amf::TAN_CONVOLUTION_METHOD
                            convMethod,

    const std::string &     playerType,

    RoomUpdateMode          roomUpdateMode
)
{
    if((useGPUConvolution && !useAMFConvolution) || (useGPURoom && !useAMFRoom))
    {
        std::cerr
            << "Error: GPU queues must be used only if OpenCL flag is set"
            << std::endl;

        return AMF_INVALID_ARG;
    }

    //m_useOCLOutputPipeline = useGPU_Conv && useGPU_IRGen;
    //m_useOCLOutputPipeline = useAMFConvolution || useAMFRoom;

    mSrc1EnableMic = useMicSource;
    mTrackHeadPos = trackHeadPos;
    mBufferSizeInSamples = bufferSizeInSamples;
    mBufferSizeInBytes = mBufferSizeInSamples * STEREO_CHANNELS_COUNT * sizeof(int16_t);

    mWavFiles.reserve(inFiles.size());

    for(const auto& fileName: inFiles)
    {
        WavContent content = {0};

        if(content.ReadWaveFile(fileName))
        {
            if(content.SamplesPerSecond != FILTER_SAMPLE_RATE)
            {
                mLastError = std::string()
                    + "Error: file " + fileName + " has an unsupported frequency! Currently only "
                    + std::to_string(FILTER_SAMPLE_RATE) + " frequency is supported!";

                return AMF_FAIL;
            }

            bool converted = content.Convert2Stereo16Bit();

            if(!converted && content.BitsPerSample != 16)
            {
                mLastError = std::string()
                    + "Error: file " + fileName + " has an unsupported bits per sample count. Currently only "
                    + std::to_string(16) + " bits is supported!";

                return AMF_FAIL;
            }
            else if(!converted && content.ChannelsCount != STEREO_CHANNELS_COUNT)
            {
                mLastError = std::string()
                    + "Error: file " + fileName + " is not a stereo file. Currently only stereo files are supported!";

                return AMF_FAIL;
            }

            if(content.SamplesCount < mBufferSizeInSamples)
            {
                mLastError = std::string()
                    + "Error: file " + fileName + " are too short (SamplesCount < single buffer size).";

                return AMF_FAIL;
            }

            //make mono
            content.JoinChannels();

            //check that samples have compatible formats
            //becouse convertions are not yet implemented
            if(mWavFiles.size())
            {
                if(!mWavFiles[0].IsSameFormat(content))
                {
                    mLastError = std::string()
                        + "Error: file " + fileName + " has a diffrent format with other opened files.";

                    return AMF_FAIL;
                }
            }

            mWavFiles.push_back(content);
        }
        else
        {
            mLastError = std::string() + "Error: could not load WAV data from file " + fileName;

            return AMF_FAIL;
        }
    }

    if(!mWavFiles.size())
    {
        std::cerr << "Error: no files opened to play" << std::endl;

        return AMF_FAIL;
    }

    //initialize hardware
    mPlayer.reset(
#if defined(__MACOSX) || defined(__APPLE__)
        static_cast<IWavPlayer *>(new PortPlayer())
#else

#ifdef ENABLE_PORTAUDIO
        playerType == "PortAudio"
            ? static_cast<IWavPlayer *>(new PortPlayer())
            :
#ifdef _WIN32
                static_cast<IWavPlayer *>(new WASAPIPlayer())
#elif !defined(__MACOSX) && !defined(__APPLE__)
                static_cast<IWavPlayer *>(new AlsaPlayer())
#endif

#else

#ifdef _WIN32
        new WASAPIPlayer()
#elif !defined(__MACOSX) && !defined(__APPLE__)
        new AlsaPlayer()
#endif

#endif

#endif
        );

    //assume that all opened files has the same format
    //and we have at least one opened file
    auto openStatus = mPlayer->Init(
        mWavFiles[0].ChannelsCount,
        mWavFiles[0].BitsPerSample,
        mWavFiles[0].SamplesPerSecond,

        mWavFiles.size() > 0,
        useMicSource
        );

    if(PlayerError::OK != openStatus)
    {
        std::cerr << "Error: could not initialize player " << std::endl;

        return AMF_FAIL;
    }

    /* # fft buffer length must be power of 2: */
    mFFTLength = 1;
    while(mFFTLength < fftLen && (mFFTLength << 1) <= MAXRESPONSELENGTH)
    {
        mFFTLength <<= 1;
    }

    /*for(int i = 0; i < MAX_SOURCES; i++)
    {
        m_samplePos[i] = 0;
    }*/

    // allocate responses in one block
    // to optimize transfer to GPU
    //mResponseBuffer = new float[mWavFiles.size() * mFFTLength * STEREO_CHANNELS_COUNT];
    mResponseBuffer = mResponseBufferStorage.Allocate(mWavFiles.size() * mFFTLength * STEREO_CHANNELS_COUNT);

    //todo: use std::align(32, sizeof(__m256), out2, space)...
    for(int idx = 0; idx < mWavFiles.size() * 2; idx++)
    {
        mResponses[idx] = mResponseBuffer + idx * mFFTLength;

        mInputFloatBufs[idx] = mInputFloatBufsStorage[idx].Allocate(mFFTLength);
        mOutputFloatBufs[idx] = mOutputFloatBufsStorage[idx].Allocate(mFFTLength);
    }

    for (int i = 0; i < mWavFiles.size() * STEREO_CHANNELS_COUNT; i++)
    {
        memset(mResponses[i], 0, sizeof(float)*mFFTLength);
        memset(mInputFloatBufs[i], 0, sizeof(float)*mFFTLength);
        memset(mOutputFloatBufs[i], 0, sizeof(float)*mFFTLength);
    }

    for (int i = 0; i < STEREO_CHANNELS_COUNT; i++)//Right and left channel after mixing
    {
        mOutputMixFloatBufs[i] = mOutputMixFloatBufsStorage[i].Allocate(mFFTLength);
        memset(mOutputMixFloatBufs[i], 0, sizeof(float)*mFFTLength);
    }

    memset(&room, 0, sizeof(room));

    room = roomDef;

    for (int idx = 0; idx < mWavFiles.size(); idx++)
    {
        sources[idx].speakerX = 0.0;
        sources[idx].speakerY = 0.0;
        sources[idx].speakerZ = 0.0;
    }

    ears.earSpacing = float(0.16);
    ears.headX = 0.0;
    ears.headZ = 0.0;
    ears.headY = 1.75;
    ears.pitch = 0.0;
    ears.roll = 0.0;
    ears.yaw = 0.0;

    {
        mMaxSamplesCount = mWavFiles[0].SamplesCount;

        for(int file = 1; file < mWavFiles.size(); ++file)
        {
            mMaxSamplesCount = std::max(
                mWavFiles[file].SamplesCount,
                mMaxSamplesCount
                );
        }

        mStereoProcessedBuffer.resize(
            STEREO_CHANNELS_COUNT
            *
            mMaxSamplesCount
            );
    }

    //
    auto factory = g_AMFFactory.GetFactory();

    AMF_RETURN_IF_FAILED(TANCreateContext(TAN_FULL_VERSION, &mTANConvolutionContext, factory), L"TANCreateContext mTANConvolutionContext failed");
    AMF_RETURN_IF_FAILED(TANCreateContext(TAN_FULL_VERSION, &mTANRoomContext, factory), L"TANCreateContext mTANRoomContext failed");

    // Allocate RT-Queues
    {
        int32_t flagsQ1 = 0;
        int32_t flagsQ2 = 0;

        //CL convolution on GPU
        if(useAMFConvolution && useGPUConvolution)
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
            //CreateGpuCommandQueues(deviceIndexConvolution, flagsQ1, &mCompute1, flagsQ2, &mCompute2, &mContext12);
            amf::AMFCompute *compute1(nullptr), *compute2(nullptr);
            CreateGpuCommandQueues(deviceIndexConvolution, flagsQ1, &compute1, flagsQ2, &compute2, &mContext12);
            mCompute1 = compute1;
            mCompute2 = compute2;

            //CL room on GPU
            if(useAMFRoom && useGPURoom && (deviceIndexConvolution == deviceIndexRoom))
            {
                mContext3 = mContext12;
                mCompute3 = mCompute2;
            }
        }

        //CL convolution on CPU
        else if(useAMFConvolution && !useGPUConvolution)
        {
#ifdef RTQ_ENABLED
            THROW_NOT_IMPLEMENTED;

            // For " core "reservation" on CPU" -ToDo test and enable
            if (cuRes_Conv > 0 && cuRes_IRGen > 0)
            {
                cl_int err = CreateCommandQueuesWithCUcount(deviceIndexConvolution, &mCmdQueue1, &mCmdQueue2, cuRes_Conv, cuRes_IRGen);
            }
            else
            {
#endif
                CreateCpuCommandQueues(deviceIndexConvolution, 0, &mCompute1, 0, &mCompute2, &mContext12);

#ifdef RTQ_ENABLED
            }
#endif

            //CL room on CPU
            if(useAMFRoom && !useGPURoom && (deviceIndexConvolution == deviceIndexRoom))
            {
                mContext3 = mContext12;
                mCompute3 = mCompute2;
            }
        }

        //room queue not yet created
        if(!mCompute3 && useAMFRoom)
        {
            //CL over GPU
            if(useGPURoom)
            {
                CreateGpuCommandQueues(deviceIndexRoom, 0, &mCompute3, 0, nullptr, &mContext3);
            }
            //CL over CPU
            else
            {
                CreateCpuCommandQueues(deviceIndexRoom, 0, &mCompute3, 0, nullptr, &mContext3);
            }
        }
    }

    //convolution over OpenCL
    if(useAMFConvolution)
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

    //room processing over OpenCL
    if(useAMFRoom)
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

    if(useAMFConvolution)
    {
        AMF_RETURN_IF_FAILED(
            mConvolution->InitGpu(
                convMethod,
                mFFTLength,
                mBufferSizeInSamples,
                mWavFiles.size() * STEREO_CHANNELS_COUNT
                )
            );
    }
    //todo, ivm:, investigate
    else //if(useGPUConvolution)
    {
        AMF_RETURN_IF_FAILED(
            mConvolution->InitCpu(
                convMethod,
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
    if(useAMFConvolution && useAMFRoom)
    {
        if(mContext12 == mContext3)
        {
            for(int i = 0; i < mWavFiles.size() * 2; i++)
            {
                AMF_RETURN_IF_FAILED(
                    mContext12->AllocBuffer(
                        amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                        mFFTLength * sizeof(float),
                        &mAMFResponses[i]
                        )
                    );

                mAMFResponsesInterfaces[i] = mAMFResponses[i];
            }

            //HACK out for test
            mUseAMFBuffers = true;
        }

        // Initialize CL output buffers

        // First create a big cl_mem buffer then create small sub-buffers from it
        AMF_RETURN_IF_FAILED(
            mContext12->AllocBuffer(
                amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                mBufferSizeInBytes * mWavFiles.size() * STEREO_CHANNELS_COUNT,
                &mOutputMainAMFBuffer
                ),
                L"Could not create OpenCL buffer"
                );

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
                    amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                    mBufferSizeInBytes,
                    &mOutputMixAMFBuffers[idx]
                    ),
                    L"Could not create OpenCL buffer"
                    );

            mOutputMixAMFBuffersInterfaces[idx] = mOutputMixAMFBuffers[idx];
        }

        // The output short buffer stores the final (after mixing) left and right channels interleaved as short samples
        // The short buffer size is equal to sizeof(short)*2*m_bufSize/sizeof(float) which is equal to m_bufSize
        AMF_RETURN_IF_FAILED(
            mContext12->AllocBuffer(
                amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL,
                mBufferSizeInBytes,
                &mOutputShortAMFBuffer
                ),
                L"Could not create OpenCL buffer"
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

    if(useGPURoom)
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
        << "Room: " << room.width<< "fm W x " << room.length << "fm L x " << room.height << "fm H"
        << std::endl;

    // head model:
    mTrueAudioVR->generateSimpleHeadRelatedTransform(
        ears.hrtf,
        ears.earSpacing
        );

    //To Do use gpu mem responses
    for (int idx = 0; idx < mWavFiles.size(); idx++)
    {
        if(mUseAMFBuffers)
        {
            mTrueAudioVR->generateRoomResponse(
                room,
                sources[idx],
                ears,
                FILTER_SAMPLE_RATE,
                mFFTLength,
                mAMFResponses[idx * 2],
                mAMFResponses[idx * 2 + 1],
                GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM,
                50
                );
        }
        else
        {
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

    if(mUseAMFBuffers)
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

int Audio3DAMF::Process(int16_t *pOut, int16_t *pChan[MAX_SOURCES], uint32_t sampleCountBytes)
{
    uint32_t sampleCount = sampleCountBytes / (sizeof(int16_t) * STEREO_CHANNELS_COUNT);

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

            PrintFloatArray("::Process, after Convert", mInputFloatBufs[idx * 2 + chan], sampleCount * sizeof(float));
        }
    }


    if(mUseAMFBuffers)
    {
        /**/
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

        static int i1 = 0;

        //std::cout << "cycle " << i1 << std::endl;
        if(3 == ++i1)
        {
            //return -1;
        }

        AMFBuffer *outputAMFBufferLeft[MAX_SOURCES] = {nullptr};
        AMFBuffer *outputAMFBufferRight[MAX_SOURCES] = {nullptr};

        for(int src = 0; src < MAX_SOURCES; src++)
        {
            outputAMFBufferLeft[src] = mOutputAMFBuffers[src * 2];// Even indexed channels for left ear input
            outputAMFBufferRight[src] = mOutputAMFBuffers[src * 2 + 1];// Odd indexed channels for right ear input
        }

        AMF_RETURN_IF_FAILED(mMixer->Mix((AMFBuffer **)outputAMFBufferLeft, mOutputMixAMFBuffersInterfaces[0]));

        AMF_RETURN_IF_FAILED(mMixer->Mix((AMFBuffer **)outputAMFBufferRight, mOutputMixAMFBuffersInterfaces[1]));

        auto amfResult = mConverter->Convert(
            mOutputMixAMFBuffers[0],
            1,
            0,
            TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortAMFBuffer,
            2,
            0,
            TAN_SAMPLE_TYPE_SHORT,
            sampleCount,
            1.f
            );
        AMF_RETURN_IF_FALSE(amfResult == AMF_OK || amfResult == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);

        amfResult = mConverter->Convert(
            mOutputMixAMFBuffers[1],
            1,
            0,
            TAN_SAMPLE_TYPE_FLOAT,
            mOutputShortAMFBuffer,
            2,
            1,
            TAN_SAMPLE_TYPE_SHORT,
            sampleCount,
            1.f
            );
        AMF_RETURN_IF_FALSE(amfResult == AMF_OK || amfResult == AMF_TAN_CLIPPING_WAS_REQUIRED, AMF_FAIL);

        AMF_RETURN_IF_FAILED(
            //mCompute1->CopyBufferToHost(
            mCompute2->CopyBufferToHost(
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

        AMF_RETURN_IF_FAILED(ret);

        float * outputFloatBufLeft[MAX_SOURCES];
        float * outputFloatBufRight[MAX_SOURCES];

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

    return 0;
}

unsigned Audio3DAMF::processThreadProc(void * ptr)
{
    Audio3DAMF *pAudio3D = static_cast<Audio3DAMF*>(ptr);

    return pAudio3D->ProcessProc();
}

int Audio3DAMF::ProcessProc()
{
    //uint32_t bytesRecorded(0);

    std::array<uint8_t, STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * sizeof(int16_t)> recordBuffer;
    std::array<uint8_t, STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * sizeof(int16_t)> extractBuffer;
    std::array<int16_t, STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE> outputBuffer;

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
                sources[idx].speakerX = ears.headX;
                sources[idx].speakerY = ears.headY;
                sources[idx].speakerZ = ears.headZ;
            }

            if(mUseAMFBuffers)
            {
                mTrueAudioVR->generateRoomResponse(
                    room,
                    sources[idx],
                    ears,
                    FILTER_SAMPLE_RATE,
                    mFFTLength,
                    mAMFResponsesInterfaces[idx * 2],
                    mAMFResponsesInterfaces[idx * 2 + 1],
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
            if(mUseAMFBuffers)
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
