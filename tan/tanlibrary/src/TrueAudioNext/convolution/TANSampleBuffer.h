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
#include "public/include/core/Buffer.h"
#include "Debug.h"

namespace amf
{
    struct TANSampleBuffer
    {
    protected:
        union
        {
            float           **host          = nullptr;

#ifndef TAN_NO_OPENCL
            cl_mem          *clmem;
#else
            amf::AMFBuffer  **
                            amfBuffers;
#endif
        }                   mChannels;

        bool                mChannelsAllocated
                                            = false;
        size_t              mChannelsCount  = 0;
        AMF_MEMORY_TYPE     mChannelsType   = AMF_MEMORY_UNKNOWN;

        bool                mBuffersAllocated
                                            = false;
        bool                mBuffersReferred = false;

    public:
        ~TANSampleBuffer()
        {
            Release();
        }

        void                Release()
        {
            if(mChannelsAllocated)
            {
#ifdef _DEBUG
                if(mBuffersAllocated)
                {
                    //verify that content buffers was properly deallocated previously
                    for(size_t channel(0); channel < mChannelsCount; ++channel)
                    {
                        if(IsHost())
                        {
                            assert(!mChannels.host[channel]);
                        }
#ifndef TAN_NO_OPENCL
                        else if(IsCL())
                        {
                            assert(!mChannels.clmem[channel]);
                        }
#else
                        else if(IsAMF())
                        {
                            assert(!mChannels.amfBuffers[channel]);
                        }
#endif
                    }
                }
#endif

                //deallocated channels itself
                if(IsHost())
                {
                    delete[] mChannels.host;
				    mChannels.host        = nullptr;
                }
#ifndef TAN_NO_OPENCL
                else if(IsCL())
                {
                    delete[] mChannels.clmem;
                    mChannels.clmem        = nullptr;
                }
#else
                else if(IsAMF())
                {
                    delete[] mChannels.amfBuffers;
				    mChannels.amfBuffers   = nullptr;
                }
#endif

                mChannelsAllocated              = false;
            }

            if(IsHost())
            {
                mChannels.host                  = nullptr;
            }
#ifndef TAN_NO_OPENCL
            else if(IsCL())
            {
                mChannels.clmem                 = nullptr;
            }
#else
            else if(IsAMF())
            {
                mChannels.amfBuffers            = nullptr;
            }
#endif
            //else
            //{
            //    assert(false);
            //}

            mChannelsType                       = AMF_MEMORY_UNKNOWN;
            mChannelsCount                      = 0;
        }

        //will deallocate buffer for each channel, don't remove channels itself
        void                                    DeallocateBuffers()
        {
            assert(mChannelsAllocated);
            assert(mChannelsType != AMF_MEMORY_UNKNOWN);
            assert(mBuffersAllocated);

            for(size_t channel(0); channel < mChannelsCount; ++channel)
            {
                if(IsHost())
                {
                    assert(mChannels.host[channel]);

                    std::cout << "delete host buffer " << std::hex << mChannels.host[channel] << std::dec << std::endl;

                    delete [] mChannels.host[channel];
                    mChannels.host[channel] = nullptr;
                }

#ifndef TAN_NO_OPENCL
                else if(IsCL())
                {
                    assert(mChannels.clmem[channel]);

                    DBG_CLRELEASE_MEMORYOBJECT(mChannels.clmem[channel]);
                }
#else
                else if(IsAMF())
                {
                    assert(mChannels.amfBuffers[channel]);

                    mChannels.amfBuffers[channel]->Release();
                    mChannels.amfBuffers[channel] = nullptr;
                }
#endif
            }

            mBuffersAllocated = false;
			mChannelsType = AMF_MEMORY_UNKNOWN;
        }

		TANSampleBuffer &                   operator=(const TANSampleBuffer & other)
		{
            //todo: add a != b and a.xx != b.xx

            if(IsBuffersAllocated())
            {
                DeallocateBuffers();
            }

            if(IsSet())
			{
				Release();
			}

			mChannelsAllocated              = false;
            mBuffersAllocated               = false;
            mBuffersReferred                = true;
			mChannelsType                   = other.mChannelsType;
			mChannels                       = other.mChannels;
            mChannelsCount                  = other.mChannelsCount;

			return *this;
		}

        inline void                         Debug(
            const std::string &             hint,
            size_t                          count
#ifndef TAN_NO_OPENCL
            , cl_command_queue              clCommandQueue = nullptr
#else
            , amf::AMFCompute *             amfCompute = nullptr
#endif
            ) const
        {
            assert(hint.length());

            auto channelsCount(mChannelsCount ? mChannelsCount : 2);

            for(size_t channel(0); channel < channelsCount; ++channel)
            {
                auto channelHint = hint + "[" + std::to_string(channel) + "]";

                if(IsHost() && mBuffersAllocated)
                {
                    //PrintArray(channelHint.c_str(), mChannels.host[0], count);
                }
    #ifndef TAN_NO_OPENCL
                else if(IsCL() && mBuffersAllocated)
                {
                    assert(clCommandQueue);
                    PrintCLArray(channelHint.c_str(), mChannels.clmem[0], clCommandQueue, count);
                }
    #else
                else if(IsAMF() && mBuffersAllocated)
                {
                    assert(amfCompute);
                    PrintAMFArray(channelHint.c_str(), mChannels.amfBuffers[0], amfCompute, count);
                }
    #endif
                else
                {
                    PrintDebug(channelHint.c_str());
                }
            }
        }

        inline bool                         IsSet() const   {return /*amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN != mChannelsType &&*/ mChannelsAllocated;}
        inline bool                         IsBuffersAllocated() const
                                                            {return mBuffersAllocated;}
        inline bool                         IsHost() const  {return amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST == mChannelsType;}
#ifndef TAN_NO_OPENCL
        inline bool                         IsCL() const    {return amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL == mChannelsType;}
#else
        inline bool                         IsAMF() const
        {
            return IsComputeBuffer();
        }
#endif

        inline bool                         IsComputeBuffer() const
        {
            return amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN != mChannelsType
                && amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST != mChannelsType;
        }

        inline const float * const * const        GetHostBuffers() const
        {
            assert(IsHost() && (mBuffersAllocated || mBuffersReferred));

            return GetHostBuffersReadWrite();
        }
        inline float ** const               GetHostBuffersReadWrite() const
        {
            assert(IsHost() && (mBuffersAllocated || mBuffersReferred));

            return mChannels.host;
        }

#ifndef TAN_NO_OPENCL
        inline cl_mem const * const         GetCLBuffers() const
        {
            assert(IsCL() && (mBuffersAllocated || mBuffersReferred));

            return mChannels.clmem;
        }

        inline cl_mem * const               GetCLBuffersReadWrite()
        {
            assert(IsCL());

            return mChannels.clmem;
        }
#else
        inline AMFBuffer * const * const    GetAMFBuffers() const
        {
            assert(IsAMF() && (mBuffersAllocated || mBuffersReferred));

            return mChannels.amfBuffers;
        }

        inline AMFBuffer ** const           GetAMFBuffersReadWrite()
        {
            assert(IsAMF());

            return mChannels.amfBuffers;
        }
#endif

        //todo: eliminate this function
        inline void                         MarkBuffersAllocated()
        {
            assert(IsSet() && mChannelsCount);

            for(size_t channel(0); channel < mChannelsCount; ++channel)
            {
                if(IsHost())
                {
                    assert(mChannels.host[channel]);
                }
#ifndef TAN_NO_OPENCL
                else if(IsCL())
                {
                    assert(mChannels.clmem[channel]);
                }
#else
                else if(IsAMF())
                {
                    assert(mChannels.amfBuffers[channel]);
                }
#endif
            }

            mBuffersAllocated = true;
        }

        inline AMF_MEMORY_TYPE              GetType() const     {return mChannelsType;}

        inline size_t                       GetChannelsCount() const
        {
            assert(mChannelsAllocated && mChannelsCount);

            return mChannelsCount;
        }

        void                                AllocateChannels(size_t channelsCount, amf::AMF_MEMORY_TYPE type)
        {
            assert(!mChannelsAllocated);

            mChannelsType = type;
            mChannelsAllocated = true;
            mChannelsCount = channelsCount;

            if(IsHost())
            {
                mChannels.host = new float *[channelsCount];
                std::memset(mChannels.host, 0, sizeof(float *) * channelsCount);
            }
#ifndef TAN_NO_OPENCL
            else if(IsCL())
            {
                mChannels.clmem = new cl_mem[channelsCount];
                std::memset(mChannels.clmem, 0, sizeof(cl_mem) * channelsCount);
            }
#else
            else if(IsAMF())
            {
                mChannels.amfBuffers = new amf::AMFBuffer *[channelsCount];
                std::memset(mChannels.amfBuffers, 0, sizeof(amf::AMFBuffer *) * channelsCount);
            }
#endif
        }

        //host
        void                                ReferHostChannels(float ** buffers/*, size_t channelsCount*/)
        {
            assert(!mChannelsAllocated);
            assert(!mChannelsCount);

            mChannelsType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST;
            mChannels.host = buffers;
            //mChannelsCount = channelsCount;
            mBuffersAllocated = true; //this is not correct in common case
        }

        void                                StoreHostChannels(size_t channelsCount, float ** buffers)
        {
            assert(!mChannelsAllocated);

            mChannelsAllocated = true;
            mChannelsType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_HOST;
            mChannels.host = buffers;
            mChannelsCount = channelsCount;
            mChannelsAllocated = true; //this is not correct in common case
        }

        void                                AllocateHostBuffers(size_t channelIndex, size_t size)
        {
            assert(IsSet() && IsHost() && (!mBuffersAllocated || channelIndex) && !mChannels.host[channelIndex]);

            mBuffersAllocated = true;
            mChannels.host[channelIndex] = new float[size];
            std::memset(mChannels.host[channelIndex], 0, sizeof(float) * size);
        }

        void                                ReferHostBuffer(size_t channelIndex, float * buffer)
        {
            assert(IsSet() && IsHost() && !mBuffersAllocated);

            mBuffersReferred = true;
            mChannels.host[channelIndex] = buffer;
        }

#ifndef TAN_NO_OPENCL
        //OpenCL
        void                                ReferCLChannels(cl_mem *buffers/*, size_t channelsCount*/)
        {
            assert(!mChannelsAllocated);
            assert(!mBuffersAllocated);
            assert(!mChannelsCount);

            mChannelsType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;

            mBuffersAllocated = true;
            mChannels.clmem = buffers;
            //mChannelsCount = channelsCount;
        }

        void                                StoreCLBuffers(size_t channelsCount, cl_mem* clBuffers)
        {
            assert(!mChannelsAllocated);

            mChannelsAllocated = true;
            mBuffersAllocated = true;
            mChannelsType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL;
            mChannels.clmem = clBuffers;
            mChannelsCount = channelsCount;
        }

        void                                ReferCLBuffers(size_t channelIndex, cl_mem buffer)
        {
            assert(IsSet() && IsCL() && !mBuffersAllocated);// || channelIndex)*/ /*&& !mChannels.amfBuffers[channelIndex]*/);

            mBuffersReferred = true;
            mChannels.clmem[channelIndex] = buffer;
        }
#else
        //AMF
        void                                ReferAMFChannels(amf::AMFBuffer ** amfBuffers/*, size_t channelsCount*/)
        {
            assert(!mChannelsAllocated);
            assert(!mChannelsCount);
            assert(amfBuffers && amfBuffers[0]);

            mChannelsType = amfBuffers[0]->GetMemoryType();

            mBuffersAllocated = true; //this is a not correct for common case
            mChannels.amfBuffers = amfBuffers;
            //mChannelsCount = channelsCount;
        }

        void                                StoreAMFChannels(size_t channelsCount, amf::AMFBuffer ** amfBuffers)
        {
            assert(!mChannelsAllocated);
            assert(channelsCount);

            auto type = amfBuffers[0] ? amfBuffers[0]->GetMemoryType() : amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN;

            for(size_t channel(1); channel < channelsCount; ++channel)
            {
                assert(amfBuffers[channel] ? amfBuffers[channel]->GetMemoryType() == type : amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN == type);
            }

            mChannelsAllocated = true;
            mBuffersAllocated = true; //this is a not correct for common case
            mChannelsType = type;
            mChannels.amfBuffers = amfBuffers;
            mChannelsCount = channelsCount;
        }

        void                                ReferAMFBuffers(size_t channelIndex, amf::AMFBuffer * buffer)
        {
            assert(IsSet() && IsAMF() && !mBuffersAllocated/*) || channelIndex)*/ /*&& !mChannels.amfBuffers[channelIndex]*/);

            mBuffersReferred = true;
            mChannels.amfBuffers[channelIndex] = buffer;
        }

#endif

        /*void FreeHostData(size_t channelIndex)
        {
            assert(mChannels.host[channelIndex]);

            delete [] mChannels.host[channelIndex], mChannels.host[channelIndex] = nullptr;
        }*/
    };
}
