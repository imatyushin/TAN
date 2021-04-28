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
///-------------------------------------------------------------------------
///  @file   ConvolutionImpl.h
///  @brief  TANConvolutionImpl interface implementation
///-------------------------------------------------------------------------
#pragma once

#include "Debug.h"
#include "Utilities.h"

#define N_FILTER_STATES 3

struct GraalArgs
{
    GraalArgs():
        versions(nullptr),
        prevVersions(nullptr),
        channels(nullptr),
        lens(nullptr),
        responses(nullptr),
        negateCnt(0),
        updatesCnt(0)
    {
    }

    ~GraalArgs()
    {
        SAFE_ARR_DELETE(versions);
        SAFE_ARR_DELETE(prevVersions);
        SAFE_ARR_DELETE(channels);
        SAFE_ARR_DELETE(lens);
        SAFE_ARR_DELETE(responses);
        SAFE_ARR_DELETE(negateCnt);
    }

    void Alloc(amf_uint32 channelCnt)
    {
        versions = new int[channelCnt];
        prevVersions = new int[channelCnt];
        channels = new int[channelCnt];
        lens = new int[channelCnt];
        responses = new float*[channelCnt];
        negateCnt = new int[channelCnt];
        std::memset(negateCnt, 0, sizeof(int) * channelCnt);
        Clear(channelCnt);
    }

    void Clear(amf_uint32 channelCnt)
    {
        std::memset(versions, 0, sizeof(int) * channelCnt);
        std::memset(prevVersions, 0, sizeof(int) * channelCnt);
        std::memset(channels, 0, sizeof(int) * channelCnt);
        std::memset(lens, 0, sizeof(int) * channelCnt);
        std::memset(responses, 0, sizeof(float*) * channelCnt);
        updatesCnt = 0;
    }

    void Pack(const GraalArgs &from, amf_uint32 channelCnt)
    {
        Clear(channelCnt);

        for (amf_uint32 channelId = 0; channelId < channelCnt; channelId++)
        {
            if (from.lens[channelId] > 0) {
                versions[updatesCnt] = from.versions[channelId];
                channels[updatesCnt] = from.channels[channelId];
                lens[updatesCnt] = from.lens[channelId];
                responses[updatesCnt] = from.responses[channelId];
                updatesCnt++;
            }
        }
    }

    void Negate(
        const GraalArgs &from,
        amf_uint32 channelCnt,
        amf_uint32 fromVersion,
        amf_uint32 toVersion)
    {
        Clear(channelCnt);

        amf_uint32 lastChannelId = 0;
        for (amf_uint32 channelId = 0; channelId < channelCnt; channelId++)
        {
            if (from.lens[channelId] == 0) {
                // We keep copying a stopped channel from curr slot to update slot up
                // until its propagated through out all the IR buffers (N_FILTER_STATES)
                if (negateCnt[channelId] >= N_FILTER_STATES-1) { continue; }
                versions[updatesCnt] = toVersion;
                prevVersions[updatesCnt] = fromVersion;
                channels[updatesCnt] = channelId;
                negateCnt[channelId]++;
                updatesCnt++;
            } else {
                negateCnt[channelId] = 0;// reset the counter
            }
        }
    }

    int *versions;
    int *prevVersions;
    int *channels;
    int *lens;
    float **responses;
    int *negateCnt; // Counts how many times a stopped channel has been copied from curr to update slot
    amf_uint32 updatesCnt;
};