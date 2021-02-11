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

// rotation, translation matrix
class TransRotMtx
{
private:
	float m[3][4] = {0};

public:
	TransRotMtx()
    {
        std::memset(m, 0, sizeof(m));
        m[0][0] = 1.0;
        m[1][1] = 1.0;
        m[2][2] = 1.0;
    }

	void setAngles(float yaw, float pitch, float roll)
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

	void setOffset(float x, float y, float z)
    {
        m[0][3] = x;
        m[1][3] = y;
        m[2][3] = z;
    }

	void transform(float &X, float &Y, float &Z)
    {
        float x = X;
        float y = Y;
        float z = Z;
        X = x*m[0][0] + y*m[0][1] + z*m[0][2] + m[0][3];
        Y = x*m[1][0] + y*m[1][1] + z*m[1][2] + m[1][3];
        Z = x*m[2][0] + y*m[2][1] + z*m[2][2] + m[2][3];
    }
};

enum RoomUpdateMode
{
    NonBlocking = 0,
    Blocking = 1
};

// Simple VR audio engine using True Audio Next GPU acceleration
class IAudio3D
{
public:
    IAudio3D()
    {
    }
    IAudio3D(IAudio3D const &) = delete;

    virtual ~IAudio3D()
    {
        Close();
    }

    virtual AMF_RESULT          Init
    (
        const std::string &     dllPath,
        const RoomDefinition &  roomDef,
        const std::vector<std::string> &
                                fileNames2Open,

        // Source 1 can optionally be captured audio from default microphone:
        bool                    useMicSource,
        const std::vector<bool> &
                                trackHeadPos,

        int                     fftLength,
        int                     bufferSizeInSamples,

        bool                    computeConvolution,
        bool                    computeOverGpuConvolution,
        int                     computeDeviceIndexConvolution,

#ifdef RTQ_ENABLED
		bool                    useHPr_Conv,
        bool                    useRTQ_Conv,
        int                     cuRes_Conv,
#endif // RTQ_ENABLED

        bool                    computeRoom,
        bool                    computeOverGpuRoom,
        int                     computeDeviceIndexRoom,

#ifdef RTQ_ENABLED
		bool                    useHPr_IRGen,
        bool                    useRTQ_IRGen,
        int                     cuRes_IRGen,
#endif

        amf::TAN_CONVOLUTION_METHOD
                                convMethod,

        IWavPlayer *            player,

        RoomUpdateMode          roomUpdateMode = RoomUpdateMode::Blocking
        )
    {
        if((computeOverGpuConvolution && !computeConvolution) || (computeOverGpuRoom && !computeRoom))
        {
            std::cerr
                << "Error: GPU queues must be used only if OpenCL flag is set"
                << std::endl;

            return AMF_INVALID_ARG;
        }

        //mComputedOutputPipeline = useGPU_Conv && useGPU_IRGen;
        mComputedOutputPipeline = computeConvolution || computeRoom;

        mSrc1EnableMic = useMicSource;
        mTrackHeadPos = trackHeadPos;
        mBufferSizeInSamples = bufferSizeInSamples;
        mBufferSizeInBytes = mBufferSizeInSamples * STEREO_CHANNELS_COUNT * sizeof(int16_t);
        mConvolutionMethod = convMethod;

        mComputeConvolution = computeConvolution;
        mComputeOverGpuConvolution = computeOverGpuConvolution;
        mComputeDeviceIndexConvolution = computeDeviceIndexConvolution;

        mComputeRoom = computeRoom;
        mComputeOverGpuRoom = computeOverGpuRoom;
        mComputeDeviceIndexRoom = computeDeviceIndexRoom;

        mWavFiles.reserve(fileNames2Open.size());

        for(const auto& fileName: fileNames2Open)
        {
            WavContent content;

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
        mPlayer = player;

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
        while(mFFTLength < fftLength && (mFFTLength << 1) <= MAXRESPONSELENGTH)
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
            memset(mResponses[i], 0, sizeof(float) * mFFTLength);

            mInputFloatBufsStorage[i].Clear();
            mOutputFloatBufsStorage[i].Clear();
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

        return InitObjects();
    }

    virtual AMF_RESULT InitObjects() = 0;

    // finalize, deallocate resources, close files, etc.
	virtual void Close()
    {
        mRunning = false;

        if(mPlayer)
        {
            mPlayer->Close();
        }

        mTrueAudioVR.reset();

        // release smart pointers:
        mFft.Release();
        mConvolution.Release();
        mConverter.Release();
        mMixer.Release();
        mTANRoomContext.Release();
        mTANConvolutionContext.Release();

        mWavFiles.resize(0);
    }

	// start audio engine:
    virtual bool Run() = 0;

	// Stop audio engine:
    virtual void Stop() = 0;

    const std::string & GetLastError() const
    {
        return mLastError;
    }

public:
    //todo: move to Init with help of struct
    // Set transform to translate game coordinates to room audio coordinates:
	virtual int setWorldToRoomCoordTransform(
        float translationX,
        float translationY,
        float translationZ,
        float rotationY,
        float headingOffset,
        bool headingCCW
        )
    {
        m_mtxWorldToRoomCoords.setOffset(translationX, translationY, translationZ);
        m_mtxWorldToRoomCoords.setAngles(rotationY, 0.0, 0.0);
        m_headingOffset = headingOffset;
        m_headingCCW = headingCCW;

        return 0;
    }

	// get's the current playback position in a stream:
    //int64_t getCurrentPosition(int stream);

	// update the head (listener) position:
    virtual int updateHeadPosition(float x, float y, float z, float yaw, float pitch, float roll)
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

	//update a source position:
    virtual int updateSourcePosition(int srcNumber, float x, float y, float z)
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
	//update a room's dimension:
	virtual int updateRoomDimension(float _width, float _height, float _length)
    {
        room.width = _width;
        room.height = _height;
        room.length = _length;

        mUpdateParams = true;

        return 0;
    }

	//update a room's damping factor
	virtual int updateRoomDamping(float _left, float _right, float _top, float _buttom, float _front, float _back)
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

    // export impulse response for source  + current listener and room:
    virtual int exportImpulseResponse(int srcNumber, char * fileName)
    {
        int convolutionLength = this->mFFTLength;
        mTrueAudioVR->generateSimpleHeadRelatedTransform(
            ears.hrtf,
            ears.earSpacing
            );

        float *leftResponse = mResponses[0];
        float *rightResponse = mResponses[1];
        short *sSamples = new short[2 * convolutionLength];
        memset(sSamples, 0, 2 * convolutionLength*sizeof(short));

        (void)mConverter->Convert(leftResponse, 1, convolutionLength, sSamples, 2, 1.f);
        (void)mConverter->Convert(rightResponse, 1, convolutionLength, sSamples + 1, 2, 1.f);

        WriteWaveFileS(fileName, FILTER_SAMPLE_RATE, STEREO_CHANNELS_COUNT, 16, convolutionLength, sSamples);
        delete[] sSamples;
        return 0;
    }

    virtual AmdTrueAudioVR * getAMDTrueAudioVR()
    {
        return mTrueAudioVR.get();
    }

    virtual const TANConverterPtr & getTANConverter()
    {
        return mConverter;
    }

protected:
    bool                        mComputeConvolution = false;
    bool                        mComputeOverGpuConvolution = false;
    int                         mComputeDeviceIndexConvolution = -1;

    bool                        mComputeRoom = false;
    bool                        mComputeOverGpuRoom = false;
    int                         mComputeDeviceIndexRoom = -1;

    bool                        mUseComputeBuffers = false;
    bool                        mComputedOutputPipeline = false;

    amf::TAN_CONVOLUTION_METHOD mConvolutionMethod = amf::TAN_CONVOLUTION_METHOD::TAN_CONVOLUTION_METHOD_FFT_OVERLAP_ADD;

    PrioritizedThread           mProcessThread;
    PrioritizedThread           mUpdateThread;

    virtual int                 ProcessProc() = 0;
    virtual int                 UpdateProc() = 0;
    virtual AMF_RESULT          Process(int16_t * pOut, int16_t * pChan[MAX_SOURCES], uint32_t sampleCount) = 0;

    bool                        mRunning = false;
    bool                        mUpdated = false;
    bool                        mStop = false;
    bool                        mUpdateParams = true;

    //std::unique_ptr<IWavPlayer> mPlayer; //todo: dynamic creation of choosen player
    IWavPlayer *                mPlayer = nullptr;

    std::vector<WavContent>     mWavFiles;
    std::vector<bool>           mTrackHeadPos;

    Timer                       mTimer;
    double                      mStartTime = 0.0;
    uint64_t                    mSamplesSent = 0;

    uint32_t                    mMaxSamplesCount = 0;
    std::vector<int16_t>        mStereoProcessedBuffer;

	TANContextPtr               mTANConvolutionContext;
	TANContextPtr               mTANRoomContext;

	TANConvolutionPtr           mConvolution;
	TANConverterPtr             mConverter;

    TANMixerPtr                 mMixer;
	TANFFTPtr                   mFft;

    std::unique_ptr<AmdTrueAudioVR>
                                mTrueAudioVR;

    RoomDefinition room;
    MonoSource sources[MAX_SOURCES];
	StereoListener ears;

    bool mSrc1EnableMic = false;
    bool mSrc1MuteDirectPath = false;

    AllignedAllocator<float, 32> mResponseBufferStorage;
	float *mResponseBuffer = nullptr;

    float *mResponses[MAX_SOURCES * 2] = {nullptr};
    //cl_mem mOCLResponses[MAX_SOURCES * 2] = {nullptr};

    std::string mLastError;

    //attention:
    //the following buffers must be 32-bit aligned to use AVX/SSE instructions

    AllignedAllocator<float, 32> mInputFloatBufsStorage[MAX_SOURCES * 2];
    float *mInputFloatBufs[MAX_SOURCES * 2] = {nullptr};

    AllignedAllocator<float, 32> mOutputFloatBufsStorage[MAX_SOURCES * 2];
    float *mOutputFloatBufs[MAX_SOURCES * 2] = {nullptr};

    AllignedAllocator<float, 32> mOutputMixFloatBufsStorage[STEREO_CHANNELS_COUNT];
    float *mOutputMixFloatBufs[2] = {nullptr};

	//cl_mem mOutputCLBufs[MAX_SOURCES * 2] = {nullptr};
	//cl_mem mOutputMainCLbuf = nullptr;
	//cl_mem mOutputMixCLBufs[2] = {nullptr};
	//cl_mem mOutputShortBuf = nullptr;

	// current position in each stream:
	//int64_t m_samplePos[MAX_SOURCES];

	// fft length must be power of 2
	// 65536/48000 = reverb time of 1.37 seconds
    int mFFTLength = 65536;  //default fft length

	// buffer length 4096 / 48000 = 85 ms update rate:
	//int m_bufSize = 4096 * 4; // default buffer length
    uint32_t mBufferSizeInSamples = 0;
    uint32_t mBufferSizeInBytes = 0;

	// World To Room coordinate transform:
	TransRotMtx m_mtxWorldToRoomCoords;
	float m_headingOffset;
	bool m_headingCCW;
};
