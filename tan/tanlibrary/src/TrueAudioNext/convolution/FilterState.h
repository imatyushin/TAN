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
#include <map>

struct ovlAddFilterState
{
    float                       **m_Filter = nullptr;
    std::map<size_t, size_t>    FilterLength;

    float                       **m_Overlap = nullptr;
    std::map<size_t, size_t>    OverlapLength;

    float                       **m_internalFilter = nullptr;
    float                       **m_internalOverlap = nullptr;
    size_t                      mChannelsCount = 0;
    bool                        mOverlapSet = false;

    ~ovlAddFilterState()
    {
        Release();
    }

    inline void DebugPrint(const std::string & caption, size_t channel)
    {
        PrintDebug(caption);

        /*m_Filter ?
            PrintFloatArray(
                "m_Filter",
                m_Filter[channel],
                FilterLength[channel],
                64
                ) :
            PrintDebug("m_Filter is null");
        m_Overlap ?
            PrintFloatArray(
                "m_Overlap",
                m_Overlap[channel],
                OverlapLength[channel],
                64
                ) :
            PrintDebug("m_Overlap is null");*/

        if(m_internalFilter && m_internalFilter[channel])
        {
            /*PrintFloatArray(
                "m_internalFilter",
                m_internalFilter[channel],
                FilterLength[channel],
                64
                );*/
        }
        else
        {
            //PrintDebug("m_internalFilter - null");
        }

        if(m_internalOverlap && m_internalOverlap[channel])
        {
            /*PrintFloatArray(
                "m_internalOverlap",
                m_internalOverlap[channel],
                FilterLength[channel],
                64
                );*/
        }
        else
        {
            //PrintDebug("m_internalOverlap - null");
        }
    }

    inline void Setup(size_t channelsCount)
    {
        mChannelsCount = channelsCount;

        m_Filter = new float *[channelsCount];
        std::memset(m_Filter, 0, sizeof(float *) * channelsCount);

        m_Overlap = new float *[channelsCount];
        std::memset(m_Overlap, 0, sizeof(float *) * channelsCount);

        m_internalFilter = new float *[channelsCount];
        std::memset(m_internalFilter, 0, sizeof(float *) * channelsCount);

        m_internalOverlap = new float *[channelsCount];
        std::memset(m_internalOverlap, 0, sizeof(float *) * channelsCount);
    }

    void Release()
    {
        if(!mChannelsCount)
        {
            return;
        }

        for(size_t channel(0); channel < mChannelsCount; ++channel)
        {
            delete [] m_Filter[channel], m_Filter[channel] = nullptr;

            if(mOverlapSet)
            {
                delete [] m_Overlap[channel], m_Overlap[channel] = nullptr;
            }
        }

        delete [] m_Filter, m_Filter = nullptr;
        delete [] m_Overlap, m_Overlap = nullptr;
        delete [] m_internalFilter, m_internalFilter = nullptr;
        delete [] m_internalOverlap, m_internalOverlap = nullptr;

        FilterLength.clear();
        OverlapLength.clear();

        mChannelsCount = 0;
    }

    inline void SetupFilter(size_t channelIndex, size_t length)
    {
        m_Filter[channelIndex] = new float[length];
        memset(m_Filter[channelIndex], 0, sizeof(float) * length);
        FilterLength[channelIndex] = length;
    }

    inline void SetupOverlap(size_t channelIndex, size_t length)
    {
        m_Overlap[channelIndex] = new float[length];
        memset(m_Overlap[channelIndex], 0, sizeof(float) * length);
        mOverlapSet = true;
        OverlapLength[channelIndex] = length;
    }

    inline void ReferOverlap(size_t channelIndex, const ovlAddFilterState & source)
    {
        m_Overlap[channelIndex] = source.m_Overlap[channelIndex];

        auto lengthIterator(source.OverlapLength.find(channelIndex));

        if(lengthIterator != source.OverlapLength.end())
        {
            OverlapLength[channelIndex] = (*lengthIterator).second;
        }
        else
        {
            assert(false);
        }
    }

    inline void FlushOverlap(size_t channelIndex)
    {
        size_t length = OverlapLength[channelIndex];
        assert(length);
        memset(m_Overlap[channelIndex], 0, length * sizeof(float));
    }
};
