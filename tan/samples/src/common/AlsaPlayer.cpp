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

#define MAXFILES 100
int muteInit = 1;
//IAudioEndpointVolume *g_pEndptVol = NULL;

AlsaPlayer::AlsaPlayer():
    startedRender(false),
    startedCapture(false),
    initializedRender(false),
    initializedCapture(false)
{
}

AlsaPlayer::~AlsaPlayer()
{
    Release();
}


/**
 *******************************************************************************
 * @fn wasapiInit
 * @brief Will export stream header contents
 *
 * @param[in/out] mp3Decoder    : Points to structure which holds
 *                                elements required for MP3Decoding
 *
 * @return INT
 *         0   for success
 *         >0  for failure
 *
 *******************************************************************************
 */
WavError AlsaPlayer::Init(STREAMINFO *streaminfo, uint32_t *bufferSize, uint32_t *frameSize, bool capture)
{
    /*HRESULT hr;

    INT sampleRate = streaminfo->SamplesPerSec;
    INT numCh = streaminfo->NumOfChannels;
    INT bitsPerSample = streaminfo->bitsPerSample;

    REFERENCE_TIME bufferDuration = (BUFFER_SIZE_8K);//(SIXTH_SEC_BUFFER_SIZE); //(MS100_BUFFER_SIZE); // (ONE_SEC_BUFFER_SIZE);

    WAVEFORMATEXTENSIBLE mixFormat;

    /* PCM audio * /
    mixFormat.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;

    /* Default is 44100 * /
    mixFormat.Format.nSamplesPerSec = sampleRate;

    /* Default is 2 channels * /
    mixFormat.Format.nChannels = (WORD) numCh;

    /* Default is 16 bit * /
    mixFormat.Format.wBitsPerSample = (WORD) bitsPerSample;

    mixFormat.Format.cbSize = sizeof(mixFormat) - sizeof(WAVEFORMATEX);

    /* nChannels * bitsPerSample / BitsPerByte (8) = 4 * /
    mixFormat.Format.nBlockAlign = (WORD)(numCh * bitsPerSample / 8);

    /* samples per sec * blockAllign * /
    mixFormat.Format.nAvgBytesPerSec = sampleRate * (numCh * bitsPerSample / 8);

    mixFormat.dwChannelMask = KSAUDIO_SPEAKER_STEREO;
    mixFormat.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
    mixFormat.Samples.wValidBitsPerSample = 16;

    /* Let's see if this is supported * /
    WAVEFORMATEX* format = NULL;
    format = (WAVEFORMATEX*) &mixFormat;

    (*frameSize) = (format->nChannels * format->wBitsPerSample / 8);

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **) &devEnum);
    FAILONERROR(hr, "Failed getting MMDeviceEnumerator.");

    if (capture){
        if (initializedCapture){
            return 0;
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
            hr = audioCapClient->Initialize(sharMode, AUDCLNT_STREAMFLAGS_RATEADJUST, bufferDuration, 0, format, NULL);
            if (hr != S_OK) {
                hr = audioCapClient->Initialize(sharMode, 0, bufferDuration, 0, format, NULL);
            }
            FAILONERROR(hr, "Failed audioCapClient->Initialize");
            hr = audioCapClient->GetService(__uuidof(IAudioCaptureClient), (void **)&captureClient);
            FAILONERROR(hr, "Failed getting captureClient");
            hr = audioCapClient->GetBufferSize(bufferSize);
            FAILONERROR(hr, "Failed getting BufferSize");
        }
        startedCapture = false;
        initializedCapture = true;

    }
    else {
        if (initializedRender){
            return 0;
        }
        devRender = NULL;
        audioClient = NULL;
        hr = devEnum->GetDefaultAudioEndpoint(eRender, eConsole, &devRender);
        FAILONERROR(hr, "Failed to getdefaultaudioendpoint for Render.");
        if (devRender) {
            hr = devRender->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void**)&audioClient);
            FAILONERROR(hr, "Failed render activate.");
        }
        if(!muteInit)
        {
            hr = devRender->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void **)&g_pEndptVol);
            FAILONERROR(hr, "Failed Mute Init.");
            muteInit =1;
        }
       hr = audioClient->Initialize(sharMode, AUDCLNT_STREAMFLAGS_RATEADJUST, bufferDuration, 0, format, NULL);
        if (hr != S_OK) {
            hr = audioClient->Initialize(sharMode, 0, bufferDuration, 0, format, NULL);
        }
        FAILONERROR(hr, "Failed audioClient->Initialize");
        hr = audioClient->GetService(__uuidof(IAudioRenderClient), (void **) &renderClient);
        FAILONERROR(hr, "Failed getting renderClient");
        hr = audioClient->GetBufferSize(bufferSize);
        FAILONERROR(hr, "Failed getting BufferSize");
        startedRender = false;
        initializedRender = true;

    }

    //AUDIO_STREAM_CATEGORY AudioCategory_ForegroundOnlyMedia
    //BOOL isOffloadCapable = false;

    return hr;*/
    return WavError::OK;
}



/**
 *******************************************************************************
 * @fn wasapiRelease
 * @brief Play output using Wasapi Application
 *
 * @param[in/out] mp3Decoder    : Points to structure which holds
 *                                elements required for MP3Decoding
 *
 * @return INT
 *         0   for success
 *         >0  for failure
 *
 *******************************************************************************
 */
void AlsaPlayer::Release()
{
    /*
    // Reset system timer
    //timeEndPeriod(1);
    if (audioClient)
        audioClient->Stop();
    SAFE_RELEASE(renderClient);
    SAFE_RELEASE(audioClient);
    SAFE_RELEASE(devRender);
    SAFE_RELEASE(devCapture);
    SAFE_RELEASE(devEnum);
	initializedRender = false;
	initializedCapture = false;
    */
}

WavError AlsaPlayer::ReadWaveFile(const std::string& fileName, long *pNsamples, unsigned char **ppOutBuffer)
{
    int samplesPerSec = 0;
    int bitsPerSample = 0;
    int nChannels = 0;
    float **pSamples;
    unsigned char *pOutBuffer;

    if(!::ReadWaveFile(
        fileName.c_str(),
        &samplesPerSec,
        &bitsPerSample,
        &nChannels,
        pNsamples,
        &pOutBuffer,
        &pSamples
        ))
    {
        return WavError::FileNotFound;
    }

    if (nChannels != 2 || bitsPerSample != 16) {
        free(pOutBuffer);
        pOutBuffer = (unsigned char *)calloc(*pNsamples, 2 * sizeof(short));
        if (!pOutBuffer)
        {
            //return -1;
            //todo: return not enoght memory
            return WavError::FileNotFound;
        }

        short *pSBuf = (short *)pOutBuffer;
        for (int i = 0; i < *pNsamples; i++)
        {
            pSBuf[2 * i + 1] = pSBuf[2 * i] = (short)(32767 * pSamples[0][i]);
            if (nChannels == 2){
                pSBuf[2 * i + 1] = (short)(32767 * pSamples[1][i]);
            }
        }
    }
    //don't need floats;
    for (int i = 0; i < nChannels; i++){
        delete pSamples[i];
    }
    delete pSamples;

    *ppOutBuffer = pOutBuffer;

    STREAMINFO streaminfo = {0};
    //memset(&streaminfo, 0, sizeof(STREAMINFO));
    streaminfo.bitsPerSample = 16;
    streaminfo.NumOfChannels = 2;
    streaminfo.SamplesPerSec = samplesPerSec;// 48000;

    return Init(&streaminfo,  &bufferSize, &frameSize);
}

/**
 *******************************************************************************
 * @fn wasapiPlay
 * @brief Play output using Wasapi Application
 *
 * @param[in/out] mp3Decoder    : Points to structure which holds
 *                                elements required for MP3Decoding
 *
 * @return INT
 *         0   for success
 *         >0  for failure
 *
 *******************************************************************************
 */
uint32_t AlsaPlayer::Play(unsigned char *pOutputBuffer, unsigned int size, bool mute)
{
    /*if (audioClient == NULL || renderClient==NULL)
        return 0;

    HRESULT hr;
    UINT padding = 0;
    UINT availableFreeBufferSize = 0;
    UINT frames;
    CHAR *buffer = NULL;

    hr = audioClient->GetCurrentPadding(&padding);
    FAILONERROR(hr, "Failed getCurrentPadding");

    availableFreeBufferSize = bufferSize - padding;

    frames = min(availableFreeBufferSize/frameSize, size/frameSize);

    hr = renderClient->GetBuffer(frames, (BYTE **) &buffer);
    FAILONERROR(hr, "Failed getBuffer");

    if (mute)
        memset(buffer, 0, (frames*frameSize));
    else
        memcpy(buffer, pOutputBuffer, (frames*frameSize));

    hr = renderClient->ReleaseBuffer(frames, NULL);
    FAILONERROR(hr, "Failed releaseBuffer");

    if (!startedRender)
    {
        startedRender = TRUE;
        audioClient->Start();
    }

    return  (frames*frameSize);*/
    return 0;
}

/**
*******************************************************************************
* @fn wasapiRecord
* @brief Play output using Wasapi Application
*
* @param[in/out]     : Points to structure
*
*
* @return INT
*         0   for success
*         >0  for failure
*
*******************************************************************************
*/
uint32_t AlsaPlayer::Record(unsigned char *pOutputBuffer, unsigned int size)
{
    /*if (captureClient == NULL)
        return 0;

    HRESULT hr;
    UINT32 frames;
    CHAR *buffer = NULL;

    if (!startedCapture)
    {
        startedCapture = TRUE;
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

/*
bool AlsaPlayer::PlayQueuedStreamChunk(bool init, long sampleCount, unsigned char *pOutBuffer )
{
    static int bytesPlayed;
    static int bytesRecorded;
    static bool done = false;
    static unsigned char *pData;
    static int size;

    if(init) {
            pData = pOutBuffer;
            size = sampleCount * frameSize;
        return false;
    }

    done = true;
    if (size > 0){
        bytesPlayed = Play( pData, size, false);
        pData += bytesPlayed;
        size -= bytesPlayed;
        done = false;
        //printf("stream%d: %d bytes played\n",i,bytesPlayed);
        Sleep(0);
    }
    Sleep(5);

    return done;
}
*/

/*
#include <alsa/asoundlib.h>
#include <stdio.h>


int main(int argc, char **argv) {
	unsigned int pcm, tmp, dir;
	int rate, channels, seconds;
	snd_pcm_t *pcm_handle;
	snd_pcm_hw_params_t *params;
	snd_pcm_uframes_t frames;
	char *buff;
	int buff_size, loops;

	if (argc < 5) {
		printf("Usage: %s <sample_rate> <channels> <seconds>\n",
								argv[0]);
		return -1;
	}

	rate 	 = atoi(argv[1]);
	channels = atoi(argv[2]);
	seconds  = atoi(argv[3]);

	/* Open the PCM device in playback mode * /
	if (pcm = snd_pcm_open(&pcm_handle, argv[4]   ,
					SND_PCM_STREAM_PLAYBACK, 0) < 0)
		printf("ERROR: Can't open \"%s\" PCM device. %s\n",
					argv[4] , snd_strerror(pcm));

	/* Allocate parameters object and fill it with default values* /
	snd_pcm_hw_params_alloca(&params);

	snd_pcm_hw_params_any(pcm_handle, params);

	/* Set parameters * /
	if (pcm = snd_pcm_hw_params_set_access(pcm_handle, params,
					SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
		printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_format(pcm_handle, params,
						SND_PCM_FORMAT_S16_LE) < 0)
		printf("ERROR: Can't set format. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_channels(pcm_handle, params, channels) < 0)
		printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm));

	if (pcm = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &rate, 0) < 0)
		printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm));

	/* Write parameters * /
	if (pcm = snd_pcm_hw_params(pcm_handle, params) < 0)
		printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm));

	/* Resume information * /
	printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));

	printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

	snd_pcm_hw_params_get_channels(params, &tmp);
	printf("channels: %i ", tmp);

	if (tmp == 1)
		printf("(mono)\n");
	else if (tmp == 2)
		printf("(stereo)\n");

	snd_pcm_hw_params_get_rate(params, &tmp, 0);
	printf("rate: %d bps\n", tmp);

	printf("seconds: %d\n", seconds);

	/* Allocate buffer to hold single period * /
	snd_pcm_hw_params_get_period_size(params, &frames, 0);

	buff_size = frames * channels * 2 /* 2 -> sample size * /;
	buff = (char *) malloc(buff_size);

	snd_pcm_hw_params_get_period_time(params, &tmp, NULL);

	for (loops = (seconds * 1000000) / tmp; loops > 0; loops--) {

		if (pcm = read(0, buff, buff_size) == 0) {
			printf("Early end of file.\n");
		}

		if (pcm = snd_pcm_writei(pcm_handle, buff, frames) == -EPIPE) {
			printf("XRUN.\n");
			snd_pcm_prepare(pcm_handle);
		} else if (pcm < 0) {
			printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm));
		}

	}

	snd_pcm_drain(pcm_handle);
	snd_pcm_close(pcm_handle);
	free(buff);

	return 0;
}
*/
/*
// A simple C example to play a mono or stereo, 16-bit 44KHz
// WAVE file using ALSA. This goes directly to the first
// audio card (ie, its first set of audio out jacks). It
// uses the snd_pcm_writei() mode of outputting waveform data,
// blocking.
//
// Compile as so to create "alsawave":
// gcc -o alsawave alsawave.c -lasound
//
// Run it from a terminal, specifying the name of a WAVE file to play:
// ./alsawave MyWaveFile.wav

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Include the ALSA .H file that defines ALSA functions/data
#include <alsa/asoundlib.h>





#pragma pack (1)
/////////////////////// WAVE File Stuff /////////////////////
// An IFF file header looks like this
typedef struct _FILE_head
{
	unsigned char	ID[4];	// could be {'R', 'I', 'F', 'F'} or {'F', 'O', 'R', 'M'}
	unsigned int	Length;	// Length of subsequent file (including remainder of header). This is in
									// Intel reverse byte order if RIFF, Motorola format if FORM.
	unsigned char	Type[4];	// {'W', 'A', 'V', 'E'} or {'A', 'I', 'F', 'F'}
} FILE_head;


// An IFF chunk header looks like this
typedef struct _CHUNK_head
{
	unsigned char ID[4];	// 4 ascii chars that is the chunk ID
	unsigned int	Length;	// Length of subsequent data within this chunk. This is in Intel reverse byte
							// order if RIFF, Motorola format if FORM. Note: this doesn't include any
							// extra byte needed to pad the chunk out to an even size.
} CHUNK_head;

// WAVE fmt chunk
typedef struct _FORMAT {
	short				wFormatTag;
	unsigned short	wChannels;
	unsigned int	dwSamplesPerSec;
	unsigned int	dwAvgBytesPerSec;
	unsigned short	wBlockAlign;
	unsigned short	wBitsPerSample;
  // Note: there may be additional fields here, depending upon wFormatTag
} FORMAT;
#pragma pack()






// Size of the audio card hardware buffer. Here we want it
// set to 1024 16-bit sample points. This is relatively
// small in order to minimize latency. If you have trouble
// with underruns, you may need to increase this, and PERIODSIZE
// (trading off lower latency for more stability)
#define BUFFERSIZE	(2*1024)

// How many sample points the ALSA card plays before it calls
// our callback to fill some more of the audio card's hardware
// buffer. Here we want ALSA to call our callback after every
// 64 sample points have been played
#define PERIODSIZE	(2*64)

// Handle to ALSA (audio card's) playback port
snd_pcm_t				*PlaybackHandle;

// Handle to our callback thread
snd_async_handler_t	*CallbackHandle;

// Points to loaded WAVE file's data
unsigned char			*WavePtr;

// Size (in frames) of loaded WAVE file's data
snd_pcm_uframes_t		WaveSize;

// Sample rate
unsigned short			WaveRate;

// Bit resolution
unsigned char			WaveBits;

// Number of channels in the wave file
unsigned char			WaveChannels;

// The name of the ALSA port we output to. In this case, we're
// directly writing to hardware card 0,0 (ie, first set of audio
// outputs on the first audio card)
static const char		SoundCardPortName[] = "plughw:1,0";

// For WAVE file loading
static const unsigned char Riff[4]	= { 'R', 'I', 'F', 'F' };
static const unsigned char Wave[4] = { 'W', 'A', 'V', 'E' };
static const unsigned char Fmt[4] = { 'f', 'm', 't', ' ' };
static const unsigned char Data[4] = { 'd', 'a', 't', 'a' };





/********************** compareID() *********************
 * Compares the passed ID str (ie, a ptr to 4 Ascii
 * bytes) with the ID at the passed ptr. Returns TRUE if
 * a match, FALSE if not.
 * /

static unsigned char compareID(const unsigned char * id, unsigned char * ptr)
{
	register unsigned char i = 4;

	while (i--)
	{
		if ( *(id)++ != *(ptr)++ ) return(0);
	}
	return(1);
}





/********************** waveLoad() *********************
 * Loads a WAVE file.
 *
 * fn =			Filename to load.
 *
 * RETURNS: 0 if success, non-zero if not.
 *
 * NOTE: Sets the global "WavePtr" to an allocated buffer
 * containing the wave data, and "WaveSize" to the size
 * in sample points.
 * /

static unsigned char waveLoad(const char *fn)
{
	const char				*message;
	FILE_head				head;
	register int			inHandle;

	if ((inHandle = open(fn, O_RDONLY)) == -1)
		message = "didn't open";

	// Read in IFF File header
	else
	{
		if (read(inHandle, &head, sizeof(FILE_head)) == sizeof(FILE_head))
		{
			// Is it a RIFF and WAVE?
			if (!compareID(&Riff[0], &head.ID[0]) || !compareID(&Wave[0], &head.Type[0]))
			{
				message = "is not a WAVE file";
				goto bad;
			}

			// Read in next chunk header
			while (read(inHandle, &head, sizeof(CHUNK_head)) == sizeof(CHUNK_head))
			{
				// ============================ Is it a fmt chunk? ===============================
				if (compareID(&Fmt[0], &head.ID[0]))
				{
					FORMAT	format;

					// Read in the remainder of chunk
					if (read(inHandle, &format.wFormatTag, sizeof(FORMAT)) != sizeof(FORMAT)) break;

					// Can't handle compressed WAVE files
					if (format.wFormatTag != 1)
					{
						message = "compressed WAVE not supported";
						goto bad;
					}

					WaveBits = (unsigned char)format.wBitsPerSample;
					WaveRate = (unsigned short)format.dwSamplesPerSec;
					WaveChannels = format.wChannels;
				}

				// ============================ Is it a data chunk? ===============================
				else if (compareID(&Data[0], &head.ID[0]))
				{
					// Size of wave data is head.Length. Allocate a buffer and read in the wave data
					if (!(WavePtr = (unsigned char *)malloc(head.Length)))
					{
						message = "won't fit in RAM";
						goto bad;
					}

					if (read(inHandle, WavePtr, head.Length) != head.Length)
					{
						free(WavePtr);
						break;
					}

					// Store size (in frames)
					WaveSize = (head.Length * 8) / ((unsigned int)WaveBits * (unsigned int)WaveChannels);

					close(inHandle);
					return(0);
				}

				// ============================ Skip this chunk ===============================
				else
				{
					if (head.Length & 1) ++head.Length;  // If odd, round it up to account for pad byte
					lseek(inHandle, head.Length, SEEK_CUR);
				}
			}
		}

		message = "is a bad WAVE file";
bad:	close(inHandle);
	}

	printf("%s %s\n", fn, message);
	return(1);
}









/********************** play_audio() **********************
 * Plays the loaded waveform.
 *
 * NOTE: ALSA sound card's handle must be in the global
 * "PlaybackHandle". A pointer to the wave data must be in
 * the global "WavePtr", and its size of "WaveSize".
 * /

static void play_audio(void)
{
	register snd_pcm_uframes_t		count, frames;

	// Output the wave data
	count = 0;
	do
	{
		frames = snd_pcm_writei(PlaybackHandle, WavePtr + count, WaveSize - count);

		// If an error, try to recover from it
		if (frames < 0)
			frames = snd_pcm_recover(PlaybackHandle, frames, 0);
		if (frames < 0)
		{
			printf("Error playing wave: %s\n", snd_strerror(frames));
			break;
		}

		// Update our pointer
		count += frames;

	} while (count < WaveSize);

	// Wait for playback to completely finish
	if (count == WaveSize)
		snd_pcm_drain(PlaybackHandle);
}





/*********************** free_wave_data() *********************
 * Frees any wave data we loaded.
 *
 * NOTE: A pointer to the wave data be in the global
 * "WavePtr".
 * /

static void free_wave_data(void)
{
	if (WavePtr) free(WavePtr);
	WavePtr = 0;
}





int main(int argc, char **argv)
{
	// No wave data loaded yet
	WavePtr = 0;

	if (argc < 2)
		printf("You must supply the name of a 16-bit mono WAVE file to play\n");

	// Load the wave file
	else if (!waveLoad(argv[1]))
	{
		register int		err;

		// Open audio card we wish to use for playback
		if ((err = snd_pcm_open(&PlaybackHandle, &SoundCardPortName[0], SND_PCM_STREAM_PLAYBACK, 0)) < 0)
			printf("Can't open audio %s: %s\n", &SoundCardPortName[0], snd_strerror(err));
		else
		{
			switch (WaveBits)
			{
				case 8:
					err = SND_PCM_FORMAT_U8;
					break;

				case 16:
					err = SND_PCM_FORMAT_S16;
					break;

				case 24:
					err = SND_PCM_FORMAT_S24;
					break;

				case 32:
					err = SND_PCM_FORMAT_S32;
					break;
			}

			// Set the audio card's hardware parameters (sample rate, bit resolution, etc)
			if ((err = snd_pcm_set_params(PlaybackHandle, err, SND_PCM_ACCESS_RW_INTERLEAVED, WaveChannels, WaveRate, 1, 500000)) < 0)
				printf("Can't set sound parameters: %s\n", snd_strerror(err));

			// Play the waveform
			else
				play_audio();

			// Close sound card
			snd_pcm_close(PlaybackHandle);
		}
	}

	// Free the WAVE data
	free_wave_data();

	return(0);
}*/

/*

PulseAudio  12.0

    Main Page
    Related Pages
    +Data Structures
    +Files
    Examples

[click to disable panel synchronisation]

PulseAudio

Introduction
Simple API
Asynchronous API
Error Handling
Logging
pkg-config
Simple API
Asynchronous API
Channel Maps
Sample Format Specifications
Volume Control
Deprecated List
Data Structures
Files
Examples

pacat-simple.c

            parec-simple.c

pacat-simple.c

A simple playback tool using the simple API
/***
  This file is part of PulseAudio.
  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.
  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
*** /
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#define BUFSIZE 1024
int main(int argc, char*argv[]) {
    /* The Sample format to use * /
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
        .channels = 2
    };
    pa_simple *s = NULL;
    int ret = 1;
    int error;
    /* replace STDIN with the specified file if needed * /
    if (argc > 1) {
        int fd;
        if ((fd = open(argv[1], O_RDONLY)) < 0) {
            fprintf(stderr, __FILE__": open() failed: %s\n", strerror(errno));
            goto finish;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, __FILE__": dup2() failed: %s\n", strerror(errno));
            goto finish;
        }
        close(fd);
    }
    /* Create a new playback stream * /
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error))) {
        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }
    for (;;) {
        uint8_t buf[BUFSIZE];
        ssize_t r;
#if 0
        pa_usec_t latency;
        if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t) -1) {
            fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        fprintf(stderr, "%0.0f usec    \r", (float)latency);
#endif
        /* Read some data ... * /
        if ((r = read(STDIN_FILENO, buf, sizeof(buf))) <= 0) {
            if (r == 0) /* EOF * /
                break;
            fprintf(stderr, __FILE__": read() failed: %s\n", strerror(errno));
            goto finish;
        }
        /* ... and play it * /
        if (pa_simple_write(s, buf, (size_t) r, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }
    /* Make sure that every single sample was played * /
    if (pa_simple_drain(s, &error) < 0) {
        fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
        goto finish;
    }
    ret = 0;
finish:
    if (s)
        pa_simple_free(s);
    return ret;
}
*/