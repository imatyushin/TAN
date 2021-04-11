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

#include "IAudio3D.h"

class Audio3DAMF:
    public IAudio3D
{
protected:
    AMFContextPtr               mContext12;
    AMFComputePtr               mCompute1;
    AMFComputePtr               mCompute2;

    AMFContextPtr               mContext3;
    AMFComputePtr               mCompute3;

    AMFContextPtr               mContextTAVR;
    AMFComputePtr               mComputeTAVR;

    AMFBufferPtr                mAMFResponses[MAX_SOURCES * 2];
    AMFBuffer *                 mAMFResponsesInterfaces[MAX_SOURCES * 2] = {nullptr};

	AMFBufferExPtr              mOutputMainAMFBuffer = nullptr;
    AMFBufferPtr                mOutputAMFBuffers[MAX_SOURCES * 2] = {nullptr};
    AMFBuffer *                 mOutputAMFBuffersInterfaces[MAX_SOURCES * 2] = {nullptr};

    AMFBufferPtr                mOutputMixAMFBuffers[2] = {nullptr};
    AMFBuffer *                 mOutputMixAMFBuffersInterfaces[2] = {nullptr};

	AMFBufferPtr                mOutputShortAMFBuffer = nullptr;

    AMF_RESULT Process(int16_t *pOut, int16_t *pChan[MAX_SOURCES], uint32_t sampleCountBytes) override;
    int ProcessProc() override;
    int UpdateProc() override;

    static unsigned processThreadProc(void *ptr);
    static unsigned updateThreadProc(void *ptr);

public:
    Audio3DAMF();
    Audio3DAMF(Audio3DAMF const &) = delete;
    virtual ~Audio3DAMF();

    AMF_RESULT InitObjects() override;

	// finalize, deallocate resources, close files, etc.
	void Close() override;

	// start audio engine:
    bool Run() override;

	// Stop audio engine:
    void Stop() override;
};
