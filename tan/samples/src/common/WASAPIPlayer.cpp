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

#include "WASAPIPlayer.h"
#include "wav.h"

#define MAXFILES 100
int muteInit = 1;
IAudioEndpointVolume *g_pEndptVol = NULL;

WASAPIPlayer::WASAPIPlayer()
{
    mStartedRender = false;
    mStartedCapture = false;
    mInitializedRender = false;
    mInitializedCapture = false;
    devRender = devCapture = NULL;

    devRender = NULL;
    devCapture = NULL;
    devEnum = NULL;
    audioClient = NULL;
    renderClient = NULL;
    audioCapClient = NULL;
    captureClient = NULL;
}

WASAPIPlayer::~WASAPIPlayer()
{
    Close();
}

PlayerError WASAPIPlayer::Init
(
    uint16_t    channelsCount,
    uint16_t    bitsPerSample,
    uint32_t    samplesPerSecond,
    bool        play,
    bool        record
)
{
    HRESULT hr;

    REFERENCE_TIME bufferDuration = (BUFFER_SIZE_8K);//(SIXTH_SEC_BUFFER_SIZE); //(MS100_BUFFER_SIZE); // (ONE_SEC_BUFFER_SIZE);

    WAVEFORMATEXTENSIBLE mixFormat;

    /* PCM audio */
    mixFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;

    /* Default is 44100 */
    mixFormat.Format.nSamplesPerSec = samplesPerSecond;

    /* Default is 2 channels */
    mixFormat.Format.nChannels = (WORD) channelsCount;

    /* Default is 16 bit */
    mixFormat.Format.wBitsPerSample = (WORD) bitsPerSample;

    mixFormat.Format.cbSize = sizeof(mixFormat) - sizeof(WAVEFORMATEX);

    /* nChannels * bitsPerSample / BitsPerByte (8) = 4 */
    mixFormat.Format.nBlockAlign = (WORD)(channelsCount * bitsPerSample / 8);

    /* samples per sec * blockAllign */
    mixFormat.Format.nAvgBytesPerSec = samplesPerSecond * (channelsCount * bitsPerSample / 8);

    mixFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    mixFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    mixFormat.Samples.wValidBitsPerSample = 16;

    /* Let's see if this is supported */
    WAVEFORMATEX* format = NULL;
    format = (WAVEFORMATEX*) &mixFormat;

    frameSize = (format->nChannels * format->wBitsPerSample / 8);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **) &devEnum);
    if (FAILED(hr))
    {
        //"Failed getting MMDeviceEnumerator."
        return PlayerError::PCMError;
    }

    if(record)
    {
        if (mInitializedCapture)
        {
            return PlayerError::OK;
        }

        devCapture = NULL;
        audioCapClient = NULL;
        hr = devEnum->GetDefaultAudioEndpoint(eCapture, eConsole, &devCapture);
        LOGERROR(hr, "Failed to getdefaultaudioendpoint for Capture.");
        if (devCapture) {
            hr = devCapture->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioCapClient);
            LOGERROR(hr, "Failed capture activate.");
        }
        captureClient = NULL;
        if (audioCapClient){
            hr = audioCapClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_RATEADJUST, bufferDuration, 0, format, NULL);
            if (hr != S_OK) {
                hr = audioCapClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, format, NULL);
            }
            
            if (FAILED(hr))
            {
                //"Failed audioCapClient->Initialize"
                return PlayerError::PCMError;
            }
            
            hr = audioCapClient->GetService(__uuidof(IAudioCaptureClient), (void **)&captureClient);
            if (FAILED(hr))
            {
                //"Failed getting captureClient"
                return PlayerError::PCMError;
            }
            
            hr = audioCapClient->GetBufferSize(&bufferSize);
            if (FAILED(hr))
            {
                //"Failed getting BufferSize"
                return PlayerError::PCMError;
            }
        }
        mStartedCapture = false;
        mInitializedCapture = true;

    }
    else {
        if (mInitializedRender)
        {
            return PlayerError::OK;
        }

        devRender = NULL;
        audioClient = NULL;
        hr = devEnum->GetDefaultAudioEndpoint(eRender, eConsole, &devRender);
        //FAILONERROR(hr, "Failed to getdefaultaudioendpoint for Render.");
        if (FAILED(hr))
        {
            //"Failed getting BufferSize"
            return PlayerError::PCMError;
        }

        if (devRender) {
            hr = devRender->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient);
            //FAILONERROR(hr, "Failed render activate.");
            if (FAILED(hr))
            {
                //"Failed getting BufferSize"
                return PlayerError::PCMError;
            }
        }
        if(!muteInit)
        {
            hr = devRender->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void **)&g_pEndptVol);
            //FAILONERROR(hr, "Failed Mute Init.");
            if (FAILED(hr))
            {
                //"Failed getting BufferSize"
                return PlayerError::PCMError;
            }

            muteInit =1;
        }
       hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_RATEADJUST, bufferDuration, 0, format, NULL);
        if (hr != S_OK) {
            hr = audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufferDuration, 0, format, NULL);
        }
        //FAILONERROR(hr, "Failed audioClient->Initialize");
        if (FAILED(hr))
        {
            //"Failed getting BufferSize"
            return PlayerError::PCMError;
        }

        hr = audioClient->GetService(__uuidof(IAudioRenderClient), (void **) &renderClient);
        //FAILONERROR(hr, "Failed getting renderClient");
        if (FAILED(hr))
        {
            //"Failed getting BufferSize"
            return PlayerError::PCMError;
        }

        hr = audioClient->GetBufferSize(&bufferSize);
        //FAILONERROR(hr, "Failed getting BufferSize");
        if (FAILED(hr))
        {
            //"Failed getting BufferSize"
            return PlayerError::PCMError;
        }

        mStartedRender = false;
        mInitializedRender = true;
    }

    return PlayerError::OK;
}

void WASAPIPlayer::Close()
{
    // Reset system timer
    //timeEndPeriod(1);
    if (audioClient)
        audioClient->Stop();
    SAFE_RELEASE(renderClient);
    SAFE_RELEASE(audioClient);
    SAFE_RELEASE(devRender);
    SAFE_RELEASE(devCapture);
    SAFE_RELEASE(devEnum);
	mInitializedRender = false;
	mInitializedCapture = false;
}

uint32_t WASAPIPlayer::Play(uint8_t * buffer2Play, uint32_t size, bool mute)
{
    if (audioClient == NULL || renderClient==NULL)
        return 0;

    HRESULT hr;
    UINT padding = 0;
    UINT availableFreeBufferSize = 0;
    UINT frames;
    CHAR *buffer = NULL;

    hr = audioClient->GetCurrentPadding(&padding);
    FAILONERROR(hr, "Failed getCurrentPadding");

    availableFreeBufferSize = size - padding;

    frames = min(availableFreeBufferSize/frameSize, size/frameSize);

    hr = renderClient->GetBuffer(frames, (BYTE **) &buffer);
    FAILONERROR(hr, "Failed getBuffer");

    if (mute)
        memset(buffer, 0, (frames*frameSize));
    else
        memcpy(buffer, buffer2Play, (frames*frameSize));

    hr = renderClient->ReleaseBuffer(frames, NULL);
    FAILONERROR(hr, "Failed releaseBuffer");

    if (!mStartedRender)
    {
        mStartedRender = TRUE;
        audioClient->Start();
    }

    return (frames*frameSize);
}

uint32_t WASAPIPlayer::Record(uint8_t * buffer, uint32_t size)
{
    /*if (captureClient == NULL)
        return 0;

    HRESULT hr;
    UINT32 frames;
    CHAR *buffer = NULL;

    if (!mStartedCapture)
    {
        mStartedCapture = TRUE;
        hr = audioCapClient->Start();
    }

    //hr = audioClient->GetCurrentPadding(&padding);
    hr = captureClient->GetNextPacketSize(&frames);
    FAILONERROR(hr, "Failed GetNextPacketSize");
    if (frames == 0) {
        return frames;
    }

    frames = min(frames, size / frameSize);


    DWORD flags;
    //UINT64 DevPosition;
    //UINT64 QPCPosition;
    hr = captureClient->GetBuffer( (BYTE **)&buffer, &frames, &flags, NULL, NULL );
    FAILONERROR(hr, "Failed getBuffer");

    memcpy(pOutputBuffer, buffer, (frames*frameSize));

    hr = captureClient->ReleaseBuffer(frames);
    FAILONERROR(hr, "Failed releaseBuffer");

    return  (frames*frameSize);*/
    return 0;
}