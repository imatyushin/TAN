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
#include "AlsaPlayer.h"
#include "wav.h"

#include <string>
#include <vector>

AlsaPlayer::AlsaPlayer():
    mPCMHandle(nullptr),
    mUpdatePeriod(0)
{
}

AlsaPlayer::~AlsaPlayer()
{
    Release();
}

/**
 *******************************************************************************
 * @fn Init
 * @brief Will init alsa
 *******************************************************************************
 */
WavError AlsaPlayer::Init(const STREAMINFO *streaminfo, uint32_t *bufferSize, uint32_t *frameSize, bool capture)
{
    std::vector<std::string> devices;
    int pcmError(0);

    //Start with first card
    int cardNum = -1;

    for(;;)
    {
        snd_ctl_t *cardHandle(nullptr);

        if((pcmError = snd_card_next(&cardNum)) < 0)
        {
            std::cerr << "Can't get the next card number: %s" << snd_strerror(pcmError) << std::endl;

            break;
        }

        if(cardNum < 0)
        {
            break;
        }

        // Open this card's control interface. We specify only the card number -- not
        // any device nor sub-device too
        {
            std::string cardName("hw:");
            cardName += std::to_string(cardNum);

            if((pcmError = snd_ctl_open(&cardHandle, cardName.c_str(), 0)) < 0)
            {
                std::cerr << "Can't open card " << cardNum << ": " << snd_strerror(pcmError) << std::endl;

                continue;
            }

            devices.push_back(cardName);
        }

        {
            snd_ctl_card_info_t *cardInfo(nullptr);

            // We need to get a snd_ctl_card_info_t. Just alloc it on the stack
            snd_ctl_card_info_alloca(&cardInfo);

            // Tell ALSA to fill in our snd_ctl_card_info_t with info about this card
            if((pcmError = snd_ctl_card_info(cardHandle, cardInfo)) < 0)
            {
                std::cerr << "Can't get info for card " << cardNum << ": " << snd_strerror(pcmError) << std::endl;
            }
            else
            {
                std::cout << "Card " << cardNum << " = " << snd_ctl_card_info_get_name(cardInfo) << std::endl;
            }
        }

        // Close the card's control interface after we're done with it
        snd_ctl_close(cardHandle);
    }

    if(devices.empty())
    {
        std::cerr << "Error: No compatible sound card found." << std::endl;

        return WavError::PCMError;
    }

    /* Open the PCM device in playback mode */
	if(pcmError = snd_pcm_open(
        &mPCMHandle,
        "default",
        SND_PCM_STREAM_PLAYBACK,
        0//SND_PCM_ASYNC | SND_PCM_NONBLOCK
        ) < 0)
    {
        std::cerr << "Error: Can't open default PCM device. " << snd_strerror(pcmError) << std::endl;

        return WavError::PCMError;
    }
    else
    {
        std::cerr << "PCM device opened: " << devices[0] << std::endl;
    }

    /* Allocate parameters object and fill it with default values*/
    snd_pcm_hw_params_t *params(nullptr);
	snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(mPCMHandle, params);

    /* Set parameters */
	if(pcmError = snd_pcm_hw_params_set_access(
        mPCMHandle,
        params,
        SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
    {
		std::cerr << "Error: Can't set interleaved mode. " << snd_strerror(pcmError) << std::endl;

        return WavError::PCMError;
    }

	if(pcmError = snd_pcm_hw_params_set_format(
        mPCMHandle,
        params,
        SND_PCM_FORMAT_S16_LE) < 0)
    {
		std::cerr << "Error: Can't set format. " << snd_strerror(pcmError) << std::endl;

        return WavError::PCMError;
    }

	if(pcmError = snd_pcm_hw_params_set_channels(mPCMHandle, params, streaminfo->NumOfChannels) < 0)
    {
		std::cerr << "Error: Can't set channels number. " << snd_strerror(pcmError) << std::endl;

        return WavError::PCMError;
    }

	uint32_t rate(uint32_t(streaminfo->SamplesPerSec));
    if(pcmError = snd_pcm_hw_params_set_rate_near(mPCMHandle, params, &rate, 0) < 0)
    {
		std::cerr << "Error: Can't set rate. " << snd_strerror(pcmError) << std::endl;

        return WavError::PCMError;
    }

	/* Write parameters */
	if(pcmError = snd_pcm_hw_params(mPCMHandle, params) < 0)
    {
		std::cerr << "Error: Can't set harware parameters. " << snd_strerror(pcmError) << std::endl;

        return WavError::PCMError;
    }

	{
        uint32_t channels(0);
	    snd_pcm_hw_params_get_channels(params, &channels);

        uint32_t rate(0);
        snd_pcm_hw_params_get_rate(params, &rate, 0);

        std::cout
            << "PCM name: " << snd_pcm_name(mPCMHandle) << std::endl
            << "PCM state: " << snd_pcm_state_name(snd_pcm_state(mPCMHandle)) << std::endl
            << "Channels count: " << channels << std::endl
            << "Rate: " << rate << std::endl
            ;
    }

    mChannelsCount = streaminfo->NumOfChannels;
    mUpdatePeriod = 0;
    snd_pcm_hw_params_get_period_time(params, &mUpdatePeriod, NULL);

	return WavError::OK;
}

/**
 *******************************************************************************
 * @fn Release
 *******************************************************************************
 */
void AlsaPlayer::Release()
{
    snd_pcm_drain(mPCMHandle);
	snd_pcm_close(mPCMHandle);

    // ALSA allocates some mem to load its config file when we call some of the
    // above functions. Now that we're done getting the info, let's tell ALSA
    // to unload the info and free up that mem
    snd_config_update_free_global();
}

WavError AlsaPlayer::ReadWaveFile(const std::string& fileName, uint32_t& samplesCount, uint8_t **ppOutBuffer)
{
    uint32_t samplesPerSec = 0;
    uint16_t bitsPerSample = 0;
    uint16_t nChannels = 0;
    float **pSamples;
    unsigned char *pOutBuffer;

    if(!::ReadWaveFile(
        fileName.c_str(),
        samplesPerSec,
        bitsPerSample,
        nChannels,
        samplesCount,
        &pOutBuffer,
        &pSamples
        ))
    {
        return WavError::FileNotFound;
    }

    if(nChannels != 2 || bitsPerSample != 16)
    {
        free(pOutBuffer);
        pOutBuffer = (unsigned char *)calloc(samplesCount, 2 * sizeof(short));

        if(!pOutBuffer)
        {
            //return -1;
            //todo: return not enoght memory
            return WavError::FileNotFound;
        }

        short *pSBuf = (short *)pOutBuffer;
        for (int i = 0; i < samplesCount; i++)
        {
            pSBuf[2 * i + 1] = pSBuf[2 * i] = (short)(32767 * pSamples[0][i]);
            if (nChannels == 2)
            {
                pSBuf[2 * i + 1] = (short)(32767 * pSamples[1][i]);
            }
        }
    }

    //don't need floats;
    for (int i = 0; i < nChannels; i++)
    {
        delete pSamples[i];
    }
    delete pSamples;

    *ppOutBuffer = pOutBuffer;

    STREAMINFO streaminfo = {0};
    streaminfo.bitsPerSample = bitsPerSample;
    streaminfo.NumOfChannels = nChannels;
    streaminfo.SamplesPerSec = samplesPerSec;

    //todo: implement similar usage with WASAPI player
    uint32_t bufferSize(0), frameSize(0);

    return Init(&streaminfo, &bufferSize, &frameSize);
}

/**
 *******************************************************************************
 * @fn Play
 * @brief Play output
 *******************************************************************************
 */
uint32_t AlsaPlayer::Play(unsigned char *pOutputBuffer, unsigned int size, bool mute)
{
    uint32_t uiFrames2Play(size / mChannelsCount / 2);

    //todo: mute in realtime?
    if(!mute)
    {
        int pcmError = 0;

        if(pcmError = snd_pcm_writei(mPCMHandle, pOutputBuffer, uiFrames2Play) == -EPIPE)
        {
            snd_pcm_prepare(mPCMHandle);
        }
        else if(pcmError < 0)
        {
            std::cerr << "Error: Can't write to PCM device. " << snd_strerror(pcmError) << std::endl;

            return 0;
        }
    }

    return uiFrames2Play * 2 * mChannelsCount;
}

/**
*******************************************************************************
* @fn Record
*******************************************************************************
*/
uint32_t AlsaPlayer::Record(unsigned char *pOutputBuffer, unsigned int size)
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