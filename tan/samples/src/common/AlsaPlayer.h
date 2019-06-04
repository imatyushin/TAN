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

#include "IWavPlayer.h"
#include <iostream>

#include <alsa/asoundlib.h>

class AlsaPlayer:
    public IWavPlayer
{
protected:
    snd_pcm_t *mPCMHandle;
    uint32_t mUpdatePeriod;
    uint8_t mChannelsCount;

public:
    AlsaPlayer();
    virtual ~AlsaPlayer();

    WavError Init(const STREAMINFO *streaminfo, uint32_t *bufferSize, uint32_t *frameSize, bool capture = false);
    void Release();

    WavError ReadWaveFile(const std::string& fileName, long *pNsamples, unsigned char **ppOutBuffer);

    uint32_t Record(unsigned char *pOutputBuffer, unsigned int size);
    uint32_t Play(unsigned char *pOutputBuffer, unsigned int size, bool mute);
};