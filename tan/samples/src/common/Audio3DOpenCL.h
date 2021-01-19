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

// Simple VR audio engine using True Audio Next GPU acceleration
class Audio3DOpenCL:
    public IAudio3D
{
public:
    Audio3DOpenCL();
    Audio3DOpenCL(Audio3DOpenCL const &) = delete;
    virtual ~Audio3DOpenCL();

    AMF_RESULT InitObjects() override;

	// finalize, deallocate resources, close files, etc.
	void Close();

	// start audio engine:
    bool Run();

	// Stop audio engine:
    void Stop();

    std::string GetLastError() const
    {
        return mLastError;
    }

protected:
    static bool useIntrinsics;
    static const int IR_UPDATE_MODE = 1; // 0: Non-Blocking 1: Blocking
    static unsigned processThreadProc(void *ptr);
    static unsigned updateThreadProc(void *ptr);

    int ProcessProc();
    int UpdateProc();
    AMF_RESULT Process(int16_t * pOut, int16_t * pChan[MAX_SOURCES], uint32_t sampleCount);

    cl_mem mOCLResponses[MAX_SOURCES * 2] = {nullptr};
    cl_mem mOutputCLBufs[MAX_SOURCES * 2] = {nullptr};
	cl_mem mOutputMainCLbuf = nullptr;
	cl_mem mOutputMixCLBufs[2] = {nullptr};
	cl_mem mOutputShortBuf = nullptr;

    // RT-Queues
    cl_command_queue mCmdQueue1 = nullptr;
    cl_command_queue mCmdQueue2 = nullptr;
    cl_command_queue mCmdQueue3 = nullptr;
};
