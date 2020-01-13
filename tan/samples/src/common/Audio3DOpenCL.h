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
#pragma once

#include <cstdint>

#include "TrueAudioNext.h"       //TAN
#include "TrueAudioVR.h"

#include "fifo.h"
#include "maxlimits.h"
#include "threads.h"
#include "IWavPlayer.h"
#include "wav.h"
#include "Timer.h"
#include "Allocators.h"

#include <memory>
#include <string>
#include <vector>
#include <array>

// Simple VR audio engine using True Audio Next GPU acceleration
class Audio3D
{
public:
    Audio3D();
    Audio3D(Audio3D const &) = delete;
    virtual ~Audio3D();

    bool Init
    (
        const std::string &     dllPath,
        const RoomDefinition &  roomDef,
        const std::vector<std::string> &
                                fileNames2Open,

        // Source 1 can optionally be captured audio from default microphone:
        bool                    useMicSource,
        const std::vector<bool> &
                                trackHeadPos,

        int                     fftLen,
        int                     bufSize,

        bool                    useCLConvolution,
        bool                    useGPUConvolution,
        int                     deviceIndexConvolution,

#ifdef RTQ_ENABLED
		bool                    useHPr_Conv,
        bool                    useRTQ_Conv,
        int                     cuRes_Conv,
#endif // RTQ_ENABLED

        bool                    useCLRoom,
        bool                    useGPURoom,
        int                     deviceIndexRoom,

#ifdef RTQ_ENABLED
		bool                    useHPr_IRGen,
        bool                    useRTQ_IRGen,
        int                     cuRes_IRGen,
#endif

        amf::TAN_CONVOLUTION_METHOD
                                convMethod,

        const std::string &     playerType
        );

	// finalize, deallocate resources, close files, etc.
	void Close();

	// start audio engine:
    bool Run();

	// Stop audio engine:
    void Stop();

    std::string GetLastError() const
    {
        return mLastError;
    }

protected:
    static bool useIntrinsics;
    static const int IR_UPDATE_MODE = 1; // 0: Non-Blocking 1: Blocking
    static unsigned processThreadProc(void *ptr);
    static unsigned updateThreadProc(void *ptr);

    PrioritizedThread mProcessThread;
    PrioritizedThread mUpdateThread;

    int ProcessProc();
    int UpdateProc();
    int Process(int16_t * pOut, int16_t * pChan[MAX_SOURCES], uint32_t sampleCount);

    bool mRunning = false;
    bool mUpdated = false;
    bool mStop = false;
    bool mUpdateParams = true;
    bool m_useOCLOutputPipeline;

    std::unique_ptr<IWavPlayer> mPlayer; //todo: dynamic creation of choosen player

    std::vector<WavContent>     mWavFiles;
    std::vector<bool>           mTrackHeadPos;

    uint32_t                    mMaxSamplesCount = 0;
    std::vector<int16_t>        mStereoProcessedBuffer;

	TANContextPtr mTANConvolutionContext;
	TANContextPtr mTANRoomContext;

	TANConvolutionPtr mConvolution;
	TANConverterPtr mConverter;
    TANMixerPtr mMixer;
	TANFFTPtr mFft;
	AmdTrueAudioVR *m_pTAVR = NULL;

    RoomDefinition room;
    MonoSource sources[MAX_SOURCES];
	StereoListener ears;

    bool mSrc1EnableMic = false;
    bool mSrc1MuteDirectPath = false;

    AllignedAllocator<float, 32>mResponseBufferStorage;
	float *mResponseBuffer = nullptr;

    float *mResponses[MAX_SOURCES * 2] = {nullptr};
    cl_mem mOCLResponses[MAX_SOURCES * 2] = {nullptr};
    bool   mUseClMemBufs = false;

    std::string mLastError;

    //attention:
    //the following buffers must be 32-bit aligned to use AVX/SSE instructions

    AllignedAllocator<float, 32> mInputFloatBufsStorage[MAX_SOURCES * 2];
    float *mInputFloatBufs[MAX_SOURCES * 2] = {nullptr};

    AllignedAllocator<float, 32> mOutputFloatBufsStorage[MAX_SOURCES * 2];
    float *mOutputFloatBufs[MAX_SOURCES * 2] = {nullptr};

    AllignedAllocator<float, 32> mOutputMixFloatBufsStorage[STEREO_CHANNELS_COUNT];
    float *mOutputMixFloatBufs[2] = {nullptr};

	cl_mem mOutputCLBufs[MAX_SOURCES * 2] = {nullptr};
	cl_mem mOutputMainCLbuf = nullptr;
	cl_mem mOutputMixCLBufs[2] = {nullptr};
	cl_mem mOutputShortBuf = nullptr;

    // RT-Queues
    cl_command_queue mCmdQueue1 = nullptr;
    cl_command_queue mCmdQueue2 = nullptr;
    cl_command_queue mCmdQueue3 = nullptr;
};
