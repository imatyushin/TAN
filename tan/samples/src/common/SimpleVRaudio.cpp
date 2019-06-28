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
//

#include <time.h>
#include <stdio.h>
#include "CL/cl.h"
#include "SimpleVRaudio.h"
#include "GpuUtils.h"
#include "../TrueAudioVR/TrueAudioVR.h"
#include "../common/AlsaPlayer.h"
#include "cpucaps.h"
#include "Utilities.h"

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

bool Audio3D::useIntrinsics = InstructionSet::AVX() && InstructionSet::FMA();

#ifndef ERROR_MESSAGE

#ifdef _WIN32
#define ERROR_MESSAGE(message) ::MessageBoxA(0, #message, "Error", MB_OK)
#else
#define ERROR_MESSAGE(message) __asm__("int3"); std::cerr << "Error: " << message << std::endl
#endif

#endif

#define RETURN_IF_FAILED(x) \
{ \
    AMF_RESULT tmp = (x); \
    if (tmp != AMF_OK) { \
        ERROR_MESSAGE(int(x)); \
        return -1; \
    } \
}

#define RETURN_IF_FALSE(x) \
{ \
    bool tmp = (x); \
    if (!tmp) { \
        ERROR_MESSAGE(int(x)); \
        return -1; \
    } \
}

#define SAFE_DELETE_ARR(x) if (x) { delete[] x; x = nullptr; }

transRotMtx::transRotMtx(){
    memset(m, 0, sizeof(m));
    m[0][0] = 1.0;
    m[1][1] = 1.0;
    m[2][2] = 1.0;
}

void transRotMtx::setAngles(float yaw, float pitch, float roll)
{
    float sinY = std::sin(yaw * (float)PI / 180);
    float cosY = std::cos(yaw * (float)PI / 180);
    float sinP = std::sin(pitch * (float)PI / 180);
    float cosP = std::cos(pitch * (float)PI / 180);
    float sinR = std::sin(roll * (float)PI / 180);
    float cosR = std::cos(roll * (float)PI / 180);

    m[0][0] = cosR*cosY - sinR*sinP*sinY;
    m[0][1] = -sinR*cosP;
    m[0][2] = cosR*sinY + sinR*sinP*cosY;
    m[1][0] = sinR*cosY + cosR*sinP*sinY;
    m[1][1] = cosR*cosP;
    m[1][2] = sinR*sinY - cosR*sinP*cosY;
    m[2][0] = -cosP*sinY;
    m[2][1] = sinP;
    m[2][2] = cosP*cosY;
}

void transRotMtx::setOffset(float x, float y, float z){
    m[0][3] = x;
    m[1][3] = y;
    m[2][3] = z;
}

void transRotMtx::transform(float &X, float &Y, float &Z)
{
    float x = X;
    float y = Y;
    float z = Z;
    X = x*m[0][0] + y*m[0][1] + z*m[0][2] + m[0][3];
    Y = x*m[1][0] + y*m[1][1] + z*m[1][2] + m[1][3];
    Z = x*m[2][0] + y*m[2][1] + z*m[2][2] + m[2][3];
}


unsigned Audio3D::processThreadProc(void * ptr)
{
    Audio3D *pAudio3D = static_cast<Audio3D*>(ptr);

    return pAudio3D->ProcessProc();
}

unsigned Audio3D::updateThreadProc(void * ptr)
{
    Audio3D *pAudio3D = static_cast<Audio3D*>(ptr);

    return pAudio3D->UpdateProc();
}

Audio3D::Audio3D():
    m_pTAVR(nullptr),
    mProcessThread(true),
    mRunning(false),
    mStop(false),
    m_headingOffset(0.),
    m_headingCCW(true),
    mPlayer(
#ifdef _WIN32
        new WASAPIUtils()
#else
        new AlsaPlayer()
#endif
    )
{
    responseBuffer = NULL;
    std::memset(responses, 0, sizeof(responses));
    std::memset(inputFloatBufs, 0, sizeof(inputFloatBufs));
    std::memset(outputFloatBufs, 0, sizeof(outputFloatBufs));
    std::memset(outputMixFloatBufs, 0, sizeof(outputMixFloatBufs));
    std::memset(m_samplePos, 0, sizeof(m_samplePos));

    for (int i = 0; i < MAX_SOURCES * 2; i++){
        oclResponses[i] = NULL;
        outputCLBufs[i] = NULL;
    }
    outputMainCLbuf = NULL;
    m_useClMemBufs = false;
    for (int i = 0; i < 2; i++)
    {
        outputMixCLBufs[i] = NULL;
        outputShortBuf = NULL;
    }
    mUpdated = false;
}

Audio3D::~Audio3D()
{
    Close();
}

int Audio3D::Close()
{
    mRunning = false;

    // destroy dumb pointer:
    if (m_pTAVR != NULL) {
        delete m_pTAVR;
        m_pTAVR = nullptr;
    }

    SAFE_DELETE_ARR(responseBuffer);
    for (int i = 0; i < MAX_SOURCES*2; i++){
        SAFE_DELETE_ARR(inputFloatBufs[i]);
        SAFE_DELETE_ARR(outputFloatBufs[i]);

        if (oclResponses[i] == NULL) continue;
        clReleaseMemObject(oclResponses[i]);
        oclResponses[i] = NULL;
    }

    for (int i = 0; i < mWavFiles.size() * 2; i++)
    {
        if (outputCLBufs[i] == NULL) continue;
        clReleaseMemObject(outputCLBufs[i]);
        outputCLBufs[i] = NULL;
    }

    if (outputMainCLbuf != NULL)
    {
        clReleaseMemObject(outputMainCLbuf);
        outputMainCLbuf = NULL;
    }
    for (int i = 0; i < 2; i++)
    {
        if (outputMixCLBufs[i] == NULL) continue;
        clReleaseMemObject(outputMixCLBufs[i]);
        outputMixCLBufs[i] = NULL;
    }
    if (outputShortBuf)
    {
        clReleaseMemObject(outputShortBuf);
        outputShortBuf = NULL;
    }
    m_useClMemBufs = false;

    // release smart pointers:
    m_spFft.Release();
    m_spConvolution.Release();
    m_spConverter.Release();
    m_spMixer.Release();
    m_spTANContext2.Release();
    m_spTANContext1.Release();

    if (mCmdQueue1 != NULL){
        clReleaseCommandQueue(mCmdQueue1);
    }
    if (mCmdQueue2 != NULL){
        clReleaseCommandQueue(mCmdQueue2);
    }
    if (mCmdQueue3 != NULL && mCmdQueue3 != mCmdQueue2){
        clReleaseCommandQueue(mCmdQueue3);
    }

    mCmdQueue1 = NULL;
    mCmdQueue2 = NULL;
    mCmdQueue3 = NULL;

    mWavFiles.resize(0);
    mPlayer->Close();

    return 0;
}

int Audio3D::Init
(
	const std::string &     dllPath,
	const RoomDefinition &  roomDef,

    const std::vector<std::string> &
                            inFiles,

    bool                    useMicSource,
    bool                    trackHeadPos,

	int                     fftLen,
	int                     bufferSizeInSamples,

    bool                    useGPU_Conv,
    int                     devIdx_Conv,
#ifdef RTQ_ENABLED
	bool                    useHPr_Conv,
    bool                    useRTQ_Conv,
    int                     cuRes_Conv,
#endif // RTQ_ENABLED
    bool                    useGPU_IRGen,
    int                     devIdx_IRGen,
#ifdef RTQ_ENABLED
	bool                    useHPr_IRGen,
    bool                    useRTQ_IRGen,
    int                     cuRes_IRGen,
#endif // RTQ_ENABLED
    amf::TAN_CONVOLUTION_METHOD
                            convMethod,
    bool                    useCPU_Conv,
    bool                    useCPU_IRGen
)
{
    //useCPU_Conv = true;
    Close();

    // shouldn't need this, they are radio buttons:
    if (useGPU_Conv) useCPU_Conv = false;
    if (useGPU_IRGen) useCPU_IRGen = false;

    //m_useCpuConv = useCPU_Conv;
    //m_useCpuIRGen = useCPU_IRGen;
    //m_useGpuConv = useGPU_Conv;
	//m_useGpuIRGen = useGPU_IRGen;

    mWavFiles.reserve(inFiles.size());

    for(const auto& fileName: inFiles)
    {
        WavContent content;

        if(content.ReadWaveFile(fileName))
        {
            if(content.SamplesPerSecond != FILTER_SAMPLE_RATE)
            {
                std::cerr
                    << "Error: file " << fileName << " has an unsupported frequency! Currently only "
                    << FILTER_SAMPLE_RATE << " frequency is supported!" << std::endl;

                return -1;
            }

            if(content.BitsPerSample != 16)
            {
                std::cerr
                    << "Error: file " << fileName << " has an unsupported bits per sample count. Currently only "
                    << 16 << " bits is supported!" << std::endl;

                return -1;
            }

            //check that samples have compatible formats
            //becouse convertions are not yet implemented
            if(mWavFiles.size())
            {
                if(!mWavFiles[0].IsSameFormat(content))
                {
                    std::cerr << "Error: file " << fileName << " has a diffrent format with opened files" << std::endl;

                    return -1;
                }
            }

            mWavFiles.push_back(content);
        }
        else
        {
            std::cerr << "Error: could not load WAV data from file " << fileName << std::endl;

            return -1;
        }
    }

    if(!mWavFiles.size())
    {
        std::cerr << "Error: no files opened to play" << std::endl;

        return -1;
    }

    /*std::cout << "WAV:" << std::endl;
    std::cout << mWavFiles[0].SamplesCount << std::endl;
    std::cout << mWavFiles[0].Data.size() << std::endl;

    for(int s = 0; s < mWavFiles[0].SamplesCount; ++s)
    {
        std::cout << std::hex
            << s << ":"
            << " 0x" << unsigned(mWavFiles[0].Data[s * 4 + 0])
            << " 0x" << unsigned(mWavFiles[0].Data[s * 4 + 1])
            << " 0x" << unsigned(mWavFiles[0].Data[s * 4 + 2])
            << " 0x" << unsigned(mWavFiles[0].Data[s * 4 + 3])
            << std::endl;
    }*/

    //initialize hardware
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

        return -1;
    }

    mSrc1EnableMic = useMicSource;
    mSrc1TrackHeadPos = trackHeadPos;

    mBufferSizeInSamples = bufferSizeInSamples;
    //mBufferSizeInBytes = STEREO_CHANNELS_COUNT * bufferSizeInSamples * (mWavFiles[0].BitsPerSample / 8);
    mBufferSizeInBytes = mBufferSizeInSamples * STEREO_CHANNELS_COUNT * sizeof(int16_t);

    /* # fft buffer length must be power of 2: */
    m_fftLen = 1;
    while((int)m_fftLen < fftLen && (m_fftLen << 1) <= MAXRESPONSELENGTH)
    {
        m_fftLen <<= 1;
    }

    for(int i = 0; i < MAX_SOURCES; i++)
    {
        m_samplePos[i] = 0;
    }

    // allocate responses in one block
    // to optimize transfer to GPU
    responseBuffer = new float[m_fftLen * mWavFiles.size() * 2];

    for(int idx = 0; idx < mWavFiles.size() * 2; idx++)
    {
        responses[idx] = responseBuffer + idx*m_fftLen;
        inputFloatBufs[idx] = new float[m_fftLen];
        outputFloatBufs[idx] = new float[m_fftLen];
    }

    for (int i = 0; i < mWavFiles.size() * 2; i++)
    {
        memset(responses[i], 0, sizeof(float)*m_fftLen);
        memset(inputFloatBufs[i], 0, sizeof(float)*m_fftLen);
        memset(outputFloatBufs[i], 0, sizeof(float)*m_fftLen);
    }

    for (int i = 0; i < 2; i++)//Right and left channel after mixing
    {
        outputMixFloatBufs[i] = new float[m_fftLen];
        memset(outputMixFloatBufs[i], 0, sizeof(float)*m_fftLen);
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
            2 //stereo
            *
            mMaxSamplesCount
            //*
            //sizeof(int16_t)
            );
    }

    //// Allocate RT-Queues
    {
        mCmdQueue1 = mCmdQueue2 = mCmdQueue3 = nullptr;
        unsigned int refCountQ1 = 0;
        unsigned int refCountQ2 = 0;

        int32_t flagsQ1 = 0;
        int32_t flagsQ2 = 0;

        //if (useGPU_Conv || useGPU_IRGen)
        if(useGPU_Conv || useCPU_Conv )
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

            CreateGpuCommandQueues(devIdx_Conv, flagsQ1, &mCmdQueue1, flagsQ2, &mCmdQueue2);

            if(devIdx_Conv == devIdx_IRGen && useGPU_IRGen)
            {
                mCmdQueue3 = mCmdQueue2;
            }
        }
        else if(useCPU_Conv)
        {
    #ifdef RTQ_ENABLED
            // For " core "reservation" on CPU" -ToDo test and enable
            if (cuRes_Conv > 0 && cuRes_IRGen > 0)
            {
                cl_int err = CreateCommandQueuesWithCUcount(devIdx_Conv, &mCmdQueue1, &mCmdQueue2, cuRes_Conv, cuRes_IRGen);
            }
            else
            {
                CreateCpuCommandQueues(devIdx_Conv, 0, &mCmdQueue1, 0, &mCmdQueue2);
            }
    #endif // RTQ_ENABLED

            if (devIdx_Conv == devIdx_IRGen && useCPU_IRGen)
            {
                mCmdQueue3 = mCmdQueue2;
            }
        }

        if(mCmdQueue3 == NULL)
        {
            if (useGPU_IRGen)
            {
                CreateGpuCommandQueues(devIdx_IRGen, 0, &mCmdQueue3, 0, nullptr);
            }
            else if (useCPU_IRGen)
            {
                CreateCpuCommandQueues(devIdx_IRGen, 0, &mCmdQueue3, 0, nullptr);
            }
        }
    }

    // Create TAN objects.
    RETURN_IF_FAILED(TANCreateContext(TAN_FULL_VERSION, &m_spTANContext1));

    if (useGPU_Conv || useCPU_Conv) {
        RETURN_IF_FAILED(m_spTANContext1->InitOpenCL(mCmdQueue1, mCmdQueue2));
    }

    RETURN_IF_FAILED(TANCreateContext(TAN_FULL_VERSION, &m_spTANContext2));
    if (useGPU_IRGen || useCPU_IRGen) {
        RETURN_IF_FAILED(m_spTANContext2->InitOpenCL(mCmdQueue3, mCmdQueue3));
    }

    RETURN_IF_FAILED(TANCreateConvolution(m_spTANContext1, &m_spConvolution));
    if (!useGPU_Conv && !useCPU_Conv) // don't use OpenCL at all
    {
        // ensures compatible with old room acoustic
		if(convMethod != TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD)
        {
            convMethod = TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD;
        }

        // C_Model implementation
        RETURN_IF_FAILED(
            m_spConvolution->InitCpu(
                convMethod,
                m_fftLen,
                mBufferSizeInSamples,
                mWavFiles.size() * STEREO_CHANNELS_COUNT
                )
            );
    }
    else
    {
        RETURN_IF_FAILED(
            m_spConvolution->InitGpu(
                //TAN_CONVOLUTION_METHOD_FHT_UNIFORM_HEAD_TAIL,
                convMethod,
                m_fftLen,
                mBufferSizeInSamples,
                mWavFiles.size() * STEREO_CHANNELS_COUNT
                )
            );
    }

    RETURN_IF_FAILED(TANCreateConverter(m_spTANContext2, &m_spConverter));
    RETURN_IF_FAILED(m_spConverter->Init());

    RETURN_IF_FAILED(TANCreateMixer(m_spTANContext2, &m_spMixer));
	RETURN_IF_FAILED(m_spMixer->Init(bufferSizeInSamples, mWavFiles.size()));

    RETURN_IF_FAILED(TANCreateFFT(m_spTANContext2, &m_spFft));
    RETURN_IF_FAILED(m_spFft->Init());

    if (m_pTAVR != NULL) {
        delete(m_pTAVR);
        m_pTAVR = NULL;
    }

    m_useOCLOutputPipeline = useGPU_Conv && useGPU_IRGen;

    if (useGPU_IRGen && useGPU_Conv)
    {
        //if (mCmdQueue3 == NULL){
        //    CreateGpuCommandQueues(devIdx_IRGen, 0, &mCmdQueue3, 0, NULL);
        //}

        cl_int status;
        cl_context context_IR;
        cl_context context_Conv;

        clGetCommandQueueInfo(mCmdQueue3, CL_QUEUE_CONTEXT, sizeof(cl_context), &context_IR, NULL);
        clGetCommandQueueInfo(mCmdQueue2, CL_QUEUE_CONTEXT, sizeof(cl_context), &context_Conv, NULL);
        if (context_IR == context_Conv) {
            for (int i = 0; i < mWavFiles.size() * 2; i++){
                oclResponses[i] = clCreateBuffer(context_IR, CL_MEM_READ_WRITE, m_fftLen * sizeof(float), NULL, &status);
            }
            //HACK out for test
            m_useClMemBufs = true;
        }
        // Initialize CL output buffers,
        if(useGPU_Conv)
        {
            cl_int clErr;

            // First create a big cl_mem buffer then create small sub-buffers from it
            outputMainCLbuf = clCreateBuffer(
                m_spTANContext1->GetOpenCLContext(),
                CL_MEM_READ_WRITE,
                mBufferSizeInSamples * mWavFiles.size() * STEREO_CHANNELS_COUNT,
                nullptr,
                &clErr
                );

            if (clErr != CL_SUCCESS)
            {
                printf("Could not create OpenCL buffer\n");
                return AMF_FAIL;
            };

            for(amf_uint32 i = 0; i < mWavFiles.size() * 2; i++)
            {
                cl_buffer_region region;
                region.origin = i * mBufferSizeInSamples;
                region.size = mBufferSizeInSamples;
                outputCLBufs[i] = clCreateSubBuffer(
                    outputMainCLbuf, CL_MEM_READ_WRITE, CL_BUFFER_CREATE_TYPE_REGION, &region, &clErr);

                if (clErr != CL_SUCCESS)
                {
                    printf("Could not create OpenCL subBuffer\n");
                    return AMF_FAIL;
                }

                float zero = 0.0;
                clErr = clEnqueueFillBuffer(mCmdQueue1, outputCLBufs[i], &zero,sizeof(zero), 0, region.size, 0, NULL, NULL);
                if (clErr != CL_SUCCESS)
                {
                    printf("Could not fill OpenCL subBuffer\n");
                    return AMF_FAIL;
                }
            }

            for (int idx = 0; idx < 2; idx++)
            {
                outputMixCLBufs[idx] = clCreateBuffer(
                    m_spTANContext1->GetOpenCLContext(),
                    CL_MEM_READ_WRITE,
                    mBufferSizeInBytes,
                    nullptr,
                    &clErr
                    );

                if (clErr != CL_SUCCESS)
                {
                    printf("Could not create OpenCL buffer\n");
                    return AMF_FAIL;
                }

                if (clErr != CL_SUCCESS)
                {

                    printf("Could not create OpenCL buffer\n");
                    return AMF_FAIL;
                }
            }

            // The output short buffer stores the final (after mixing) left and right channels interleaved as short samples
            // The short buffer size is equal to sizeof(short)*2*m_bufSize/sizeof(float) which is equal to m_bufSize
            outputShortBuf = clCreateBuffer(
                m_spTANContext1->GetOpenCLContext(),
                CL_MEM_READ_WRITE,
                mBufferSizeInBytes,
                nullptr,
                &clErr
                );
        }
    }

    #ifdef _WIN32
    HMODULE TanVrDll;
    TanVrDll = LoadLibraryA("TrueAudioVR.dll");
    typedef int  (WINAPI *CREATEVR)(AmdTrueAudioVR **taVR, TANContextPtr pContext, TANFFTPtr pFft, cl_command_queue cmdQueue, float samplesPerSecond, int convolutionLength);
    CREATEVR CreateAmdTrueAudioVR = nullptr;

    CreateAmdTrueAudioVR = (CREATEVR)GetProcAddress(TanVrDll, "CreateAmdTrueAudioVR");
    #endif

    CreateAmdTrueAudioVR(
        &m_pTAVR,
        m_spTANContext2,
        m_spFft,
        mCmdQueue3,
        48000, //todo: other frequencies?
        m_fftLen
        );

    if (useGPU_IRGen){
        m_pTAVR->SetExecutionMode(AmdTrueAudioVR::GPU);
    }
    else {
        m_pTAVR->SetExecutionMode(AmdTrueAudioVR::CPU);
    }

    std::cout << "Room: " << room.width<< "fm W x " << room.length << "fm L x " << room.height << "fm H" << std::endl;

    // head model:
    m_pTAVR->generateSimpleHeadRelatedTransform(&ears.hrtf, ears.earSpacing);

    //To Do use gpu mem responses
    for (int idx = 0; idx < mWavFiles.size(); idx++){
        if (m_useClMemBufs) {
            m_pTAVR->generateRoomResponse(room, sources[idx], ears, FILTER_SAMPLE_RATE, m_fftLen, oclResponses[idx * 2], oclResponses[idx * 2 + 1], GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM, 50);
        }
        else {
            m_pTAVR->generateRoomResponse(room, sources[idx], ears, FILTER_SAMPLE_RATE, m_fftLen, responses[idx * 2], responses[idx * 2 + 1], GENROOM_LIMIT_BOUNCES, 50);
        }
    }

    if (m_useClMemBufs) {
        RETURN_IF_FAILED(m_spConvolution->UpdateResponseTD(oclResponses, m_fftLen, nullptr, IR_UPDATE_MODE));
    }
    else {
        RETURN_IF_FAILED(m_spConvolution->UpdateResponseTD(responses, m_fftLen, nullptr, IR_UPDATE_MODE));
    }

    mRunning = true;

    return 0;
}

int Audio3D::updateHeadPosition(float x, float y, float z, float yaw, float pitch, float roll)
{
    // world to room coordinates transform:
    m_mtxWorldToRoomCoords.transform(x, y, z);
    yaw = m_headingOffset + (m_headingCCW ? yaw : -yaw);

    if (x == ears.headX && y == ears.headY && z == ears.headZ //) {
        && yaw == ears.yaw && pitch == ears.pitch && roll == ears.roll) {
            return 0;
    }

    ears.headX = x;
    ears.headY = y;
    ears.headZ = z;

    ears.yaw = yaw;
    ears.pitch = pitch;
    ears.roll = roll;

    mUpdateParams = true;
    return 0;
}

int Audio3D::updateSourcePosition(int srcNumber, float x, float y, float z)
{
    if (srcNumber >= mWavFiles.size()){
        return -1;
    }
    // world to room coordinates transform:
    m_mtxWorldToRoomCoords.transform(x, y, z);

    sources[srcNumber].speakerX = x;// +room.width / 2.0f;
    sources[srcNumber].speakerY = y;
    sources[srcNumber].speakerZ = z;// +room.length / 2.0f;

    mUpdateParams = true;
    return 0;
}

int Audio3D::updateRoomDimension(float _width, float _height, float _length)
{
	room.width = _width;
	room.height = _height;
	room.length = _length;

	mUpdateParams = true;
	return 0;
}

int Audio3D::updateRoomDamping(float _left, float _right, float _top, float _buttom, float _front, float _back)
{
	room.mTop.damp = _top;
	room.mBottom.damp = _buttom;
	room.mLeft.damp = _left;
	room.mRight.damp = _right;
	room.mFront.damp = _front;
	room.mBack.damp = _back;

	mUpdateParams = true;
	return 0;
}

AmdTrueAudioVR* Audio3D::getAMDTrueAudioVR()
{
	return m_pTAVR;
}

TANConverterPtr Audio3D::getTANConverter()
{
	return m_spConverter;
}

int Audio3D::setWorldToRoomCoordTransform(
    float translationX=0.,
    float translationY=0.,
    float translationZ=0.,

    float rotationY=0.,

    float headingOffset=0.,
    bool headingCCW=true
    )
{
    m_mtxWorldToRoomCoords.setOffset(translationX, translationY, translationZ);
    m_mtxWorldToRoomCoords.setAngles(rotationY, 0.0, 0.0);
    m_headingOffset = headingOffset;
    m_headingCCW = headingCCW;

    return 0;
}

int Audio3D::Run()
{
    mStop = false;
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
    return 0;
}

int Audio3D::Stop()
{
    mStop = true;

    mProcessThread.WaitCloseInfinite();
    mUpdateThread.WaitCloseInfinite();

    return 0;
}

int Audio3D::Process(int16_t *pOut, int16_t *pChan[MAX_SOURCES], uint32_t sampleCountBytes)
{
    uint32_t sampleCount = sampleCountBytes / (sizeof(int16_t) * STEREO_CHANNELS_COUNT);

    // Read from the files
    for (int idx = 0; idx < mWavFiles.size(); idx++) {
        for (int chan = 0; chan < 2; chan++){
            // The way sources in inputFloatBufs are ordered is: Even indexed elements for left channels, odd indexed ones for right,
            // this ordering matches with the way impulse responses are generated and indexed to be convolved with the sources.
            RETURN_IF_FAILED(
                m_spConverter->Convert(
                    pChan[idx] + chan,
                    2,
                    sampleCount,
                    inputFloatBufs[idx * 2 + chan],
                    1,
                    1.f
                    )
                );
        }
    }

    if (m_useOCLOutputPipeline)
    {   // OCL device memory objects are passed to the TANConvolution->Process method.
        // Mixing and short conversion is done on GPU.

        RETURN_IF_FAILED(m_spConvolution->Process(inputFloatBufs, outputCLBufs, sampleCount, nullptr, nullptr));

        cl_mem outputCLBufLeft[MAX_SOURCES];
        cl_mem outputCLBufRight[MAX_SOURCES];
        for (int src = 0; src < MAX_SOURCES; src++)
        {
            outputCLBufLeft[src] = outputCLBufs[src*2 ];// Even indexed channels for left ear input
            outputCLBufRight[src] = outputCLBufs[src*2+1];// Odd indexed channels for right ear input
        }
        AMF_RESULT ret = m_spMixer->Mix(outputCLBufLeft, outputMixCLBufs[0]);
        RETURN_IF_FALSE(ret == AMF_OK);

        ret = m_spMixer->Mix(outputCLBufRight, outputMixCLBufs[1]);
        RETURN_IF_FALSE(ret == AMF_OK);
        ret = m_spConverter->Convert(outputMixCLBufs[0], 1, 0, TAN_SAMPLE_TYPE_FLOAT,
            outputShortBuf, 2, 0, TAN_SAMPLE_TYPE_SHORT, sampleCount, 1.f);
        RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);

        ret = m_spConverter->Convert(outputMixCLBufs[1], 1, 0, TAN_SAMPLE_TYPE_FLOAT,
            outputShortBuf, 2, 1, TAN_SAMPLE_TYPE_SHORT, sampleCount, 1.f);
        RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);

        cl_int clErr = clEnqueueReadBuffer(m_spTANContext1->GetOpenCLGeneralQueue(), outputShortBuf, CL_TRUE,
             0, sampleCountBytes, pOut, NULL, NULL, NULL);
        RETURN_IF_FALSE(clErr == CL_SUCCESS);

    }
    else
    {   // Host memory pointers are passed to the TANConvolution->Process method
        // Mixing and short conversion are still performed on CPU.

        RETURN_IF_FAILED(m_spConvolution->Process(inputFloatBufs, outputFloatBufs, sampleCount,
            nullptr, nullptr));

        float * outputFloatBufLeft[MAX_SOURCES];
        float * outputFloatBufRight[MAX_SOURCES];
        for (int src = 0; src < MAX_SOURCES; src++)
        {
            outputFloatBufLeft[src] = outputFloatBufs[src * 2];// Even indexed channels for left ear input
            outputFloatBufRight[src] = outputFloatBufs[src * 2 + 1];// Odd indexed channels for right ear input
        }
        AMF_RESULT ret = m_spMixer->Mix(outputFloatBufLeft, outputMixFloatBufs[0]);
        RETURN_IF_FALSE(ret == AMF_OK);

        ret = m_spMixer->Mix(outputFloatBufRight, outputMixFloatBufs[1]);
        RETURN_IF_FALSE(ret == AMF_OK);

        ret = m_spConverter->Convert(outputMixFloatBufs[0], 1, sampleCount, pOut, 2, 1.f);
        RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);

        ret = m_spConverter->Convert(outputMixFloatBufs[1], 1, sampleCount, pOut + 1, 2, 1.f);
        RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);
    }


#if 0// Old code: Crossfade, Mixing and Conversion on CPU

    for (int idx = 0; idx < mWavFiles.size(); idx++) {
        for (int chan = 0; chan < 2; chan++){
            RETURN_IF_FAILED(m_spConverter->Convert(pChan[idx] + chan, 2, sampleCount,
                inputFloatBufs[idx*2 + chan], 1, 1.f));
        }
    }

    RETURN_IF_FAILED(m_spConvolution->Process(inputFloatBufs, outputFloatBufs, sampleCount,
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


    AMF_RESULT ret = m_spConverter->Convert(outputFloatBufs[0], 1, sampleCount, pOut, 2, 1.f);
    RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);

    ret = m_spConverter->Convert(outputFloatBufs[1], 1, sampleCount, pOut + 1, 2, 1.f);
    RETURN_IF_FALSE(ret == AMF_OK || ret == AMF_TAN_CLIPPING_WAS_REQUIRED);
#endif
    return 0;
}

int Audio3D::ProcessProc()
{
    uint32_t bytesTotalPlayed(0);
    uint32_t bytesRecorded(0);

    std::array<int16_t, STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE> recordBuffer;
    std::array<int16_t, STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE> outputBuffer;
    FifoBuffer recFifo(STEREO_CHANNELS_COUNT * FILTER_SAMPLE_RATE * sizeof(int16_t));

    int16_t *pWaves[MAX_SOURCES] = {nullptr};
    int16_t *pWaveStarts[MAX_SOURCES] = {nullptr};

    uint32_t sizes[MAX_SOURCES] = {0};

    for(int file = 0; file < mWavFiles.size(); ++file)
    {
        int16_t *data16Bit((int16_t *)&(mWavFiles[file].Data[0]));

        pWaveStarts[file] = pWaves[file] = data16Bit;
        sizes[file] = mWavFiles[file].SamplesCount * sizeof(int16_t); // stereo short samples
    }

    auto *processed = &mStereoProcessedBuffer.front();

    while(!mUpdated && !mStop)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    while(!mStop)
    {
        if(mSrc1EnableMic)
        {
            bytesRecorded = 0;

            while
            (
                !mStop
                &&
                (recFifo.fifoLength() < mBufferSizeInBytes)
            )
            {
                // get some more:
                auto recordedSamplesCount = mPlayer->Record(
                    (unsigned char *)&recordBuffer.front(),
                    //m_bufSize - recordedSamplesCount //todo: unclear meaning, investigate
                    mBufferSizeInBytes - recFifo.fifoLength()
                    );
                std::cout << "recorded: " << recordedSamplesCount << std::endl;

                /*for(int sample = 0; sample < recordedSamplesCount; ++sample)
                {
                    if(recordBuffer[sample * 2] || recordBuffer[sample * 2 + 1])
                    {
                        std::cout << std::hex
                            << sample << ":"
                            << " 0x" << recordBuffer[sample * 2]
                            << " 0x" << recordBuffer[sample * 2 + 1]
                            << std::endl;
                    }
                }*/

                //recFifo.store((char *)&recordBuffer.front(), nRec);
                recFifo.store(
                    (char *)&recordBuffer.front(),
                    STEREO_CHANNELS_COUNT * recordedSamplesCount * sizeof(int16_t)
                    );

                //Sleep(5);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }

            recFifo.retrieve(
                (char *)&recordBuffer.front(),
                //mBufferSizeInBytes
                recFifo.fifoLength()
                );

            // [temporarily] duck the wave audio for mic audio:
            int16_t *wavPtr = pWaves[0];
            pWaves[0] = &recordBuffer.front();

            Process(&outputBuffer.front(), pWaves, mBufferSizeInBytes);

            pWaves[0] = wavPtr;
        }
        else
        {
            Process(&outputBuffer.front(), pWaves, mBufferSizeInBytes);
        }

        /*
        //std::cout << mBufferSizeInBytes << " bytes processed" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(0));

        memcpy(processed, &outputBuffer.front(), mBufferSizeInBytes);

        //todo: mBufferSizeInBytes could be too large at this point
        //use actual filled size instead of mBufferSizeInBytes

        auto bytes2Play = mBufferSizeInBytes;
        unsigned char *pData = (unsigned char *)&outputBuffer.front();

        while(bytes2Play > 0 && !mStop)
        {
            bytesPlayed = mPlayer->Play(pData, bytes2Play, false);
            bytes2Play -= bytesPlayed;

            pData += bytesPlayed;

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        bytesPlayed = mBufferSizeInBytes;

        for (int idx = 0; idx < mWavFiles.size(); idx++)
        {
            pWaves[idx] += bytesPlayed / 2;

            if (pWaves[idx] - pWaveStarts[idx] + (mBufferSizeInBytes / sizeof(int16_t) / STEREO_CHANNELS_COUNT) > sizes[idx])
            {
                pWaves[idx] = pWaveStarts[idx];
            }
        }

        processed += mBufferSizeInBytes / sizeof(int16_t);

        if (processed - &mStereoProcessedBuffer.front() + (mBufferSizeInBytes / sizeof(int16_t)) > mMaxSamplesCount)
        {
            processed = &mStereoProcessedBuffer.front();
        }

        ///compute current sample position for each stream:
        for (int i = 0; i < mWavFiles.size(); i++)
        {
            m_samplePos[i] = (pWaves[i] - pWaveStarts[i]) / sizeof(short);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        */
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

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
    }

    mRunning = false;

    return 0;
}

int64_t Audio3D::getCurrentPosition(int streamIdx)
{
    return m_samplePos[streamIdx];
}


int Audio3D::UpdateProc()
{
    while(mRunning && !mStop)
    {
        while(!mUpdateParams && mRunning && !mStop)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if(!mStop)
        {
            if(mSrc1TrackHeadPos)
            {
                sources[0].speakerX = ears.headX;
                sources[0].speakerY = ears.headY;
                sources[0].speakerZ = ears.headZ;
            }
        }

        for(int idx = 0; !mStop && (idx < mWavFiles.size()); idx++)
        {
            if(m_useClMemBufs)
            {
                m_pTAVR->generateRoomResponse(
                    room,
                    sources[idx],
                    ears,
                    FILTER_SAMPLE_RATE,
                    m_fftLen,
                    oclResponses[idx * 2],
                    oclResponses[idx * 2 + 1],
                    GENROOM_LIMIT_BOUNCES | GENROOM_USE_GPU_MEM,
                    50
                    );
            }
            else
            {
                memset(responses[idx * 2], 0, sizeof(float )* m_fftLen);
                memset(responses[idx * 2 + 1], 0, sizeof(float) * m_fftLen);

                m_pTAVR->generateRoomResponse(
                    room,
                    sources[idx],
                    ears,
                    FILTER_SAMPLE_RATE,
                    m_fftLen,
                    responses[idx * 2],
                    responses[idx * 2 + 1],
                    GENROOM_LIMIT_BOUNCES,
                    50
                    );
            }
        }

        AMF_RESULT ret = AMF_OK;

        while(mRunning && !mStop)
        {
            if(m_useClMemBufs)
            {
                ret = m_spConvolution->UpdateResponseTD(oclResponses, m_fftLen, nullptr, IR_UPDATE_MODE);
            }
            else
            {
                ret = m_spConvolution->UpdateResponseTD(responses, m_fftLen, nullptr, IR_UPDATE_MODE);
            }

            if(ret != AMF_INPUT_FULL)
            {
                break;
            }
        }

        //todo: investigate about AMF_BUSY
        RETURN_IF_FALSE(ret == AMF_OK /*|| ret == AMF_BUSY*/);

        mUpdated = true;

        //Sleep(20);
        mUpdateParams = false;
    }

    return 0;
}

int Audio3D::exportImpulseResponse(int srcNumber, char * fileName)
{
    int convolutionLength = this->m_fftLen;
    m_pTAVR->generateSimpleHeadRelatedTransform(&ears.hrtf, ears.earSpacing);

    float *leftResponse = responses[0];
    float *rightResponse = responses[1];
    short *sSamples = new short[2 * convolutionLength];
     memset(sSamples, 0, 2 * convolutionLength*sizeof(short));

    (void)m_spConverter->Convert(leftResponse, 1, convolutionLength, sSamples, 2, 1.f);

    (void)m_spConverter->Convert(rightResponse, 1, convolutionLength, sSamples + 1, 2, 1.f);

    WriteWaveFileS(fileName, 48000, 2, 16, convolutionLength, sSamples);
    delete[] sSamples;
    return 0;
}