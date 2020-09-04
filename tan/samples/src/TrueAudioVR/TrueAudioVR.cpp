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

// TrueAudioVR.cpp : Defines the exported functions for the DLL application.
//
#include "stdafx.h"
#include "TrueAudioVR.h"

#include "public/common/TraceAdapter.h"

#ifdef USE_METAL
  #include "Kernels/Fill.metal.h"
  #include "Kernels/GenerateRoomResponse.metal.h"
#else
  #include "Kernels/Fill.cl.h"
  #include "Kernels/GenerateRoomResponse.cl.h"
#endif

#include "OCLHelper.h"
#include "cpucaps.h"
#include "Exceptions.h"
#include "Debug.h"

#ifdef OMP_ENABLED
  #include <omp.h>
#endif
//#if !defined(__APPLE__) && !defined(__MACOSX)
//  #include <omp.h>
//#endif

#include <immintrin.h>

#include <fstream>
#include <sstream>
#include <cmath>

//const InstructionSet::InstructionSet_Internal InstructionSet::CPU_Rep;
bool AmdTrueAudioVR::useIntrinsics = true; // InstructionSet::AVX() && InstructionSet::FMA();

const float AmdTrueAudioVR::S = 340.0; //Speed of sound

class TrueAudioVRimpl: AmdTrueAudioVR
{
private:
    TANFFTPtr mFFT;
    TANMathPtr mMath;
    TANContextPtr mContext;

#ifndef TAN_NO_OPENCL
    bool m_clInitialized = false;

    //OpenCL initialization
    cl_context m_context = nullptr;
    cl_command_queue m_cmdQueue = nullptr;

    cl_kernel m_kernel = nullptr;
    cl_kernel m_kernelFill = nullptr;

    cl_mem m_pReflection = nullptr;
    cl_mem m_pResponse = nullptr;
    cl_mem m_pFloatResponse = nullptr;
    cl_mem m_pLPF = nullptr;
    cl_mem m_pHPF = nullptr;

#else

    bool mInitialized = false;

	//todo: smrtptr
	amf::AMFFactory * mFactory = nullptr;
    bool mFactoryCreated = false;

    amf::AMFComputePtr mCompute;

    amf::AMFComputeKernelPtr mKernelResponse;
    amf::AMFComputeKernelPtr mKernelFill;

    amf::AMFBufferPtr mReflection = nullptr;
    amf::AMFBufferPtr mResponse = nullptr;
    amf::AMFBufferPtr mFloatResponse = nullptr;
    amf::AMFBufferPtr mLPF = nullptr;
    amf::AMFBufferPtr mHPF = nullptr;
#endif

    std::vector<float> mResponseBuffer;
    uint32_t mResponseLength = 0;

    //speed of sound = 340 m/s
    size_t m_globalWorkSize[3] = {0};
    size_t m_globaSizeFill = 0;

#ifndef TAN_NO_OPENCL
    AMF_RESULT InitializeCL(
        StereoListener & ears,
        int nW,
        int nH,
        int nL,
        int responseLength
        );
#else
    AMF_RESULT InitializeAMF(
        StereoListener & ears,
        int nW,
        int nH,
        int nL,
        int responseLength
        );
#endif

    static float freq(int i, int samplesPerSec, int fftLen);

    size_t RoundUp(int groupSize, int globalSize)
    {
        int r = globalSize % groupSize;
        if (r == 0)
        {
            return globalSize;
        }
        else
        {
            return globalSize + groupSize - r;
        }
    }

    AMF_RESULT generateRoomResponseGPU(
        const MonoSource & sound,
        const RoomDefinition & room,
        float* response,
        float headX,
        float headY,
        float headZ,
        float earVX,
        float earVY,
        float earVZ,
        float earV,
        float maxGain,
        float dMin,
        int inSampRate,
        int responseLength,
        int hrtfResponseLength,
        int filterLength,
        int nW,
        int nH,
        int nL
        );

#ifndef TAN_NO_OPENCL

    AMF_RESULT generateRoomResponseGPU(
        const MonoSource & sound,
        const RoomDefinition & room,
        cl_mem response,
        float headX,
        float headY,
        float headZ,
        float earVX,
        float earVY,
        float earVZ,
        float earV,
        float maxGain,
        float dMin,
        int inSampRate,
        int responseLength,
        int hrtfResponseLength,
        int filterLength,
        int nW,
        int nH,
        int nL
        );

#else

    AMF_RESULT generateRoomResponseGPU(
        const MonoSource & sound,
        const RoomDefinition & room,
        AMFBuffer * response,
        float headX,
        float headY,
        float headZ,
        float earVX,
        float earVY,
        float earVZ,
        float earV,
        float maxGain,
        float dMin,
        int inSampRate,
        int responseLength,
        int hrtfResponseLength,
        int filterLength,
        int nW,
        int nH,
        int nL
        );

#endif

    AMF_RESULT Release();

private:
    VRExecutionMode m_executionMode = VRExecutionMode::CPU;

    void generateRoomResponseCPU(
        const RoomDefinition & room,
        const MonoSource & sound,
        float earSpacing,
        const HeadModel & hrtf,
        float* response,
        float headX,
        float headY,
        float headZ,
        float earVX,
        float earVY,
        float earVZ,
        int inSampRate,
        int responseLength,
        int hrtfResponseLength,
        int nW,
        int nH,
        int nL,
        int flag = 0
        );

    void generateDirectResponseCPU(
        const RoomDefinition & room,
        const MonoSource & sound,
        float earSpacing,
        const HeadModel & hrtf,
        float* response,
        float headX,
        float headY,
        float headZ,
        float earVX,
        float earVY,
        float earVZ,
        int inSampRate,
        int responseLength,
        int *firstNonZero,
        int *lastNonZero
        );

    void generateDoorwayDirectResponse(
        const RoomDefinition & room1,
        const RoomDefinition & room2,
        const MonoSource & source,
        const Door & door,
        const StereoListener & ear,
        int inSampRate,
        int responseLength,
        float *responseIn,
        float *responseLeft,
        float *responseRight,
        int flags
        );

public:

#ifndef TAN_NO_OPENCL

    TrueAudioVRimpl(
        const TANContextPtr & context,
        const TANFFTPtr & fft,
        cl_command_queue queue,
        float samplesPerSecond,
        int convolutionLength
        );

#else

    TrueAudioVRimpl(
        const TANContextPtr & context,
        const TANFFTPtr & fft,
        const AMFComputePtr & queue,
        float samplesPerSecond,
        int convolutionLength,
		AMFFactory * factory
        );

#endif

    virtual ~TrueAudioVRimpl();

    AMF_RESULT Initialize(
        StereoListener & ears,
        int nW,
        int nH,
        int nL,
        int responseLength
        );

    void generateRoomResponse(
        const RoomDefinition & room,
        MonoSource source,
        StereoListener & ear,
        int inSampRate,
        int responseLength,
        void *responseLeft,
        void *responseRight,
        int flags = 0,
        int maxBounces = 0
        );

    void generateDirectResponse(
        const RoomDefinition & room,
        MonoSource source,
        const StereoListener & ear,
        int inSampRate,
        int responseLength,
        void *responseLeft,
        void *responseRight,
        int *firstNZ,
        int *lastNZ
        );

    void generateDoorwayResponse(
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
        );

    void SetExecutionMode(VRExecutionMode executionMode)
    {
        m_executionMode = executionMode;
    }

    VRExecutionMode GetExecutionMode()
    {
        return m_executionMode;
    }

    /**************************************************************************************************
    AmdTrueAudio::generateSimpleHeadRelatedTransform:

    Generates a simple head related transfer function (HRTF) for acoustic shadowing as heard by
    a human ear on a human head, as a function of angle to a sound source.

    This function models the head as a sphere of diameter earSpacing * 1.10, and generates a table of
    180 impulse response curves for 1 degree increments from the direction the ear points.

    **************************************************************************************************/
    void generateSimpleHeadRelatedTransform(HeadModel & head, float earSpacing);

    void applyHRTF(const HeadModel & head, float scale, float *response, int length, float earVX, float earVY, float earVZ, float srcVX, float srcVY, float srcZ);
    void applyHRTFoptCPU(const HeadModel & head, float scale, float *response, int length, float earVX, float earVY, float earVZ, float srcVX, float srcVY, float srcZ);
};

#ifndef TAN_NO_OPENCL
TAN_SDK_LINK AMF_RESULT TAN_CDECL_CALL CreateAmdTrueAudioVR
(
    AmdTrueAudioVR **taVR,
    const TANContextPtr & context,
    const TANFFTPtr & fft,
    cl_command_queue cmdQueue,
    float samplesPerSecond,
    int convolutionLength
)
{
    *taVR = (AmdTrueAudioVR *) new TrueAudioVRimpl(
        context,
        fft,
        cmdQueue,
        samplesPerSecond,
        convolutionLength
        );

    return AMF_OK;
}
#else
TAN_SDK_LINK AMF_RESULT TAN_CDECL_CALL CreateAmdTrueAudioVR
(
    AmdTrueAudioVR **taVR,
    const TANContextPtr & context,
    const TANFFTPtr & fft,
    AMFCompute * compute,
    float samplesPerSecond,
    int convolutionLength,
	amf::AMFFactory * factory
)
{
    *taVR = (AmdTrueAudioVR *) new TrueAudioVRimpl(
        context,
        fft,
        compute,
        samplesPerSecond,
        convolutionLength,
		factory
        );

    return AMF_OK;
}
#endif

#ifndef TAN_NO_OPENCL
TrueAudioVRimpl::TrueAudioVRimpl(
    const TANContextPtr &   context,
    const TANFFTPtr &       fft,
    cl_command_queue        queue,
    float                   samplesPerSecond,
    int                     convolutionLength
    ):
    mContext    (context),
    mFFT        (fft),
    m_cmdQueue  (queue)
{
    if(m_cmdQueue)
    {
        clRetainCommandQueue(m_cmdQueue);
        CLQUEUE_REFCOUNT(m_cmdQueue);
    }
}
#else
TrueAudioVRimpl::TrueAudioVRimpl(
    const TANContextPtr &   context,
    const TANFFTPtr &       fft,
    const AMFComputePtr &   compute,
    float                   samplesPerSecond,
    int                     convolutionLength,
	AMFFactory *            factory
    ):
	mFactory    (factory),
    mContext    (context),
    mFFT        (fft),
    mCompute    (compute)
{
}
#endif

TrueAudioVRimpl::~TrueAudioVRimpl()
{
#ifndef TAN_NO_OPENCL
    if (m_cmdQueue != 0)
	{
		DBG_CLRELEASE_QUEUE(m_cmdQueue,"m_cmdQueue");
	}
#endif
}

AMF_RESULT TrueAudioVRimpl::Release()
{
#ifndef TAN_NO_OPENCL
    if (m_context)
    {
        clReleaseContext(m_context);
        m_context = NULL;
    }

    if (m_cmdQueue)
    {
        //printf("Queue release %llX\r\n", m_cmdQueue);
        DBG_CLRELEASE_QUEUE(m_cmdQueue,"m_cmdQueue");
        m_cmdQueue = NULL;
    }

    if (m_kernel)
    {
        clReleaseKernel(m_kernel);
        m_kernel = NULL;
    }

    if (m_kernelFill)
    {
        clReleaseKernel(m_kernelFill);
        m_kernelFill = NULL;
    }

    if (m_pReflection)
    {
        clReleaseMemObject(m_pReflection);
        m_pReflection = NULL;
    }

    if (m_pResponse)
    {
        clReleaseMemObject(m_pResponse);
        m_pResponse = NULL;
    }

    if (m_pFloatResponse)
    {
        clReleaseMemObject(m_pFloatResponse);
        m_pFloatResponse = NULL;
    }

    if (m_pLPF)
    {
        clReleaseMemObject(m_pLPF);
        m_pLPF = NULL;
    }

    if (m_pHPF)
    {
        clReleaseMemObject(m_pHPF);
        m_pHPF = NULL;
    }
#else
    if(mFactoryCreated)
    {
        AMF_RETURN_IF_FAILED(g_AMFFactory.Terminate());
    }

    THROW_NOT_IMPLEMENTED;

    return AMF_NOT_IMPLEMENTED;
#endif

    return AMF_OK;
}

/**************************************************************************************************
freq	-	compute the frequency corresponding to a point in an FFT:
**************************************************************************************************/
float TrueAudioVRimpl::freq(int i, int samplesPerSec, int fftLen)
{
    float f;
    float inDelta = (float)(1.0 / (float)samplesPerSec);

    f = (float)((float)(i / 2) / ((float)fftLen*inDelta));

    return(f);
}


/**************************************************************************************************
AmdTrueAudio::generateSimpleHeadRelatedTransform:

Generates a simple head related transfer function (HRTF) for acoustic shadowing as heard by
a human ear on a human head.

This function models the head as a sphere of diameter earSpacing * 1.10, and generates
a low pass filter representing frequency response for a source shadowed by the head,
and a complementary high pass filter, such that the two filters sum to all pass.

**************************************************************************************************/
void TrueAudioVRimpl::generateSimpleHeadRelatedTransform(
    HeadModel & head,
    float earSpacing
    )
{

    int fftLen = 1;
    int log2len = 0;
    while ((int)fftLen < SIMPLEHRTF_FFTLEN){
        fftLen <<= 1;
        ++log2len;
    }
    head.filterLength = fftLen;

    std::vector<float> impulseHolder(fftLen * 2);
    float *impulse(impulseHolder.data());
    memset(impulse, 0, sizeof(float) * fftLen * 2);
    impulse[0] = 1.0;

    // Wavelengths smaller than a head are blocked by it:
    //float cornerFreq = 0.25 * SPEED_OF_SOUND / (1.10*earSpacing); //~ 500Hz
    float cornerFreq = float(SPEED_OF_SOUND / (1.10 * earSpacing)); //~ 2kHz
    memset(head.lowPass, 0, sizeof(float) * fftLen);
    memset(head.highPass, 0, sizeof(float) * fftLen);

    if (mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &impulse, &impulse) != AMF_OK)
    {
        return;
    }
    //m_ata->Fft(1, &impulse, &impulse, log2len, AmdTrueAudio::TA_FFT_DIR::FORWARD);

    // filter it...
    for (int j = 0; j < fftLen; j++)
    {
        float f = freq(j, FILTER_SAMPLE_RATE, fftLen);
        float d = float(cornerFreq / (cornerFreq + f));
        //std::cout << "freq: " << f << " d: " << d << std::endl;

        impulse[j << 1] *= d;
        impulse[(j << 1) + 1] *= d;
    }

    if (mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_BACKWARD, log2len, 1, &impulse, &impulse) != AMF_OK)
    {
        return;
    }
    //m_ata->Fft(1, &impulse, &impulse, log2len, AmdTrueAudio::TA_FFT_DIR::REVERSE);

    // extract real part of each sample
    for (int k = 0; k<2 * fftLen; k += 2){
        head.lowPass[k >> 1] = impulse[k];
    }

    // complementary high pass filter
    head.highPass[0] = 1.0;
    for (int i = 0; i < fftLen; i++){
        head.highPass[i] -= head.lowPass[i];
    }
}

/**************************************************************************************************
AmdTrueAudioVR::applyHRTF:

Applies a simple head related transfer function (HRTF) for acoustic shadowing as heard by
a human ear on a human head, as a function of angle to a sound source.

**************************************************************************************************/

void TrueAudioVRimpl::applyHRTF(
    const HeadModel & head,
    float scale, float *response, int length,
    float earVX, float earVY, float earVZ,
    float srcVX, float srcVY, float srcVZ
    )
{
#ifdef _WIN32
    if (AmdTrueAudioVR::useIntrinsics){
        return applyHRTFoptCPU(head, scale, response, length, earVX, earVY, earVZ, srcVX, srcVY, srcVZ);
    }
#endif

    // dot prod
    float earV = sqrtf(earVX*earVX + earVY*earVY + earVZ*earVZ);
    float srcV = sqrtf(srcVX*srcVX + srcVY*srcVY + srcVZ*srcVZ);
    float dp = earVX*srcVX + earVY*srcVY + earVZ*srcVZ;
    float cosA = dp / (earV*srcV);
    float hf = float((cosA + 1.0) / 2.0);

    int len = head.filterLength;
    if (len > length)
        len = length;

    //#pragma omp parallel for
    for (int i = 0; i < len; i++)
    {
        response[i] += scale*(head.lowPass[i] + hf*head.highPass[i]);
    }
}

/**************************************************************************************************
AmdTrueAudioVR::applyHRTFoptCPU:       CPU optimized version of applyHRTF
**************************************************************************************************/

void TrueAudioVRimpl::applyHRTFoptCPU(
    const HeadModel & head,
    float scale, float *response, int length,
    float earVX, float earVY, float earVZ,
    float srcVX, float srcVY, float srcVZ
    )
{
    if (length < head.filterLength)
        return;

    // dot prod
    float earV = sqrtf(earVX*earVX + earVY*earVY + earVZ*earVZ);
    float srcV = sqrtf(srcVX*srcVX + srcVY*srcVY + srcVZ*srcVZ);
    float dp = earVX*srcVX + earVY*srcVY + earVZ*srcVZ;
    float cosA = dp / (earV*srcV);
    float hf = float((cosA + 1.0) / 2.0);

    int len = head.filterLength;

    // use AVX and FMA intrinsics to process 8 samples per pass
    register __m256 hfReg, scaleReg;
    register __m256 *hpReg, *lpReg, *respReg, tmpReg;
    _mm256_zeroall();
    hfReg = _mm256_set1_ps(hf);
    scaleReg = _mm256_set1_ps(scale);
    hpReg = (__m256 *)head.highPass;
    lpReg = (__m256 *)head.lowPass;
    respReg = (__m256 *)response;

    // unrolled version - a little faster
#if SIMPLEHRTF_FFTLEN == 64
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[0], lpReg[0]);
    respReg[0] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[0]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[1], lpReg[1]);
    respReg[1] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[1]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[2], lpReg[2]);
    respReg[2] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[2]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[3], lpReg[3]);
    respReg[3] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[3]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[4], lpReg[4]);
    respReg[4] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[4]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[5], lpReg[5]);
    respReg[5] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[5]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[6], lpReg[6]);
    respReg[6] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[6]);
    tmpReg = _mm256_fmadd_ps(hfReg, hpReg[7], lpReg[7]);
    respReg[7] = _mm256_fmadd_ps(scaleReg, tmpReg, respReg[7]);
#else
    len >>= 3;
    do
    {
        tmpReg = _mm256_fmadd_ps(hfReg, *hpReg, *lpReg);
        *respReg = _mm256_fmadd_ps(scaleReg, tmpReg, *respReg);
        hpReg++;
        lpReg++;
        respReg++;
    } while (--len > 0);
#endif
}


TAN_SDK_LINK float estimateReverbTime(RoomDefinition room, float finaldB, int *nReflections)
{
    double S = 340; // speed of sound m/s
    double dbL = float(DAMPTODB(room.mFront.damp) + DAMPTODB(room.mBack.damp)) / 2;
    double dbH = float(DAMPTODB(room.mBottom.damp) + DAMPTODB(room.mTop.damp)) / 2;
    double dbW = float(DAMPTODB(room.mLeft.damp) + DAMPTODB(room.mRight.damp)) / 2;

    double dbT = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double dz = 0.0;
    int nW = 1;
    while (dbT < finaldB){
        dx += room.width;
        dbT = nW * dbW + -10.0*log10(room.width / dx);
        ++nW;
    }
    nW *= 2;
    dbT = 0.0;
    int nH = 1;
    while (dbT < finaldB){
        dy += room.height;
        dbT = nH * dbH + -10.0*log10(room.height / dy);
        ++nH;
    }
    nH *= 2;
    dbT = 0.0;
    int nL = 1;
    while (dbT < finaldB){
        dz += room.length;
        dbT = nL * dbL + -10.0*log10(room.length / dz);
        ++nL;
    }
    nL *= 2;
    double d = sqrt(dx*dx + dy*dy + dz*dz);

    if (nReflections != NULL)
        *nReflections = nH*nW*nL;

    return float(d / S); // reverb time in seconds

}

/**************************************************************************************************
static inline void rotate(float &X, float &Y, float &Z, float yaw, float pitch, float roll)

rotation matrices:

For X => right, Y => up,  Z -> in

Rx   1    0    0   Pitch
     0  cos -sin
     0  sin  cos

Ry cos    0  sin   Yaw
     0    1    0
  -sin    0  cos

Rz cos -sin    0   Roll
   sin  cos    0
     0    0    1

but we want to do in Yaw,Pitch,Roll order
where: cosY = cos(Yaw), cosP= cos(Pitch) cosR = cos(Roll)

 cosR -sinR    0      1     0     0       cosY    0  sinY       X
 sinR  cosR    0   x  0  cosP -sinP   x 	 0    1    0    x   Y
   0     0     1      0  sinP  cosP		 -sinY    0  cosY		Z

  cosY         0       sinY
 sinPsinY   cosP  -sinPcosY
 -cosPsinY  sinP   cosPcosY

     combined yaw,pitch,roll matrix:

  (cosRcosY -sinRsinPsinY)   (-sinRcosP)   (cosRsinY + sinRsinPcosY)
  (sinRcosY +cosRsinPsinY)   ( cosRcosP)   (sinRsinY - cosRsinPcosY)
  (-cosPsinY)                ( sinP)       (cosPcosY)


Xr  =  X*(cosRcosY -sinRsinPsinY) + Y*(-sinRcosP) + Z*(cosRsinY + sinRsinPcosY)
Yr  =  X*(sinRcosY +cosRsinPsinY) + Y*( cosRcosP) + Z*(sinRsinY - cosRsinPcosY)
Zr  =  X*(-cosPsinY) + Y*( sinP) + Z*(cosPcosY)

**************************************************************************************************/
/*static inline void rotate(float &X, float &Y, float &Z, float yaw, float pitch, float roll)
{
    float sinY = sin(yaw);
    float cosY = cos(yaw);
    float sinP = sin(pitch);
    float cosP = cos(pitch);
    float sinR = sin(roll);
    float cosR = cos(roll);

    X = X*(cosR*cosY - sinR*sinP*sinY) + Y*(-sinR*cosP) + Z*(cosR*sinY + sinR*sinP*cosY);
    Y = X*(sinR*cosY + cosR*sinP*sinY) + Y*(cosR*cosP) + Z*(sinR*sinY - cosR*sinP*cosY);
    Z = X*(-cosP*sinY) + Y*(sinP)+Z*(cosP*cosY);
}
*/
class rotMtx{
public:
    float m[3][3];
    void setAngles(float yaw, float pitch, float roll);
    void rotate(float &X, float &Y, float &Z);
};

void rotMtx::setAngles(float yaw, float pitch, float roll)
{
    float sinY = sin(yaw * (float)PI / 180);
    float cosY = cos(yaw * (float)PI / 180);
    float sinP = sin(pitch * (float)PI / 180);
    float cosP = cos(pitch * (float)PI / 180);
    float sinR = sin(roll * (float)PI / 180);
    float cosR = cos(roll * (float)PI / 180);

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

void rotMtx::rotate(float &X, float &Y, float &Z)
{
    float x = X;
    float y = Y;
    float z = Z;
    X = x*m[0][0] + y*m[0][1] + z*m[0][2];
    Y = x*m[1][0] + y*m[1][1] + z*m[1][2];
    Z = x*m[2][0] + y*m[2][1] + z*m[2][2];
}


/**************************************************************************************************
AmdTrueAudio::generateRoomResponse:

Generates an impulse response for a rectangular room, given room dimensions, damping factors for
each of the six walls, source and microphone positions in the room.

**************************************************************************************************/
void TrueAudioVRimpl::generateRoomResponse(
    const RoomDefinition & room,
    MonoSource sound,
    StereoListener & ears,
    int inSampRate,
    int responseLength,
    void *responseL,
    void *responseR,
    int flags,
    int maxBounces
    )
{
    if (sound.speakerX > room.width) sound.speakerX = room.width;
    if (sound.speakerX < 0) sound.speakerX = 0;
    if (sound.speakerY > room.height) sound.speakerY = room.height;
    if (sound.speakerY < 0) sound.speakerY = 0;
    if (sound.speakerZ > room.length) sound.speakerZ = room.length;
    if (sound.speakerZ < 0) sound.speakerZ = 0;

    //rotate head:
    float earDxL = ears.earSpacing / 2;
    float earDyL = 0.0;
    float earDzL = 0.0;
    float earDxR = -ears.earSpacing / 2;
    float earDyR = 0.0;
    float earDzR = 0.0;
    float earVxL = earDxL;
    float earVyL = 0.0;
    float earVzL = -earDxL;
    float earVxR = earDxR;
    float earVyR = 0.0;
    float earVzR = earDxR;

    rotMtx rotM;
    rotM.setAngles(ears.yaw, ears.pitch, ears.roll);
    rotM.rotate(earDxL, earDyL, earDzL);
    rotM.rotate(earDxR, earDyR, earDzR);
    rotM.rotate(earVxL, earVyL, earVzL);
    rotM.rotate(earVxR, earVyR, earVzR);

    int nSamplesW = int((room.width / S) * inSampRate);
    int nSamplesH = int((room.height / S) * inSampRate);
    int nSamplesL = int((room.length / S) * inSampRate);

    int nW = (1 + responseLength / nSamplesW);
    int nH = (1 + responseLength / nSamplesH);
    int nL = (1 + responseLength / nSamplesL);

    if (GENROOM_LIMIT_BOUNCES & flags) {
        nW = (nW > maxBounces) ? maxBounces : nW;
        nH = (nH > maxBounces) ? maxBounces : nH;
        nL = (nL > maxBounces) ? maxBounces : nL;
    }

    auto info = ES + "Computing "
        + std::to_string(nW) + " x "
        + std::to_string(nH) + " x "
        + std::to_string(nL) + " x = "
        + std::to_string(nW * nH * nL) + " reflections";
    PrintDebug(info);

    if (m_executionMode == VRExecutionMode::GPU)
    {
#ifndef TAN_NO_OPENCL
        if (!m_clInitialized)
        {
            InitializeCL(ears, 2 * nW, 2 * nH, 2 * nL, responseLength);
            m_clInitialized = true;
        }
#else
        if(!mInitialized)
        {
            InitializeAMF(ears, 2 * nW, 2 * nH, 2 * nL, responseLength);
            mInitialized = true;
        }
#endif
    }

    for (int chan = 0; chan < 2; chan++)
    {
        float *response = (float *)responseL;

#ifndef TAN_NO_OPENCL
        cl_mem oclResponse = (cl_mem)responseL;
#else
        AMFBuffer *amfResponse = (AMFBuffer *)responseL;
#endif

        float headX, headY, headZ;
        float earVX, earVY, earVZ;
        earVY = 0.0;
        headY = ears.headY;
        float vx = 0.0;

        switch (chan){
        case 0:
            response = (float *)responseL;

#ifndef TAN_NO_OPENCL
            oclResponse = (cl_mem)responseL;
#else
            amfResponse = (AMFBuffer *)responseL;
#endif

            headX = ears.headX + earDxL;
            headY = ears.headY + earDyL;
            headZ = ears.headZ + earDzL;
            earVX = earVxL;
            earVY = earVyL;
            earVZ = earVzL;
            break;
        case 1:
            response = (float *)responseR;

#ifndef TAN_NO_OPENCL
            oclResponse = (cl_mem)responseR;
#else
            amfResponse = (AMFBuffer *)responseR;
#endif

            headX = ears.headX + earDxR;
            headY = ears.headY + earDyR;
            headZ = ears.headZ + earDzR;
            earVX = earVxR;
            earVY = earVyR;
            earVZ = earVzR;

            break;
        }

        // arbitrary cutoff for applying HRTF to echos
        int hrtfResponseLength = 2 * (nSamplesW + nSamplesH + nSamplesL);

        if (m_executionMode == VRExecutionMode::GPU)
        {
            float earV = sqrtf(earVX*earVX + earVY*earVY + earVZ*earVZ);
            float maxGain = 2.0;
            float dMin = 2 * ears.earSpacing;
            int filterLength = ears.hrtf.filterLength;

            if(flags & GENROOM_USE_GPU_MEM)
            {
                generateRoomResponseGPU(
                    sound,
                    room,
#ifndef TAN_NO_OPENCL
                    oclResponse,
#else
                    amfResponse,
#endif
                    headX,
                    headY,
                    headZ,
                    earVX,
                    earVY,
                    earVZ,
                    earV,
                    maxGain,
                    dMin,
                    inSampRate,
                    responseLength,
                    hrtfResponseLength,
                    filterLength,
                    2 * nW,
                    2 * nH,
                    2 * nL
                    );
            }
            else
            {
                generateRoomResponseGPU(
                    sound,
                    room,
                    response,
                    headX,
                    headY,
                    headZ,
                    earVX,
                    earVY,
                    earVZ,
                    earV,
                    maxGain,
                    dMin,
                    inSampRate,
                    responseLength,
                    hrtfResponseLength,
                    filterLength,
                    2 * nW,
                    2 * nH,
                    2 * nL
                    );
            }
        }
        else
        {
            generateRoomResponseCPU(room, sound, ears.earSpacing, ears.hrtf, response, headX, headY, headZ, earVX, earVY, earVZ, inSampRate, responseLength, hrtfResponseLength, nW, nH, nL);
        }
    }
}

/**************************************************************************************************
AmdTrueAudio::generateRoomResponse:

Generates an impulse response for a rectangular room, given room dimensions, damping factors for
each of the six walls, source and microphone positions in the room.

**************************************************************************************************/
void TrueAudioVRimpl::generateDirectResponse(
    const RoomDefinition & room,
    MonoSource sound,
    const StereoListener & ears,
    int inSampRate,
    int responseLength,
    void *responseL,
    void *responseR,
    int *pFirstNZ,
    int *pLastNZ
    )
{
    if (sound.speakerX > room.width) sound.speakerX = room.width;
    if (sound.speakerX < 0) sound.speakerX = 0;
    if (sound.speakerY > room.height) sound.speakerY = room.height;
    if (sound.speakerY < 0) sound.speakerY = 0;
    if (sound.speakerZ > room.length) sound.speakerZ = room.length;
    if (sound.speakerZ < 0) sound.speakerZ = 0;

    //rotate head:
    float earDxL = ears.earSpacing / 2;
    float earDyL = 0.0;
    float earDzL = 0.0;
    float earDxR = -ears.earSpacing / 2;
    float earDyR = 0.0;
    float earDzR = 0.0;
    float earVxL = earDxL;
    float earVyL = 0.0;
    float earVzL = -earDxL;
    float earVxR = earDxR;
    float earVyR = 0.0;
    float earVzR = earDxR;

    rotMtx rotM;
    rotM.setAngles(ears.yaw, ears.pitch, ears.roll);
    rotM.rotate(earDxL, earDyL, earDzL);
    rotM.rotate(earDxR, earDyR, earDzR);
    rotM.rotate(earVxL, earVyL, earVzL);
    rotM.rotate(earVxR, earVyR, earVzR);

    for (int chan = 0; chan < 2; chan++)
    {
        float *response = (float *)responseL;

        float headX, headY, headZ;
        float earVX, earVY, earVZ;
        earVY = 0.0;
        headY = ears.headY;
        float vx = 0.0;

        switch (chan){
        case 0:
            response = (float *)responseL;
            headX = ears.headX + earDxL;
            headY = ears.headY + earDyL;
            headZ = ears.headZ + earDzL;
            earVX = earVxL;
            earVY = earVyL;
            earVZ = earVzL;
            break;
        case 1:
            response = (float *)responseR;
            headX = ears.headX + earDxR;
            headY = ears.headY + earDyR;
            headZ = ears.headZ + earDzR;
            earVX = earVxR;
            earVY = earVyR;
            earVZ = earVzR;

            break;
        }

        generateDirectResponseCPU(
            room,
            sound,
            ears.earSpacing,
            ears.hrtf,
            response,
            headX,
            headY,
            headZ,
            earVX,
            earVY,
            earVZ,
            inSampRate,
            responseLength,
            pFirstNZ,
            pLastNZ
            );
    }
}

AMF_RESULT TrueAudioVRimpl::Initialize(
    StereoListener & ears,
    int nW,
    int nH,
    int nL,
    int responseLength
    )
{
    mResponseLength = responseLength;
    mResponseBuffer.resize(responseLength);

	return AMF_OK;
}

#ifndef TAN_NO_OPENCL
AMF_RESULT TrueAudioVRimpl::InitializeCL(
    StereoListener & ears,
    int nW,
    int nH,
    int nL,
    int responseLength
    )
{
    //AmdTAlogger::logMessage(m_fpLog, "GenerateRoomResponseGPU");

    cl_int status = CL_SUCCESS;
    mResponseLength = responseLength;

    m_context = static_cast<cl_context>(mContext->GetOpenCLContext());

    // Compile kernel
    {
        size_t lengths[1] = { GenerateRoomResponseCount };
        const char *text = reinterpret_cast<const char*>(GenerateRoomResponse);
        cl_program program = clCreateProgramWithSource(m_context, 1, &text, lengths, &status);
        AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

        status = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
        AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

        m_kernel = clCreateKernel(program, "GenerateRoomResponse", &status);
        AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);
    }

    {
        // TODO: include the proper path
        size_t lengths[1] = { FillCount };
        const char *text = reinterpret_cast<const char*>(Fill);
        cl_program program = clCreateProgramWithSource(m_context, 1, &text, lengths, &status);
        AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

        status = clBuildProgram(program, 0, NULL, NULL, NULL, NULL);
        AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

        m_kernelFill = clCreateKernel(program, "Fill", &status);
        AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);
    }

    m_pResponse =  clCreateBuffer(m_context, CL_MEM_READ_WRITE, responseLength * sizeof(float), NULL, &status);
    AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

    m_pFloatResponse =  clCreateBuffer(m_context, CL_MEM_READ_WRITE, responseLength * sizeof(float), NULL, &status);

    // zero buffers
    float fill = 0.0;
    status = clEnqueueFillBuffer(m_cmdQueue, m_pResponse, &fill, sizeof(float), 0, responseLength * sizeof(float), 0, NULL, NULL);
    status = clEnqueueFillBuffer(m_cmdQueue, m_pFloatResponse, &fill, sizeof(float), 0, responseLength * sizeof(float), 0, NULL, NULL);

    //void *frMap = clEnqueueMapBuffer(m_cmdQueue, m_pFloatResponse, CL_TRUE, CL_MAP_READ, 0, responseLength * sizeof(float), 0, NULL, NULL, &status);

    // TODO: log errors
    AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

    //TODO: combine the LPF and HPF into one chunk of memory
    m_pHPF = clCreateBuffer(m_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        HeadFilterSize * sizeof(float), ears.hrtf.highPass, &status);

    AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

    m_pLPF = clCreateBuffer(m_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
        HeadFilterSize * sizeof(float), ears.hrtf.lowPass, &status);

    AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

    m_globalWorkSize[0] = RoundUp(localX, nW);
    m_globalWorkSize[1] = RoundUp(localY, nH);
    m_globalWorkSize[2] = RoundUp(localZ, nL);

    m_globaSizeFill = RoundUp(responseLength / 4, localSizeFill);

    return AMF_OK;
}
#else
AMF_RESULT TrueAudioVRimpl::InitializeAMF(
	StereoListener & ears,
    int nW,
    int nH,
    int nL,
    int responseLength
    )
{
	if(!mFactory)
	{
		AMF_RETURN_IF_FAILED(g_AMFFactory.Init());

		mFactory = g_AMFFactory.GetFactory();
        mFactoryCreated = true;
	}

    mResponseLength = responseLength;

    AMF_RETURN_IF_FALSE(
        GetOclKernel(
            mKernelResponse,
            mCompute,

            "GenerateRoomResponse",
            (const char *)GenerateRoomResponse,
            GenerateRoomResponseCount,
            "GenerateRoomResponse",

            "",
            mFactory
            ),
        AMF_FAIL
        );

    AMF_RETURN_IF_FALSE(
        GetOclKernel(
            mKernelFill,
            mCompute,

            "Fill",
            (const char *)Fill,
            FillCount,
            "Fill",

            "",
            mFactory
            ),
        AMF_FAIL
        );

    AMF_RETURN_IF_FAILED(
        mContext->GetAMFContext()->AllocBuffer(
#ifndef USE_METAL
            amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
            amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
            ,
            responseLength * sizeof(float),
            &mResponse
            )
        );

    AMF_RETURN_IF_FAILED(
        mContext->GetAMFContext()->AllocBuffer(
#ifndef USE_METAL
            amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
            amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
            ,
            responseLength * sizeof(float),
            &mFloatResponse
            )
        );

    // zero buffers
    float fill = 0.0;

    AMF_RETURN_IF_FAILED(
        mCompute->FillBuffer(
            mResponse,
            0,
            responseLength * sizeof(float),
            &fill,
            sizeof(float)
            )
        );
    AMF_RETURN_IF_FAILED(
        mCompute->FillBuffer(
            mFloatResponse,
            0,
            responseLength * sizeof(float),
            &fill,
            sizeof(float)
            )
        );

    //TODO: combine the LPF and HPF into one chunk of memory
    //m_pHPF = clCreateBuffer(m_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
    //    HeadFilterSize * sizeof(float), ears.hrtf.highPass, &status);
    AMF_RETURN_IF_FAILED(
        mContext->GetAMFContext()->CreateBufferFromHostNative(
            ears.hrtf.highPass,
            HeadFilterSize * sizeof(float),
            &mHPF,
            nullptr
            )
        );
    AMF_RETURN_IF_FAILED(mHPF->Convert(
#ifndef USE_METAL
        amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
        amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
        ));

    //m_pLPF = clCreateBuffer(m_context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
    //    HeadFilterSize * sizeof(float), ears.hrtf.lowPass, &status);
    AMF_RETURN_IF_FAILED(
        mContext->GetAMFContext()->CreateBufferFromHostNative(
            ears.hrtf.lowPass,
            HeadFilterSize * sizeof(float),
            &mLPF,
            nullptr
            )
        );
    AMF_RETURN_IF_FAILED(mLPF->Convert(
#ifndef USE_METAL
        amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL
#else
        amf::AMF_MEMORY_TYPE::AMF_MEMORY_METAL
#endif
        ));

    m_globalWorkSize[0] = RoundUp(localX, nW);
    m_globalWorkSize[1] = RoundUp(localY, nH);
    m_globalWorkSize[2] = RoundUp(localZ, nL);

    m_globaSizeFill = RoundUp(responseLength / 4, localSizeFill);

	return AMF_OK;
}
#endif

#ifndef TAN_NO_OPENCL
AMF_RESULT TrueAudioVRimpl::generateRoomResponseGPU(
    const MonoSource & sound,
    const RoomDefinition & room,
    cl_mem floatResponse,
    float headX,
    float headY,
    float headZ,
    float earVX,
    float earVY,
    float earVZ,
    float earV,
    float maxGain,
    float dMin,
    int inSampRate,
    int responseLength,
    int hrtfResponseLength,
    int filterLength,
    int nW,
    int nH,
    int nL
    )
{
    //Set kernel arguments
    //TODO: pass parameters as structures
    int argIdx = 0;
    int status = 0;
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(cl_mem), &m_pResponse);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(cl_mem), &m_pHPF);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(cl_mem), &m_pLPF);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &sound.speakerX);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &sound.speakerY);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &sound.speakerZ);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &headX);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &headY);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &headZ);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &earVX);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &earVY);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &earVZ);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &earV);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.width);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.length);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.height);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.mRight.damp);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.mLeft.damp);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.mFront.damp);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.mBack.damp);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.mTop.damp);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &room.mBottom.damp);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &maxGain);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(float), &dMin);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &inSampRate);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &responseLength);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &hrtfResponseLength);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &filterLength);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &nW);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &nH);
    status |= clSetKernelArg(m_kernel, argIdx++, sizeof(int), &nL);

    size_t localWorkSize[3] = { localX, localY, localZ };

    status = clEnqueueNDRangeKernel(m_cmdQueue, m_kernel, 3, NULL, m_globalWorkSize, localWorkSize, 0, NULL, NULL);

    AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);

    //convert the response buffer
     size_t localSize = localSizeFill;
    status = clSetKernelArg(m_kernelFill, 0, sizeof(cl_mem), &m_pResponse);
    status |= clSetKernelArg(m_kernelFill, 1, sizeof(cl_mem), &floatResponse);
    status |= clEnqueueNDRangeKernel(m_cmdQueue, m_kernelFill, 1, NULL, &m_globaSizeFill, &localSize, 0, NULL, NULL);

    AMF_RETURN_IF_FALSE(status == CL_SUCCESS, AMF_FAIL);
}
#else
AMF_RESULT TrueAudioVRimpl::generateRoomResponseGPU(
    const MonoSource & sound,
    const RoomDefinition & room,
    AMFBuffer * floatResponse,
    float headX,
    float headY,
    float headZ,
    float earVX,
    float earVY,
    float earVZ,
    float earV,
    float maxGain,
    float dMin,
    int inSampRate,
    int responseLength,
    int hrtfResponseLength,
    int filterLength,
    int nW,
    int nH,
    int nL
    )
{
    size_t localWorkSize[3] = { localX, localY * localZ, 1 };

    std::cout << "m_globalWorkSize: {"
        << m_globalWorkSize[0] << ", "
        << m_globalWorkSize[1] << ", "
        << m_globalWorkSize[2] << "};"
        << std::endl;
    std::cout << "localWorkSize: {"
        << localWorkSize[0] << ", "
        << localWorkSize[1] << ", "
        << localWorkSize[2] << "};"
        << std::endl;

    auto before1(m_globalWorkSize[1]);
    auto before2(m_globalWorkSize[2]);
    m_globalWorkSize[1] *= m_globalWorkSize[2];
    m_globalWorkSize[2] = 1;

    //Set kernel arguments
    //TODO: pass parameters as structures
    int argIdx = 0;
    int status = 0;
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgBuffer(argIdx++, mResponse, AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgBuffer(argIdx++, mHPF, AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgBuffer(argIdx++, mLPF, AMF_ARGUMENT_ACCESS_READWRITE));

    //AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, before2));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, sound.speakerX));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, sound.speakerY));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, sound.speakerZ));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, headX));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, headY));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, headZ));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, earVX));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, earVY));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, earVZ));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, earV));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.width));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.length));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.height));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.mRight.damp));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.mLeft.damp));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.mFront.damp));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.mBack.damp));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.mTop.damp));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, room.mBottom.damp));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, maxGain));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgFloat(argIdx++, dMin));

    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, inSampRate));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, responseLength));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, hrtfResponseLength));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, filterLength));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, nW));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, nH));
    AMF_RETURN_IF_FAILED(mKernelResponse->SetArgInt32(argIdx++, nL));

    AMF_RETURN_IF_FAILED(
        mKernelResponse->Enqueue(2, nullptr, m_globalWorkSize, localWorkSize)
        );

    //convert the response buffer
    size_t localSize = localSizeFill;

    mKernelFill->SetArgBuffer(0, mResponse, AMF_ARGUMENT_ACCESS_READWRITE);
    mKernelFill->SetArgBuffer(1, floatResponse, AMF_ARGUMENT_ACCESS_READWRITE);

    AMF_RETURN_IF_FAILED(
        mKernelFill->Enqueue(1, nullptr, &m_globaSizeFill, &localSize)
        );

    AMF_RETURN_IF_FAILED(mCompute->FinishQueue());

    return AMF_OK;
}
#endif

AMF_RESULT TrueAudioVRimpl::generateRoomResponseGPU(
    const MonoSource & sound,
    const RoomDefinition & room,
    float* response,
    float headX,
    float headY,
    float headZ,
    float earVX,
    float earVY,
    float earVZ,
    float earV,
    float maxGain,
    float dMin,
    int inSampRate,
    int responseLength,
    int hrtfResponseLength,
    int filterLength,
    int nW,
    int nH,
    int nL
    )
{
    generateRoomResponseGPU(
        sound,
        room,
#ifndef TAN_NO_OPENCL
        m_pFloatResponse,
#else
        mFloatResponse,
#endif
        headX,
        headY,
        headZ,
        earVX,
        earVY,
        earVZ,
        earV,
        maxGain,
        dMin,
        inSampRate,
        responseLength,
        hrtfResponseLength,
        filterLength,
        nW,
        nH,
        nL
        );

#ifndef TAN_NO_OPENCL
    //ToDO fix this case -- returns empty buffer:
    int status = clEnqueueReadBuffer(
        m_cmdQueue,
        m_pFloatResponse,
        CL_TRUE,
        0,
        responseLength * sizeof(float),
        response,
        0,
        nullptr,
        nullptr
        );

    clFinish(m_cmdQueue);
#else
    mCompute->CopyBufferToHost(
        mFloatResponse,
        0,
        responseLength * sizeof(float),
        response,
        true
        );

    mCompute->FinishQueue();
#endif

	return AMF_OK;
}

/**************************************************************************************************
AmdTrueAudio::generateRoomResponse:

Generates an impulse response for a rectangular room, given room dimensions, damping factors for
each of the six walls, source and microphone positions in the room.

**************************************************************************************************/

void TrueAudioVRimpl::generateRoomResponseCPU(
    const RoomDefinition & room,
    const MonoSource & sound,
    float earSpacing,
    const HeadModel & hrtf,
    float* response,
    float headX,
    float headY,
    float headZ,
    float earVX,
    float earVY,
    float earVZ,
    int inSampRate,
    int responseLength,
    int hrtfResponseLength,
    int nW,
    int nH,
    int nL,
    int flags
    )
{
//#pragma omp parallel  for default(none) shared(flags, hrtf, earSpacing, sound, inSampRate, responseLength, room, nW, nH, nL, mid, incy,responseL,responseR,earDxL, earDxR, earDyL, earDyR, earDzL, earDzR,earDxL, earVxR, earVyL, earVyR, earVzL, earVzR) num_threads(2)

    hrtfResponseLength = hrtfResponseLength > responseLength ? responseLength : hrtfResponseLength;

    float dx = sound.speakerX - headX;
    float dy = sound.speakerY - headY;
    float dz = sound.speakerZ - headZ;

    float maxGain = 2.0;
    float d0 = sqrtf(dx*dx + dy*dy + dz*dz);
    float dMin = 2 * earSpacing;
    int r0 = 1 + (int)((d0 / S) * inSampRate);
    float dr;
    if (d0 <= dMin)
        dr = maxGain;
    else
        dr = maxGain*dMin / d0;


    float W = room.width;
    float H = room.height;
    float L = room.length;
    float Sx = sound.speakerX;
    float Sy = sound.speakerY;
    float Sz = sound.speakerZ;
    float dampRight = room.mRight.damp;
    float dampLeft = room.mLeft.damp;
    float dampTop = room.mTop.damp;
    float dampBottom = room.mBottom.damp;
    float dampFront = room.mFront.damp;
    float dampBack = room.mBack.damp;

    float x, y, z;
    //float dx, dy, dz;
    for (int iz = -nL; iz < nL; iz++) {
        for (int iy = -nH; iy < nH; iy++) {
            for (int ix = -nW; ix < nW; ix++) {

                if ((flags & GENROOM_SUPPRESS_DIRECT) && (ix == 0) && (iy == 0) && (iz == 0)){
                    continue;
                }

                x = Sx;
                y = Sy;
                z = Sz;

                if (ix & 1) {
                    x = W - x;
                }
                if (iy & 1) {
                    y = H - y;

                }
                if (iz & 1) {
                    z = L - z;
                }

                x += ix*W;
                y += iy*H;
                z += iz*L;

                dx = x - headX;
                dy = y - headY;
                dz = z - headZ;

                float d = sqrtf(dx*dx + dy*dy + dz*dz);
                int ridx = 1 + (int)((d / S) * inSampRate);

                if (d <= dMin)
                    dr = maxGain;
                else
                    dr = maxGain*(dMin / d);

                    float nP, nN;
                    nP = nN = 0.0;
                    if (ix > 0) {
                        nP = (ix + 1) / 2.f;
                        nN = (ix) / 2.f;
                    }
                    else if (ix < 0){
                        nP = abs(ix) / 2.f;
                        nN = abs(ix - 1) / 2.f;
                    }
                    float amp = powf(dampRight, nP) *powf(dampLeft, nN);
                    if (iy > 0) {
                        nP = (iy + 1) / 2.f;
                        nN = (iy) / 2.f;
                    }
                    else if (iy < 0){
                        nP = abs(iy) / 2.f;
                        nN = abs(iy - 1) / 2.f;
                    }
                    amp *= powf(dampTop, nP) *powf(dampBottom, nN);
                    if (iz > 0) {
                        nP = (iz + 1) / 2.f;
                        nN = (iz) / 2.f;
                    }
                    else if (iz < 0){
                        nP = abs(iz) / 2.f;
                        nN = abs(iz - 1) / 2.f;
                    }
                    amp *= powf(dampFront, nP) *powf(dampBack, nN);

                if (ridx < hrtfResponseLength){
                    applyHRTF(hrtf, amp*dr, &response[ridx], responseLength - ridx, earVX, earVY, earVZ, dx, dy, dz);
                }
                else if (ridx < responseLength){
                    response[ridx] += amp*dr;
                }

            }
        }
    }
}



void TrueAudioVRimpl::generateDirectResponseCPU(
    const RoomDefinition & room,
    const MonoSource & sound,
    float earSpacing,
    const HeadModel & hrtf,
    float* response,
    float headX,
    float headY,
    float headZ,
    float earVX,
    float earVY,
    float earVZ,
    int inSampRate,
    int responseLength,
    int *firstNonZero,
    int *lastNonZero
    )
{
    float dx = sound.speakerX - headX;
    float dy = sound.speakerY - headY;
    float dz = sound.speakerZ - headZ;

    float maxGain = 2.0;
    float d0 = sqrtf(dx*dx + dy*dy + dz*dz);
    float dMin = 2 * earSpacing;
    int r0 = 1 + (int)((d0 / S) * inSampRate);
    float dr;
    if (d0 <= dMin)
        dr = maxGain;
    else
        dr = maxGain*dMin / d0;

    float W = room.width;
    float H = room.height;
    float L = room.length;
    float Sx = sound.speakerX;
    float Sy = sound.speakerY;
    float Sz = sound.speakerZ;
    float dampRight = room.mRight.damp;
    float dampLeft = room.mLeft.damp;
    float dampTop = room.mTop.damp;
    float dampBottom = room.mBottom.damp;
    float dampFront = room.mFront.damp;
    float dampBack = room.mBack.damp;

    float x, y, z;
    //float dx, dy, dz;


    x = Sx;
    y = Sy;
    z = Sz;


    dx = x - headX;
    dy = y - headY;
    dz = z - headZ;

    float d = sqrtf(dx*dx + dy*dy + dz*dz);
    int ridx = 1 + (int)((d / S) * inSampRate);

    if (d <= dMin)
        dr = maxGain;
    else
        dr = maxGain*(dMin / d);

    float nP, nN;
    nP = nN = 0.0;

    if (ridx < *firstNonZero)
        *firstNonZero = ridx;
    if (ridx + hrtf.filterLength > *lastNonZero)
        *lastNonZero = ridx + hrtf.filterLength;

    if (ridx < responseLength){
        applyHRTF(hrtf, dr, &response[ridx], responseLength - ridx, earVX, earVY, earVZ, dx, dy, dz);
    }
}

// ToDo remove for distro
#ifdef DOORWAY_TRANSFORM
void TrueAudioVRimpl::generateDoorwayResponse(
    const RoomDefinition & room1,
    const RoomDefinition & room2,
    const MonoSource & source,
    const Door & door,
    StereoListener & ear,
    int inSampRate,
    int responseLength,
    float *responseLeft,
    float *responseRight,
    int flags,
    int maxBounces
    )
{
    int nSamplesW = int((room1.width / S) * inSampRate);
    int nSamplesH = int((room1.height / S) * inSampRate);
    int nSamplesL = int((room1.length / S) * inSampRate);

    int nW = (1 + responseLength / nSamplesW);
    int nH = (1 + responseLength / nSamplesH);
    int nL = (1 + responseLength / nSamplesL);

    if (GENROOM_LIMIT_BOUNCES & flags) {
        nW = (nW > maxBounces) ? maxBounces : nW;
        nH = (nH > maxBounces) ? maxBounces : nH;
        nL = (nL > maxBounces) ? maxBounces : nL;
    }

    if (responseLength > mResponseLength)
        responseLength = mResponseLength;

    memset(m_pResponseBuffer, 0, mResponseLength*sizeof(float));

    // no hrtf for virtual source, so hrtfResponseLength = 0. :
    //ToDo: apply doorway specific filter ???
    generateRoomResponseCPU(room1, source, ear.earSpacing, &ear.hrtf, m_pResponseBuffer, door.r1cX, door.r1cY, door.r1cZ, 0., 0., 0., inSampRate, responseLength, 0., nW, nH, nL, GENROOM_SUPPRESS_DIRECT);

    // ToDo: attenuation factor ???
    generateDoorwayDirectResponse(room1, room2, source, door, ear, inSampRate, responseLength, m_pResponseBuffer, responseLeft, responseRight, 0);

    // HRTF stereo:
    float *hrtfbuf = m_pResponseBuffer;


    //rotate head:
    float earDxL = ear.earSpacing / 2;
    float earDyL = 0.0;
    float earDzL = 0.0;
    float earDxR = -ear.earSpacing / 2;
    float earDyR = 0.0;
    float earDzR = 0.0;
    float earVxL = earDxL;
    float earVyL = 0.0;
    float earVzL = -earDxL;
    float earVxR = earDxR;
    float earVyR = 0.0;
    float earVzR = earDxR;

    rotMtx rotM;
    rotM.setAngles(ear.yaw, ear.pitch, ear.roll);
    rotM.rotate(earDxL, earDyL, earDzL);
    rotM.rotate(earDxR, earDyR, earDzR);
    rotM.rotate(earVxL, earVyL, earVzL);
    rotM.rotate(earVxR, earVyR, earVzR);

    float dx = door.r1cX - ear.headX;
    float dy = door.r1cY - ear.headY;
    float dz = door.r1cZ - ear.headZ;

     int len = 1;
    int log2len = 0;
    // use next bigger power of 2:
    while (len < responseLength){
        len <<= 1;
        ++log2len;
    }

   // HRTF left
    float dxL = dx + earDxL;
    float dyL = dy + earDyL;
    float dzL = dz + earDzL;
    float dxR = dx + earDxR;
    float dyR = dy + earDyR;
    float dzR = dz + earDzR;

    memset(hrtfbuf, 0, responseLength*sizeof(float));
    applyHRTF(&ear.hrtf, 1.0, m_pResponseBuffer, responseLength, earVxL, earVyL, earVzL, dxL, dyL, dzL);
    // combine responses:
    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &m_pResponseBuffer, &m_pResponseBuffer);
    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &responseLeft, &responseLeft);

    m_pMath->ComplexMultiplication(&m_pResponseBuffer, &responseLeft, &responseLeft, 1, responseLength);

    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_BACKWARD, log2len, 1, &responseLeft, &responseLeft);

    // HRTF right
    memset(hrtfbuf, 0, responseLength*sizeof(float));
    applyHRTF(&ear.hrtf, 1.0, m_pResponseBuffer, responseLength, earVxR, earVyR, earVzR, dxR, dyR, dzR);
    // combine responses:
    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &m_pResponseBuffer, &m_pResponseBuffer);
    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &responseRight, &responseRight);

    m_pMath->ComplexMultiplication(&m_pResponseBuffer, &responseRight, &responseRight, 1, responseLength);

    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_BACKWARD, log2len, 1, &responseRight, &responseRight);

    /*
    MonoSource vSource;
    vSource.speakerX = door.r2cX;
    vSource.speakerY = door.r2cY;
    vSource.speakerZ = door.r2cZ;

    generateRoomResponseCPU(room1, vSource, ear.earSpacing, &ear.hrtf, response2, ear.headX, ear.headY, ear.headZ, 0., 0., 0., inSampRate, responseLength, 0., nW, nH, nL, GENROOM_SUPPRESS_DIRECT);

    // combine responses:
    int log2len = 0;
    int len = 1;
    // use next bigger power of 2:
    while (len < responseLength){
        len <<= 1;
        ++log2len;
    }
    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &response1, &response1);
    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_FORWARD, log2len, 1, &response2, &response2);

    m_pMath->ComplexMultiplication( &response1, &response2, &response2, 1, responseLength);

    mFFT->Transform(TAN_FFT_TRANSFORM_DIRECTION_BACKWARD, log2len, 1, &response2, &response2);
    */
}


// source in room1, ear in room2, door in both:
void  TrueAudioVRimpl::generateDoorwayDirectResponse(
    const RoomDefinition & room1,
    const RoomDefinition & room2,
    const MonoSource & source,
    const Door & door,
    StereoListener & ear,
    int inSampRate,
    int responseLength,
    float *responseIn,
    float *responseLeft,
    float *responseRight,
    int flags
    )
{
    float maxGain = 1.0;
    float dMin = 1.0;

    //rotate head:
    float earDxL = ear.earSpacing / 2;
    float earDyL = 0.0;
    float earDzL = 0.0;
    float earDxR = -ear.earSpacing / 2;
    float earDyR = 0.0;
    float earDzR = 0.0;

    rotMtx rotM;
    rotM.setAngles(ear.yaw, ear.pitch, ear.roll);
    rotM.rotate(earDxL, earDyL, earDzL);
    rotM.rotate(earDxR, earDyR, earDzR);

    float dxL = ear.headX - door.r2cX + earDxL;
    float dyL = ear.headY - door.r2cY + earDyL;
    float dzL = ear.headZ - door.r2cZ + earDzL;

    float dxR = ear.headX - door.r2cX + earDxR;
    float dyR = ear.headY - door.r2cY + earDyR;
    float dzR = ear.headZ - door.r2cZ + earDzR;

    // add room1 one reverb to room2 shifted by distance to ear in room2 coords:
    {

        float dL = sqrt((dxL*dxL) + (dyL*dyL) + (dzL*dzL));
        float dR = sqrt((dxR*dxR) + (dyR*dyR) + (dzR*dzR));

        int shiftL = 1 + (int)((dL / S) * inSampRate);
        int shiftR = 1 + (int)((dR / S) * inSampRate);

        float dr;
        if (dL <= dMin)
            dr = maxGain;
        else
            dr = maxGain*(dMin / dL);

        for (int ridx = shiftL; ridx < responseLength; ridx++){
            responseLeft[ridx] += dr*responseIn[ridx - shiftL];
        }
        if (dR <= dMin)
            dr = maxGain;
        else
            dr = maxGain*(dMin / dR);

        for (int ridx = shiftR; ridx < responseLength; ridx++){
            responseRight[ridx] += dr*responseIn[ridx - shiftR];
        }
    }

    // add in direct path
    // planar rectangular doorway defined by center and two bottom corners:

    //
    // doorway definition in room 1:
    float r1blX = door.r1blX; // bottom left corner
    float r1blY = door.r1blY;
    float r1blZ = door.r1blZ;

    float r1brX = door.r1brX; // bottom right corner
    float r1brY = door.r1brY;
    float r1brZ = door.r1brZ;

    float r1tlX = r1brX + 2 * (door.r1cX - r1brX); // top left corner
    float r1tlY = r1brY + 2 * (door.r1cY - r1brY);
    float r1tlZ = r1brZ + 2 * (door.r1cZ - r1brZ);

    float r1trX = r1blX + 2 * (door.r1cX - r1blX); //top right corner
    float r1trY = r1blY + 2 * (door.r1cY - r1blY);
    float r1trZ = r1blZ + 2 * (door.r1cZ - r1blZ);

    // number of virtual sources vertically:
    float vx1 = r1tlX - r1blX;
    float vy1 = r1tlY - r1blY;
    float vz1 = r1tlZ - r1blZ;

    float v1 = sqrt(vx1*vx1 + vy1*vy1 + vz1*vz1);
    int nv = v1 / DOOR_VS_SPACING;
    float dxv1 = vx1 / nv;
    float dyv1 = vy1 / nv;
    float dzv1 = vz1 / nv;

    // number of virtual sources horizontally:
    float hx1 = r1brX - r1blX;
    float hy1 = r1brY - r1blY;
    float hz1 = r1brZ - r1blZ;

    float h1 = sqrt(hx1*hx1 + hy1*hy1 + hz1*hz1);
    int nh = h1 / DOOR_VS_SPACING;
    float dxh1 = hx1 / nv;
    float dyh1 = hy1 / nv;
    float dzh1 = hz1 / nv;

    float sx = source.speakerX;
    float sy = source.speakerY;
    float sz = source.speakerZ;

    float scale = 1.0 / (nv * nh);
    maxGain *= scale; // ???

    //
    // doorway definition in room 2:
    float r2blX = door.r2blX; // bottom left corner
    float r2blY = door.r2blY;
    float r2blZ = door.r2blZ;

    float r2brX = door.r2brX; // bottom right corner
    float r2brY = door.r2brY;
    float r2brZ = door.r2brZ;

    float r2tlX = r2brX + 2 * (door.r2cX - r2brX); // top left corner
    float r2tlY = r2brY + 2 * (door.r2cY - r2brY);
    float r2tlZ = r2brZ + 2 * (door.r2cZ - r2brZ);

    float r2trX = r2blX + 2 * (door.r2cX - r2blX); //top right corner
    float r2trY = r2blY + 2 * (door.r2cY - r2blY);
    float r2trZ = r2blZ + 2 * (door.r2cZ - r2blZ);

    // number of virtual sources vertically:
    float vx2 = r2tlX - r2blX;
    float vy2 = r2tlY - r2blY;
    float vz2 = r2tlZ - r2blZ;

    float v2 = sqrt(vx2*vx2 + vy2*vy2 + vz2*vz2);
    int nv2 = v2 / DOOR_VS_SPACING;
    float dxv2 = vx2 / nv;
    float dyv2 = vy2 / nv;
    float dzv2 = vz2 / nv;

    // number of virtual sources horizontally:
    float hx2 = r2brX - r2blX;
    float hy2 = r2brY - r2blY;
    float hz2 = r2brZ - r2blZ;

    float h2 = sqrt(hx2*hx2 + hy2*hy2 + hz2*hz2);
    int nh2 = h2 / DOOR_VS_SPACING;
    float dxh2 = hx2 / nv;
    float dyh2 = hy2 / nv;
    float dzh2 = hz2 / nv;

    float lx = ear.headX;
    float ly = ear.headY;
    float lz = ear.headZ;

    for (int j = 0; j < nv; j++){
        float x1 = r1blX + j*dxv1;
        float y1 = r1blY + j*dyv1;
        float z1 = r1blZ + j*dzv1;

        float x2 = r2blX + j*dxv2;
        float y2 = r2blY + j*dyv2;
        float z2 = r2blZ + j*dzv2;

        for (int i = 0; i < nh; i++){
            // room1 coordinate of virtual source in doorway:
            x1 += dxh1;
            y1 += dyh1;
            z1 += dzh1;

            x2 += dxh2;
            y2 += dyh2;
            z2 += dzh2;

            float dx = sx = x1;
            float dy = sy - y1;
            float dz = sz - z1;
            // distance to real source from virtual source:
            float d1 = sqrt(dx*dx + dy*dy + dz*dz);

            dx = lx + earDxL - x2;
            dy = ly + earDyL - y2;
            dz = lz + earDzL - z2;

            // distance  from virtual source to listener (left ear):
            float d2 = sqrt(dx*dx + dy*dy + dz*dz);

            // add to impulse response:
            int ridxL = 1 + (int)(((d2 + d1) / S) * inSampRate);

            float dr; //damping due to radial distance.
            if (d2 <= dMin)
                dr = maxGain;
            else
                dr = maxGain*(dMin / d2);

            if (ridxL < responseLength){
                responseLeft[ridxL] += dr;
            }

            dx = lx + earDxR - x2;
            dy = ly + earDyR - y2;
            dz = lz + earDzR - z2;

            // distance  from virtual source to listener(right ear):
            d2 = sqrt(dx*dx + dy*dy + dz*dz);

            // add to impulse response:
            int ridxR = 1 + (int)(((d2 + d1) / S) * inSampRate);

            if (d2 <= dMin)
                dr = maxGain;
            else
                dr = maxGain*(dMin / d2);

            if (ridxL < responseLength){
                responseRight[ridxR] += dr;
            }

        }
    }
}
#endif // DOORWAY_TRANSFORM
