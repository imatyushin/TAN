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
#pragma once
#ifndef AMD_TA_VR
#define AMD_TA_VR

#include "TrueAudioNext.h"       //TAN
#include "Debug.h"

#include "public/include/core/Result.h"
#include "public/include/core/Context.h"
#include "public/include/core/Buffer.h"
#include "public/include/core/Compute.h"
#include "public/include/core/Result.h"
using namespace amf;

#ifndef TAN_NO_OPENCL
  #include <CL/cl.h>
#endif

#include <cmath>
#include <stdio.h>

#define SWAP(a,b) tempr=(a);(a)=(b);(b)=tempr
#define PI 3.141592653589793

#define ATAL_FFT_FORWARD -1
#define ATAL_FFT_INVERSE  1
//#define FFT_LENGTH 32768

#define SPEED_OF_SOUND 343.0 // m/s
//todo: remove hardcode from here
#define FILTER_SAMPLE_RATE 48000
#define STEREO_CHANNELS_COUNT 2

#define DBTODAMP(dB) std::pow(10.0,float(-dB/20.0))
#define DAMPTODB(d) float(-20.0*log10(d))

//flags for generateRoomResponse
#define GENROOM_NONE 0
#define GENROOM_LIMIT_BOUNCES 0x40  // stop after maxBounces reflections on any axis
#define GENROOM_SUPPRESS_DIRECT 0x80  // suppress direct sound path, render only echos
#define GENROOM_USE_GPU_MEM     0x800  // create impulse responses in GPU buffer

#define SIMPLEHRTF_FFTLEN 64

// Head Related Transfer Function
struct HeadModel
{
    int filterLength = 0;
    // head blocks wavelengths shorter the it's diameter:
    float lowPass[SIMPLEHRTF_FFTLEN] = {0};
    float highPass[SIMPLEHRTF_FFTLEN] = {0};

    inline void Debug(const std::string & hint) const
    {
        PrintDebug(
            hint
            + " " + std::to_string(filterLength)
            );
        PrintFloatArray(hint + "lowPass", lowPass, 64);
        PrintFloatArray(hint + "highPass", highPass, 64);
    }
};

struct MonoSource
{
    //speaker position:
    float speakerX = 0.0f;
    float speakerY = 0.0f;
    float speakerZ = 0.0f;

    inline void Debug(const std::string & hint) const
    {
        PrintDebug(
            hint
            + " " + std::to_string(speakerX)
            + " " + std::to_string(speakerY)
            + " " + std::to_string(speakerZ)
            );
    }
};

struct StereoListener
{
    float headX = 0.0f, headY = 0.0f, headZ = 0.0f;
    float earSpacing = 0.0f;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    struct HeadModel hrtf;

    inline void Debug(const std::string & hint) const
    {
        PrintDebug(
            hint
            + " " + std::to_string(headX)
            + " " + std::to_string(headY)
            + " " + std::to_string(headZ)
            + " " + std::to_string(yaw)
            + " " + std::to_string(pitch)
            + " " + std::to_string(roll)
            );
        hrtf.Debug(hint);
    }
};

struct MaterialProperty
{
    // surface acoustic absorption: [required]
    float damp = 0.0f;		    // acoustic absorption factor [0 < damp < 1.0]

    // Frequency response: [optional], set irLen = 0 to disable
    //int irLen;                // length of impulse response
    //float *iresponse;         // impulse response, 48kHz, normalized (will be scaled by dampdB)

    // Surface roughness:  [optional], set nHostLevels = 0 to disable
    //int nHistLevels;	        // roughnessHistogram is array representing
    //float histHeight;		    //	fractions of surface area having heights
    //float *roughnessHistogram;// in each of nHistLevels bands between 0 and histHeight
    inline void Debug(const std::string & hint) const
    {
        PrintDebug(
            hint
            + " " + std::to_string(damp)
            );
    }
};

// RoomDefinition:
// data structure to define dimensions and acoustic properties of a
// rectangular room. All dimensions in meters.
struct RoomDefinition
{
    // room dimensions:
    float width = 0.0f, height = 0.0f, length = 0.0f;
    // wall material properties:
    //left wall, right wall, cieling, floor, front wall, back wall
    MaterialProperty mLeft, mRight, mTop, mBottom, mFront, mBack;

    inline void Debug(const std::string & hint) const
    {
        PrintDebug(
            hint
            + " " + std::to_string(width)
            + " " + std::to_string(height)
            + " " + std::to_string(length)
            );
        mLeft.Debug(hint + " mLeft");
        mRight.Debug(hint + " mRight");

        mTop.Debug(hint + " mTop");
        mBottom.Debug(hint + " mBottom");

        mFront.Debug(hint + " mFront");
        mBack.Debug(hint + " mBack");
    }
};

#define DOOR_VS_SPACING 0.01; // 1cm = 1/2 wavelength at 17 kHz

struct Door
{
    float r1cX = 0.0f, r1cY = 0.0f, r1cZ = 0.0f;    //  center
    float r1blX = 0.0f, r1blY = 0.0f, r1blZ = 0.0f; //  bottom left corner
    float r1brX = 0.0f, r1brY = 0.0f, r1brZ = 0.0f; //  bottom right corner
    // tr = bl + 2*(c - bl)
    // tl = br + 2*(c - br)
    float r2cX = 0.0f, r2cY = 0.0f, r2cZ = 0.0f;    //  center
    float r2blX = 0.0f, r2blY = 0.0f, r2blZ = 0.0f; //  bottom left corner
    float r2brX = 0.0f, r2brY = 0.0f, r2brZ = 0.0f; //  bottom right corner
};

class TAN_SDK_LINK AmdTrueAudioVR
{
public:
    enum VRExecutionMode
    {
        CPU,
        GPU
    };

private:

protected:
    AmdTrueAudioVR() { };
public:

    static constexpr float S = 340.0f;

    static const int localX = 4;
    static const int localY = 4;
    static const int localZ = 4;

    static const int localSizeFill = 256;

    static const int HeadFilterSize = 64;

    static bool useIntrinsics;

    virtual ~AmdTrueAudioVR()
    {
    };

    virtual void generateRoomResponse(
        const RoomDefinition & room,
        MonoSource source,
        const StereoListener & ear,
        int inSampRate,
        int responseLength,
        void *responseL,
        void *responseR,
        int flags = 0,
        int maxBounces = 0
        ) = 0;

    virtual void generateDirectResponse(
        const RoomDefinition & room,
        MonoSource source,
        const StereoListener & ear,
        int inSampRate,
        int responseLength,
        void *responseL,
        void *responseR,
        int *pFirstNonZero,
        int *pLastNonZero
        ) = 0;

#ifdef DOORWAY_TRANSFORM
    virtual void generateDoorwayResponse(
        const RoomDefinition & room1,
        const RoomDefinition & room2,
        const MonoSource & source,
        const Door & door,
        const StereoListener & ear,
        int inSampRate,
        int responseLength,
        float *responseLeft,
        float *responseRight,
        int flags,
        int maxBounces
        ) = 0;
#endif

    virtual void SetExecutionMode(VRExecutionMode executionMode) = 0;
    virtual VRExecutionMode GetExecutionMode() = 0;

    /**************************************************************************************************
    AmdTrueAudio::generateSimpleHeadRelatedTransform:

    Generates a simple head related transfer function (HRTF) for acoustic shadowing as heard by
    a human ear on a human head, as a function of angle to a sound source.

    This function models the head as a sphere of diameter earSpacing * 1.10, and generates a table of
    180 impulse response curves for 1 degree increments from the direction the ear points.

    **************************************************************************************************/
    virtual void generateSimpleHeadRelatedTransform(HeadModel & head, float earSpacing) = 0;

    virtual void applyHRTF(const HeadModel & head, float scale, float *response, int length, float earVX, float earVY, float earVZ, float srcVX, float srcVY, float srcZ) = 0;
    virtual void applyHRTFoptCPU(const HeadModel & head, float scale, float *response, int length, float earVX, float earVY, float earVZ, float srcVX, float srcVY, float srcZ) = 0;
};


// TAN objects creation functions.
extern "C"
{
#ifndef TAN_NO_OPENCL
    TAN_SDK_LINK AMF_RESULT TAN_CDECL_CALL CreateAmdTrueAudioVR(
        AmdTrueAudioVR **taVR,
        const TANContextPtr & pContext,
        const TANFFTPtr & pFft,
        cl_command_queue cmdQueue,
        float samplesPerSecond,
        int convolutionLength
        );
#else
    TAN_SDK_LINK AMF_RESULT TAN_CDECL_CALL CreateAmdTrueAudioVR(
        AmdTrueAudioVR **taVR,
        const TANContextPtr & pContext,
        const TANFFTPtr & pFft,
        AMFCompute * cmdQueue,
        float samplesPerSecond,
        int convolutionLength,
		amf::AMFFactory * factory = nullptr
        );
#endif

    TAN_SDK_LINK float estimateReverbTime(const RoomDefinition & room, float finaldB, int *nReflections);
}

#endif // #ifndef AMD_TA_VR
