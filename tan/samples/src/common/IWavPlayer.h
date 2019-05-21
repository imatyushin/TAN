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

#define ONE_SEC_BUFFER_SIZE   (10000000)
#define MS500_BUFFER_SIZE      (5000000)
#define MS100_BUFFER_SIZE      (1000000)
#define FIFTY_MS_BUFFER_SIZE    (500000)
#define SIXTH_SEC_BUFFER_SIZE  (10000000/6) //@48000/6 = 8000 samples
#define BUFFER_SIZE_8K	       (1706667)
#define ONE_SEC               (1000)
#define FAILONERROR(hr, msg) { if (FAILED(hr)) {std::cerr << msg << std::endl; return hr;} }
#define LOGERROR(hr, msg) { if (FAILED(hr)) {std::cerr << msg << std::endl;} }
#define SAFE_RELEASE(pointer) if(pointer){pointer->Release(); pointer = NULL;}

/*Structure which has all elements related to stream */
typedef struct streaminfo
{
    /* Number of channels 1=Mono 2=Stereo   */
    uint16_t NumOfChannels;
    /* Sampling Frequency in Hz   */
    uint32_t SamplesPerSec;
    /*Num of bytes Decoded */
    uint32_t DecodedBytes;
    /*Num of bits per sample for all channels*/
    int32_t bitsPerSample;
} STREAMINFO;

enum class QueueErrors
{
    OK = 0,

    FileNotFound = 1 << 1
};

/*Structure which hold elements required for wasapi playback */
struct IWavPlayer
{
    virtual ~IWavPlayer() {};//wasapiRelease();
    virtual QueueErrors QueueWaveFile(char *inFile,long *pNsamples, unsigned char **ppOutBuffer) = 0;
    virtual int32_t Record( unsigned char *pOutputBuffer, unsigned int size) = 0;
    virtual int32_t Play(unsigned char *pOutputBuffer, unsigned int size, bool mute) = 0;
};
