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

#ifndef _WASAPI_H_
#define _WASAPI_H_

#include "IWavPlayer.h"

#include <Windows.h>
//#include <VersionHelpers.h>
#include <iostream>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <mmsystem.h>
#include <Audioclient.h>
#include <endpointvolume.h>

/*Structure which hold elements required for wasapi playback */
class WASAPIUtils:
  public IWavPlayer
{
public:
    WASAPIUtils();
    ~WASAPIUtils();
    IMMDevice *devRender;
    IMMDevice *devCapture;
    IMMDeviceEnumerator *devEnum;
    IAudioClient *audioClient;
    IAudioRenderClient *renderClient;
    IAudioClient *audioCapClient;
    IAudioCaptureClient *captureClient;
    UINT frameSize;
    UINT bufferSize;
    bool mStartedRender;
    bool mStartedCapture;
    bool mInitializedRender;
    bool mInitializedCapture;

    WavError ReadWaveFile(const char *inFile, long *pNsamples, unsigned char **ppOutBuffer);
    int32_t Record(unsigned char *pOutputBuffer, unsigned int size);
    int32_t Play(unsigned char *pOutputBuffer, unsigned int size, bool mute);

    int wasapiInit(STREAMINFO *streaminfo, UINT *bufferSize, UINT *frameSize, AUDCLNT_SHAREMODE sharMode, bool capture = false);
    void wasapiRelease();
    bool PlayQueuedStreamChunk(bool init, long sampleCount, unsigned char *pOutBuffer );
};

#endif /* _WASAPI_H_ */

/*End Of File */
