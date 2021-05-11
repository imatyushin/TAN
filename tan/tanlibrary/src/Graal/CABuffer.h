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
        assert(!mSize);
        assert(!mComputeBufferAllocated);
        assert(!mSystemMemoryAllocated);

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

    size_t                      mSize = 0;
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

    AMF_RESULT create(T *buffer, size_t size, uint32_t flags, amf::AMF_MEMORY_TYPE memoryType)
    {
        AMF_RETURN_IF_FALSE(size != 0, AMF_INVALID_ARG);

        AMF_RESULT ret = AMF_OK;

        mMemoryType = memoryType;

#ifndef TAN_NO_OPENCL
        mBufferCL = clCreateBuffer(
            mContext,
            flags,
            size * sizeof(T),
            const_cast<void *>(buffer),
            &ret
            );

        if(ret != CL_SUCCESS)
        {
#ifdef _DEBUG_PRINTF
            printf("error creating buffer: %d\n", ret);
#endif
            AMF_ASSERT(ret == CL_SUCCESS, L"error creating buffer: %d", ret);
            return ret == CL_INVALID_BUFFER_SIZE ? AMF_OUT_OF_MEMORY : AMF_FAIL;
        }
#else
        AMF_RETURN_IF_FAILED(
            mContext->AllocBuffer(
                mMemoryType,
                size * sizeof(T),
                &mBufferAMF
                )
            );
#endif

        mSize = size;
        mComputeBufferAllocated = true;
        mFlags = flags;
        mSystemMemory = buffer;

        return ret;
    }

    AMF_RESULT create(size_t size, uint32_t flags, amf::AMF_MEMORY_TYPE memoryType)
    {
        return create(nullptr, size, flags, memoryType);
    }

// TO DO :: CORECT
    AMF_RESULT attach(const T *buffer, size_t size)
    {
        AMF_RESULT ret = AMF_OK;
        uint32_t flags = mFlags;
        bool old_sys_own = mSystemMemoryAllocated;
        T * old_ptr = mSystemMemory;

        if ( size > mSize ) {
            release();
            AMF_RETURN_IF_FAILED(create(size, flags));
        }

        if ( mSystemMemoryAllocated && mSystemMemory  && old_ptr != buffer)
        {
            delete [] mSystemMemory;
            mSystemMemory = 0;
        }

        mSystemMemory = (T*)buffer;
        mSize = size;
        mSystemMemoryAllocated = (old_ptr != buffer) ? false : old_sys_own;

        return ret;
    }

// TO DO : CORRECT
#ifndef TAN_NO_OPENCL
    AMF_RESULT attach(
        cl_mem buffer,
        size_t size
        )
    {
        AMF_RESULT ret = AMF_OK;

        if(buffer != mBufferCL || size > mSize)
        {
            AMF_RETURN_IF_FAILED(release());
            mBufferCL = buffer;
            mSize = size;
            mComputeBufferAllocated = false;
        }

        return ret;
    }
#else
    AMF_RESULT attach(
        amf::AMFBuffer * buffer,
        size_t size
        )
    {
        AMF_RESULT ret = AMF_OK;

        if(buffer != mBufferAMF || size > mSize)
        {
            AMF_RETURN_IF_FAILED(release());

            mBufferAMF = buffer;
            mSize = size;
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
                    mSize*sizeof(T),
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
                mSize*sizeof(T),
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
                                                mSize*sizeof(T),
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

        if (size != -1 && (size > mSize || !data))
        {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToDevice: wrong data");
            return AMF_INVALID_ARG;
        }

        if(!mBufferCL)
        {
            create(mSize * sizeof(T), flags);
        }

        size_t len = (size != -1 )? size : mSize;

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
    AMF_RESULT copyToDevice(const amf::AMFComputePtr & commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        if (size != -1 && (size > mSize || !data))
        {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToDevice: wrong data");
            return AMF_INVALID_ARG;
        }

        if (!mBufferAMF)
        {
            AMF_RETURN_IF_FAILED(create(mSize * sizeof(T), flags, commandQueue->GetMemoryType()));
        }

        size_t len = (size != -1 ) ? size : mSize;

        const T * sys_ptr = (size != -1) ? data : mSystemMemory;
        AMF_RETURN_IF_INVALID_POINTER(
            sys_ptr,
            L"Internal error: buffer hasn't been preallocated"
            );

        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferToHost(
                mBufferAMF,
                offset * sizeof(T),
                len * sizeof(T),
                (void *)sys_ptr,
                true
                )
            );

        return AMF_OK;
    }
#endif

#ifndef TAN_NO_OPENCL
    AMF_RESULT copyToDeviceNonBlocking(cl_command_queue commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        if (size!=-1 && (size > mSize || !data)) {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToDeviceA: wrong data");

            return AMF_INVALID_ARG;
        }

        if ( !mBufferCL )
        {
            AMF_RETURN_IF_FAILED(create(mSize * sizeof(T), flags));
        }

        size_t len = (size!=-1 )? size : mSize;
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
    AMF_RESULT copyToDeviceNonBlocking(const amf::AMFComputePtr & commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;

        if (size!=-1 && (size > mSize || !data)) {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToDeviceA: wrong data");

            return AMF_INVALID_ARG;
        }

        if (!mBufferAMF)
        {
            AMF_RETURN_IF_FAILED(create(mSize * sizeof(T), flags, commandQueue->GetMemoryType()));
        }

        size_t len = (size!=-1 )? size : mSize;
        const T * sys_ptr = (size!=-1) ? data : mSystemMemory;
        AMF_RETURN_IF_INVALID_POINTER(sys_ptr,
                            L"Internal error: buffer hasn't been preallocated");
        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferFromHost(
                sys_ptr,
                len * sizeof(T),
                mBufferAMF,
                offset * sizeof(T),
                false
                )
            );

        return AMF_OK;
    }
#endif

#ifndef TAN_NO_OPENCL
    AMF_RESULT copyToHost(cl_command_queue commandQueue)
    {
        AMF_RESULT err = AMF_OK;

        if(mSize == 0 || !mBufferCL) {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToHost: wrong data");
            return(-1);
        }

        if ( !mSystemMemory )
        {
            mSystemMemory = new T[mSize];
            if(!mSystemMemory )
            {
                err = AMF_FAIL;
#ifdef _DEBUG_PRINTF
                printf("error creating buffer: %d\n", err);
#endif
                AMF_ASSERT(false, L"error creating buffer: %d\n", err);
                return err;
            }
            mSystemMemoryAllocated = true;
        }

        AMF_RETURN_IF_CL_FAILED(
            clEnqueueReadBuffer(
                commandQueue,
                mBufferCL,
                CL_TRUE,
                0,
                mSize * sizeof(T),
                mSystemMemory,
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

        if(mSize == 0 || !mBufferAMF) {
#ifdef _DEBUG_PRINTF
            printf("wrong data\n");
#endif
            AMF_ASSERT(false, L"copyToHost: wrong data");
            return AMF_INVALID_ARG;
        }

        if(!mSystemMemory)
        {
            mSystemMemory = new T[mSize];
            if(!mSystemMemory )
            {
                err = AMF_FAIL;
#ifdef _DEBUG_PRINTF
                printf("error creating buffer: %d\n", err);
#endif
                AMF_ASSERT(false, L"error creating buffer: %d\n", err);
                return err;
            }
            mSystemMemoryAllocated = true;
        }

        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferToHost(
                mBufferAMF,
                0,
                mSize * sizeof(T),
                mSystemMemory,
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
                mSize * sizeof(T),
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
                mSize * sizeof(T),
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
        for( int i = 0; i < mSize; i++)
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
    AMF_RESULT setValue2(cl_command_queue commandQueue, T _val, size_t size = 0, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;
        int len = (size != 0) ? size: mSize;
        if (NULL != commandQueue)
        {
            printf("todo: fix sync issue\n!");

            cl_event event = clCreateUserEvent(context, &returnCode);
            if(returnCode != CL_SUCCESS)
            {
                return returnCode;
            }

            AMF_RETURN_IF_CL_FAILED(
                clEnqueueFillBuffer(
                    commandQueue,
                    mBufferCL,
                    (const void *)&_val,
                    sizeof(_val),
                    offset * sizeof(T),
                    len * sizeof(T),
                    0,
                    nullptr,
                    &event
                    )
                );
        }

        for (int i = offset; mSystemMemory && i < len; i++)
        {
            mSystemMemory[i] = _val;
        }

        return err;
    }
#else
    AMF_RESULT setValue2(amf::AMFCompute * commandQueue, T _val, size_t size = 0, size_t offset = 0)
    {
        AMF_RESULT err = AMF_OK;
        int len = (size != 0) ? size: mSize;

        if(commandQueue)
        {
            printf("todo: fix sync issue\n!");

            AMF_RETURN_IF_FAILED(
                commandQueue->FillBuffer(
                    mBufferAMF,
                    offset * sizeof(T),
                    len * sizeof(T),
                    (const void *)&_val,
                    sizeof(_val)
                    )
                );
        }

        for (int i = offset; mSystemMemory && i < len; i++)
        {
            mSystemMemory[i] = _val;
        }

        return err;
    }
#endif

    AMF_RESULT release()
    {
        AMF_RESULT ret = AMF_OK;
        if ( mSystemMemoryAllocated && mSystemMemory)
        {
            delete [] mSystemMemory;
            mSystemMemory = 0;
        }
        mSystemMemoryAllocated  = false;

        if ( mComputeBufferAllocated )
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
        mSize = 0;

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
    inline const cl_mem &                   GetBuffer() const
    {
        return mBufferCL;
    }
#else
    inline /*const*/ amf::AMFBuffer *           GetBuffer() const
    {
        return mBufferAMF;
    }
#endif

    inline virtual T * & getSysMem(void)
    {
        if (!mSystemMemory)
        {
            mSystemMemory = new T[mSize];

            if (!mSystemMemory)
            {
#ifdef _DEBUG_PRINTF
#ifdef TAN_SDK_EXPORTS
                AMF_ASSERT(false, L"Cannot allocate memory: %lu", mSize * sizeof(T));
#else
                printf("error creating bufffer: %d\n", ret);
#endif
#endif
                return mSystemMemory;
            }

            mComputeBufferAllocated = true;
        }

        return(mSystemMemory);
    }

    inline T * & getMappedMem(void)
    {
        return(mMappedMemory);
    }

    inline void setSysOwnership(bool own )
    {
        mSystemMemoryAllocated = own;
    }

    inline bool getSysOwnership(void)
    {
        return(mSystemMemoryAllocated);
    }

    // DANGEROUS
    inline void setLen(size_t size)
    {
        mSize = size;
    }

    inline size_t getLen(void)
    {
        return(mSize);
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

    T *                         mSystemMemory = nullptr;
    T *                         mMappedMemory = nullptr;

#ifndef TAN_NO_OPENCL
    cl_mem                      mBufferCL = nullptr;
#else
    amf::AMFBuffer *            mBufferAMF = nullptr;
#endif

    size_t                      mSize = 0;
    uint32_t                    mFlags = 0;
    bool                        mSystemMemoryAllocated = false;
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

    AMF_RESULT create(size_t offset, size_t size, uint32_t flags)
    {
        AMF_RESULT ret = AMF_OK;

        AMF_RETURN_IF_FALSE(size > 0, AMF_OK);

        mOffset = offset;

#ifndef TAN_NO_OPENCL
        cl_buffer_region region = {0};

        region.origin = offset * sizeof(T);
        region.size = size * sizeof(T);

        auto subBuffer = clCreateSubBuffer(
            GetBuffer(),
            flags,
            CL_BUFFER_CREATE_TYPE_REGION,
            &region,
            &ret
            );
        AMF_RETURN_IF_CL_FAILED(subBuffer);

        CABuf<T>::mBufferCL = subBuffer;
#else
        amf::AMFBufferExPtr bufferEx(CABuf<T>::mBufferAMF);
        AMF_RETURN_IF_FALSE(bufferEx != nullptr, AMF_INVALID_ARG);

        amf::AMFBufferPtr subBuffer;

        AMF_RETURN_IF_FAILED(
            bufferEx->CreateSubBuffer(
                &subBuffer,
                offset * sizeof(T),
                size * sizeof(T)
                )
            );

        CABuf<T>::mBufferAMF = subBuffer;
#endif

        CABuf<T>::mSize = size;
        CABuf<T>::mComputeBufferAllocated = true;
        CABuf<T>::mFlags = flags;

        return ret;
    }

    int create(size_t size, uint32_t flags)
    {
        create(0, size, flags);
    }

    T * & getSysMem(void) override
    {
        if(!CABuf< T >::mSystemMemory)
        {
            CABuf< T >::mSystemMemory = getSysMem();

                                                     //todo: investigate retcode usage
            AMF_RETURN_IF_FALSE(CABuf< T >::mSystemMemory != nullptr, CABuf< T >::mSystemMemory, L"Internal error: subbuffer's parent failed to allocate system memory");
            CABuf< T >::mSystemMemory += mOffset;
        }

        return CABuf< T >::mSystemMemory;
    }

protected:
    size_t mOffset = 0;
};

}
