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

#include "public/include/core/Context.h"
#include "public/include/core/Compute.h"
#include "public/include/core/Buffer.h"

//#include <iostream>

namespace graal
{

template<typename T>
class CABuf
{
public:
#ifndef TAN_NO_OPENCL
    CABuf(cl_context context):
        mContext(context)
    {
    }
#else
    CABuf(const amf::AMFContextPtr & context):
        mContext(context)
    {
    }
#endif

    /*// TO DO : correct copy costructor, operator =, clone()
    CABuf(const CABuf & src)
    {
        assert(!mCount);
        assert(!mComputeBufferAllocated);
        assert(!mSystemMemory.size());

        mContext = src.mContext;
        mMappingQueue = src.mMappingQueue;
        mMemoryType = src.mMemoryType;
        mComputeKernel = src.mComputeKernel;
        mSystemMemory = nullptr;
    T *                         mMappedMemory = nullptr;

#ifndef TAN_NO_OPENCL
    cl_mem                      mBufferCL = nullptr;
#else
    amf::AMFBuffer *            mBufferAMF = nullptr;
#endif

    size_t                      mCount = 0;
    uint32_t                    mFlags = 0;
    bool                        mSystemMemoryAllocated = false;
    bool                        mComputeBufferAllocated = false;

    }*/

    CABuf()
    {
    }

    virtual ~CABuf()
    {
        release();
    }

/*
    CABuf clone(void)
    {
        CABuf new_buf(mContext);
        return(new_buf);
    }
*/

    virtual AMF_RESULT create(size_t count, uint32_t flags, amf::AMF_MEMORY_TYPE memoryType)
    {
        AMF_RETURN_IF_FALSE(count != 0, AMF_INVALID_ARG);

        if(!mSystemMemory.size())
        {
            AMF_RETURN_IF_FAILED(CreateSystemMemory(count));
        }

        assert(mCount == count);

        AMF_RESULT ret = AMF_OK;

#ifndef TAN_NO_OPENCL
        cl_int success(CL_SUCCESS);

        mBufferCL = clCreateBuffer(
            mContext,
            flags,
            count * sizeof(T),
            nullptr, //buffer removed because not used anywhere
            &success
            );
        AMF_RETURN_IF_CL_FAILED(success);
        AMF_RETURN_IF_FALSE(mBufferCL != nullptr, AMF_FAIL);
#else
        mMemoryType = memoryType;

        AMF_RETURN_IF_FAILED(
            mContext->AllocBuffer(
                mMemoryType,
                count * sizeof(T),
                &mBufferAMF
                )
            );
#endif

        mComputeBufferAllocated = true;
        mFlags = flags;

        return ret;
    }

    inline AMF_RESULT CreateSystemMemory(size_t count)
    {
        assert(!mCount);
        assert(!mSystemMemory.size());
        assert(!mComputeBufferAllocated);

        mSystemMemory.resize(count);
        AMF_RETURN_IF_FALSE(mSystemMemory.size() == count, AMF_OUT_OF_MEMORY);

        mCount = count;

        return AMF_OK;
    }

    /*
    // TO DO :: CORECT
    AMF_RESULT attach(const T *buffer, size_t size)
    {
        AMF_RESULT ret = AMF_OK;
        uint32_t flags = mFlags;

        bool old_sys_own = mSystemMemoryAllocated;
        T * old_ptr = mSystemMemory;

        if ( size > mCount ) {
            release();
            AMF_RETURN_IF_FAILED(create(size, flags));
        }

        if( mSystemMemoryAllocated && mSystemMemory  && old_ptr != buffer)
        {
            delete [] mSystemMemory;
            mSystemMemory = 0;
        }

        mSystemMemory = (T*)buffer;
        mCount = size;
        mSystemMemoryAllocated = (old_ptr != buffer) ? false : old_sys_own;

        return ret;
    }*/

// TO DO : CORRECT
#ifndef TAN_NO_OPENCL
    AMF_RESULT attach(cl_mem buffer, size_t size)
    {
        if(buffer != mBufferCL || size > mCount)
        {
            AMF_RETURN_IF_FAILED(release());

            mBufferCL = buffer;
            mCount = size;
            mComputeBufferAllocated = false;
        }

        return AMF_OK;
    }
#else
    AMF_RESULT attach(amf::AMFBuffer * buffer, size_t size)
    {
        AMF_RESULT ret = AMF_OK;

        if(buffer != mBufferAMF || size > mCount)
        {
            AMF_RETURN_IF_FAILED(release());

            mBufferAMF = buffer;
            mCount = size;
            mComputeBufferAllocated = false;
        }

        return ret;
    }
#endif

#ifndef TAN_NO_OPENCL
    T*  map(cl_command_queue _mappingQ, uint32_t flags)
    {
        T* ret = 0;
        int status = 0;

        if ( mBufferCL && !mMappedMemory ) {
            mMappingQueue = _mappingQ;

                ret = mMappedMemory = (T *)clEnqueueMapBuffer(
                    mMappingQueue,
                    mBufferCL,
                    CL_TRUE,
                    flags, //CL_MAP_WRITE_INVALIDATE_REGION,
                    0,
                    mCount*sizeof(T),
                    0,
                    NULL,
                    NULL,
                    &status
                    );
        }

        return ret;
    }
#else
    T*  map(const amf::AMFComputePtr & mappingQueue, uint32_t flags)
    {
        throw "Not implemented";

        T* ret = 0;
        /*int status = 0;

        if(mBufferAMF && !mMappedMemory)
        {
            mMappingQueue = mappingQueue;

            ret = mMappedMemory = (T *)clEnqueueMapBuffer(mMappingQueue,
                mBufferCL,
                CL_TRUE,
                flags, //CL_MAP_WRITE_INVALIDATE_REGION,
                0,
                mCount*sizeof(T),
                0,
                NULL,
                NULL,
                &status);
        }*/

        return ret;
    }
#endif

    //seems unneeded
    /*
    T*  mapA(cl_command_queue _mappingQ, uint32_t flags, cl_event *_wait_event = NULL, cl_event *_set_event = NULL )
    {
        T* ret = 0;
        int status = 0;
        if ( mBufferCL && !mMappedMemory ) {

            int n_wait_events = 0;
            cl_event * p_set_event = _set_event;
            cl_event * p_wait_event = _wait_event;
            if (_wait_event != NULL)
            {
                n_wait_events = 1;
            }


            mMappingQueue = _mappingQ;

            ret = mMappedMemory = (T *)clEnqueueMapBuffer (mMappingQueue,
                                                mBufferCL,
                                                CL_FALSE,
                                                flags, //CL_MAP_WRITE_INVALIDATE_REGION,
                                                0,
                                                mCount*sizeof(T),
                                                n_wait_events,
                                                p_wait_event,
                                                p_set_event,
                                                &status);
            if (_wait_event != NULL)
            {
                clReleaseEvent(*_wait_event);
                *_wait_event = NULL;
            }


        }
        return ret;
    }*/

#ifndef TAN_NO_OPENCL
    AMF_RESULT unmap(cl_event *_wait_event = NULL, cl_event *_set_event = NULL)
    {
        throw "Not implemented";

        /*AMF_RESULT ret = AMF_OK;
        if ( mBufferCL && mMappedMemory && mMappingQueue) {

            int n_wait_events = 0;
            cl_event * p_set_event = _set_event;
            cl_event * p_wait_event = _wait_event;
            if (_wait_event != NULL)
            {
                n_wait_events = 1;
            }

            ret = clEnqueueUnmapMemObject(mMappingQueue,
                mBufferCL,
                mMappedMemory,
                n_wait_events,
                p_wait_event,
                p_set_event
                );

            if (_wait_event != NULL)
            {
                clReleaseEvent(*_wait_event);
                *_wait_event = NULL;
            }

            mMappedMemory = 0;
            mMappingQueue = 0;
        }
        return ret;*/
    }
#else
    AMF_RESULT unmap()
    {
        throw "Not implemented";
    }
#endif

#ifndef TAN_NO_OPENCL
    AMF_RESULT copyToDevice(cl_command_queue commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        if (size != -1 && (size > mCount || !data))
        {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToDevice: wrong data");
            return AMF_INVALID_ARG;
        }

        if(!mBufferCL)
        {
            //todo: ivm: seems incorrect
            //create(mCount * sizeof(T), flags, amf::AMF_MEMORY_OPENCL);
            create(mCount, flags, amf::AMF_MEMORY_OPENCL);
        }

        size_t len = (size != -1 )? size : mCount;

        const T * sys_ptr = (size != -1) ? data : mSystemMemory;
        AMF_RETURN_IF_INVALID_POINTER(sys_ptr,
                            L"Internal error: buffer hasn't been preallocated");
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueWriteBuffer(
                commandQueue,
                mBufferCL,
                CL_TRUE,
                offset * sizeof(T),
                len * sizeof(T),
                sys_ptr,
                0,
                NULL,
                NULL
                ),
            L"Error: writing data to device"
            );

        return AMF_OK;
    }
#else
    AMF_RESULT copyToDevice(const amf::AMFComputePtr & commandQueue, uint32_t flags)
    {
        assert(mCount);
        assert(mSystemMemory.size());

        AMF_RESULT err = AMF_OK;

        if (!mBufferAMF)
        {
            //todo: ivm: seems incorrect size calc
            //AMF_RETURN_IF_FAILED(create(mCount * sizeof(T), flags, commandQueue->GetMemoryType()));
            //todo: ivm: seems correct size calc
            AMF_RETURN_IF_FAILED(create(mCount, flags, commandQueue->GetMemoryType()));
        }

        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferFromHost(
                &mSystemMemory.front(),
                mCount * sizeof(T),
                mBufferAMF,
                0, //offset
                true //blocking
                )
            );

        return AMF_OK;
    }
#endif

#ifndef TAN_NO_OPENCL
    AMF_RESULT copyToDeviceNonBlocking(cl_command_queue commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        if (size!=-1 && (size > mCount || !data)) {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToDeviceA: wrong data");

            return AMF_INVALID_ARG;
        }

        if ( !mBufferCL )
        {
            AMF_RETURN_IF_FAILED(create(mCount * sizeof(T), flags, amf::AMF_MEMORY_TYPE::AMF_MEMORY_OPENCL));
        }

        size_t len = (size!=-1 )? size : mCount;
        const T * sys_ptr = (size!=-1) ? data : mSystemMemory;
        AMF_RETURN_IF_INVALID_POINTER(sys_ptr,
                            L"Internal error: buffer hasn't been preallocated");
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueWriteBuffer(
                commandQueue,
                mBufferCL,
                CL_FALSE,
                offset * sizeof(T),
                len * sizeof(T),
                sys_ptr,
                0,
                NULL,
                NULL
            ),
            L"Error: writing data to device failed"
            );

        return AMF_FAIL;
    }
#else
    AMF_RESULT copyToDeviceNonBlocking(
        const amf::AMFComputePtr &  commandQueue,
        uint32_t                    flags,
        const T *                   data            = NULL,
        size_t                      count           = -1,
        size_t                      offset          = 0
        )
    {
        //todo: implement flags for metal

        AMF_RESULT err = AMF_OK;

        if(count != -1 && (count > mCount || !data))
        {
            AMF_ASSERT(false, L"copyToDeviceA: wrong data");

            return AMF_INVALID_ARG;
        }

        if(!mBufferAMF)
        {
            AMF_RETURN_IF_FAILED(
                create(mCount * sizeof(T), flags, commandQueue->GetMemoryType())
                );
        }

        size_t countToCopy = (count != -1) ? count : mCount;
        auto systemMemory = data ? data : &mSystemMemory.front();

        AMF_RETURN_IF_INVALID_POINTER(systemMemory, L"Internal error: buffer hasn't been preallocated");
        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferFromHost(
                systemMemory,
                countToCopy * sizeof(T),
                mBufferAMF,
                offset * sizeof(T),
                true
                )
            );

        return AMF_OK;
    }
#endif

#ifndef TAN_NO_OPENCL
    AMF_RESULT copyToHost(cl_command_queue commandQueue)
    {
        AMF_RESULT err = AMF_OK;

        AMF_RETURN_IF_FALSE(!!mCount && !!mBufferCL, AMF_FAIL);

        AMF_RETURN_IF_CL_FAILED(
            clEnqueueReadBuffer(
                commandQueue,
                mBufferCL,
                CL_TRUE,
                0,
                mCount * sizeof(T),
                GetSystemMemory(),
                0,
                NULL,
                NULL
                )
            );

        return AMF_OK;
    }
#else
    AMF_RESULT copyToHost(const amf::AMFComputePtr & commandQueue)
    {
        AMF_RESULT err = AMF_OK;

        if(!mCount || !mBufferAMF)
        {
            AMF_ASSERT(false, L"copyToHost: wrong data");
            return AMF_INVALID_ARG;
        }

        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferToHost(
                mBufferAMF,
                0,
                mCount * sizeof(T),
                GetSystemMemory(),
                true
                )
            );

        return AMF_OK;
    }
#endif

    /*
#ifndef TAN_NO_OPENCL
    AMF_RESULT copy(CABuf<T> & src, cl_command_queue commandQueue, size_t src_offset = 0, size_t dst_offset = 0)
    {
        assert(mBufferCL);

        AMF_RETURN_IF_CL_FAILED(
            clEnqueueCopyBuffer(
                commandQueue,
                src.GetBuffer(),
                mBufferCL,
                src_offset,
                dst_offset,
                mCount * sizeof(T),
                0,
                NULL,
                NULL
                )
            );

        return AMF_OK;
    }
#else
    AMF_RESULT copy(CABuf<T> & src, const amf::AMFComputePtr & commandQueue, size_t src_offset = 0, size_t dst_offset = 0)
    {
        assert(mBufferAMF);

        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBuffer(
                src.GetBuffer(),
                src_offset,
                mCount * sizeof(T),
                mBufferAMF,
                dst_offset
                )
            );

        return AMF_OK;
    }
#endif
    */

#ifndef TAN_NO_OPENCL
    AMF_RESULT CopyBufferMemory(const cl_mem src, cl_command_queue commandQueue, size_t size)
    {
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueCopyBuffer(
                commandQueue,
                src,
                mBufferCL,
                0,
                0,
                size,
                0,
                NULL,
                NULL
                )
            );

        return AMF_OK;
    }
#else
    AMF_RESULT CopyBufferMemory(const amf::AMFBuffer * src, const amf::AMFComputePtr & commandQueue, size_t size)
    {
        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBuffer(
                const_cast<amf::AMFBuffer *>(src),
                0,
                size,
                mBufferAMF,
                0
                )
            );

        return AMF_OK;
    }
#endif

    /*AMF_RESULT setValue(cl_command_queue commandQueue, T _val)
    {
        AMF_RESULT err = AMF_OK;

        T * map_ptr = map(commandQueue, CL_MAP_WRITE_INVALIDATE_REGION);
        for( int i = 0; i < mCount; i++)
        {
            map_ptr[i] = _val;
            if ( mSystemMemory )
            {
                mSystemMemory[i] = _val;
            }
        }
        unmap();
        return(err);
    }*/

#ifndef TAN_NO_OPENCL
    AMF_RESULT setValue2(cl_command_queue commandQueue, T value, size_t count = 0, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        int count2Fill = (count != 0) ? count : mCount;

        if(commandQueue)
        {
            cl_int returnCode(CL_SUCCESS);

            cl_event event = clCreateUserEvent(mContext, &returnCode);
            AMF_RETURN_IF_CL_FAILED(returnCode);

            AMF_RETURN_IF_CL_FAILED(
                clEnqueueFillBuffer(
                    commandQueue,
                    mBufferCL,
                    (const void *)&value,
                    sizeof(value),
                    offset * sizeof(T),
                    count2Fill * sizeof(T),
                    0,
                    nullptr,
                    &event
                    )
                );
            AMF_RETURN_IF_CL_FAILED(
                clWaitForEvents(1, &event)
                );
        }

        for (int i = offset; mSystemMemory && i < count2Fill; i++)
        {
            mSystemMemory[i] = value;
        }

        return err;
    }
#else
    AMF_RESULT setValue2(amf::AMFCompute * compute, T value, size_t count = 0, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        size_t count2Fill = (count != 0) ? count : mCount;
        size_t size2FillInBytes = count2Fill * sizeof(T);
        size_t offsetInBytes = offset * sizeof(T);
        assert(offsetInBytes + size2FillInBytes <= mBufferAMF->GetSize());

        if(!value)
        {
            assert(!offsetInBytes && size2FillInBytes == mBufferAMF->GetSize());
        }

        if(compute)
        {
            AMF_RETURN_IF_FAILED(
                compute->FillBuffer(
                    mBufferAMF,
                    offset * sizeof(T),
                    count2Fill * sizeof(T),
                    &value,
                    sizeof(T)
                    )
                );
        }

        if(mSystemMemory.size())
        {
            for(int i = offset; i < count2Fill; i++)
            {
                mSystemMemory[i] = value;
            }
        }

        return err;
    }
#endif

    AMF_RESULT release()
    {
        AMF_RESULT ret = AMF_OK;

        mSystemMemory.resize(0);

        if(mComputeBufferAllocated)
        {
            unmap();

#ifndef TAN_NO_OPENCL
            if ( mBufferCL )
            {
                if (clReleaseMemObject(mBufferCL) != CL_SUCCESS) {
                    ret = AMF_OPENCL_FAILED;
#ifdef TAN_SDK_EXPORTS
                    AMF_ASSERT(false, L"clReleaseMemObject() failed");
#endif
                }
            }
            mBufferCL = 0;
#else
            mBufferAMF = nullptr;
#endif
        }

        mComputeBufferAllocated = false;
        mCount = 0;

        assert(!mMappedMemory);

        return ret;
    }

    /*
#ifndef TAN_SDK_EXPORTS
    inline void setContext(cl_context _context)
    {
        mContext = _context;
    }
#endif

    inline cl_context getContext(void)
    {
        return(mContext);
    }
    */

#ifndef TAN_NO_OPENCL
    inline cl_mem                           GetBuffer()
    {
        assert(mBufferCL);
        return mBufferCL;
    }
#else
    inline amf::AMFBuffer *                 GetBuffer()
    {
        return mBufferAMF;
    }
#endif

    virtual T *                             GetSystemMemory()
    {
        assert(mSystemMemory.size());

        return &mSystemMemory.front();
    }

    inline size_t                           GetCount() const
    {
        return mCount;
    }

protected:
#ifndef TAN_NO_OPENCL
    cl_context                  mContext;
    cl_command_queue            mMappingQueue;
#else
    amf::AMF_MEMORY_TYPE        mMemoryType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN;
    amf::AMFContextPtr          mContext;
    amf::AMFComputePtr          mMappingQueue;
    amf::AMFComputeKernelPtr    mComputeKernel;
#endif

    std::vector<T>              mSystemMemory;
    T *                         mMappedMemory = nullptr;

#ifndef TAN_NO_OPENCL
    cl_mem                      mBufferCL = nullptr;
#else
    amf::AMFBuffer *            mBufferAMF = nullptr;
#endif

    size_t                      mCount = 0;
    uint32_t                    mFlags = 0;
    //bool                        mSystemMemoryAllocated = false;
    bool                        mComputeBufferAllocated = false;
};

template<typename T>
class CASubBuf: public CABuf<T>
{
public:
    CASubBuf(CABuf<T> & base):
        CABuf< T >(base)
    {
        assert(CABuf<T>::GetBuffer());
    }
                                   //count, not a size in bytes
    AMF_RESULT Create(size_t offset, size_t count, uint32_t flags)
    {
        assert(count);
        AMF_RETURN_IF_FALSE(count > 0, AMF_FAIL);

        auto offsetInBytes = offset * sizeof(T);
        auto sizeInBytes = count * sizeof(T);

        assert(offsetInBytes + sizeInBytes <= CABuf<T>::GetBuffer()->GetSize());

        //std::cout << sizeInBytes << offsetInBytes << std::endl;

#ifndef TAN_NO_OPENCL
        cl_buffer_region region = {0};

        region.origin = offsetInBytes;
        region.size = sizeInBytes;

        cl_int returnCode(CL_SUCCESS);
        auto subBuffer = clCreateSubBuffer(
            CABuf<T>::GetBuffer(),
            flags,
            CL_BUFFER_CREATE_TYPE_REGION,
            &region,
            &returnCode
            );
        AMF_RETURN_IF_CL_FAILED(returnCode);
        AMF_RETURN_IF_FALSE(subBuffer != nullptr, AMF_OK);

        CABuf<T>::mBufferCL = subBuffer;
#else
        amf::AMFBufferExPtr bufferEx(CABuf<T>::mBufferAMF);
        AMF_RETURN_IF_FALSE(bufferEx != nullptr, AMF_INVALID_ARG);

        amf::AMFBufferPtr subBuffer;

        AMF_RETURN_IF_FAILED(
            bufferEx->CreateSubBuffer(
                &subBuffer,
                offsetInBytes,
                sizeInBytes
                )
            );

        CABuf<T>::mBufferAMF = subBuffer;
#endif

        CABuf<T>::mCount = count;
        CABuf<T>::mComputeBufferAllocated = true;
        CABuf<T>::mFlags = flags;
        mOffset = offset;

        return AMF_OK;
    }

    /*virtual AMF_RESULT create(size_t size, uint32_t flags)
    {
        create(0, size, flags);
    }*/

    T * GetSystemMemory() override
    {
        auto systemMemory(CABuf<T>::GetSystemMemory());
        AMF_RETURN_IF_FALSE(!!systemMemory, nullptr, L"Internal error: subbuffer's parent failed to allocate system memory");

        return systemMemory + mOffset;
    }

protected:
    size_t mOffset = 0;
};

}