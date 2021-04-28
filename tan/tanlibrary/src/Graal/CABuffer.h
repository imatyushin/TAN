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
        mContextCL(context)
    {
    }
#else
    CABuf(const amf::AMFContextPtr & context):
        mContextAMF(context)
    {
    }
#endif

    CABuf()
    {
    }

    virtual ~CABuf()
    {
        release();
    }


// TO DO : correct copy costructor, operator =, clone()
    CABuf(const CABuf & src)
    {
#ifndef TAN_NO_OPENCL
        mContextCL = src.mContextCL;
#else
        mContextAMF = src.mContextAMF;
#endif
    }

/*
    CABuf clone(void)
    {
        CABuf new_buf(mContextCL);
        return(new_buf);
    }
*/

    AMF_RESULT create(const T *_buf, size_t _sz, uint32_t flags)
    {
        AMF_RESULT ret = AMF_OK;
        if (_sz == 0 )
        {
            ret = AMF_FAIL;

            return ret;
        }

#ifndef TAN_NO_OPENCL
        mBufferCL = clCreateBuffer(mContextCL, flags, _sz * sizeof(T), _buf, &ret);

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
            mContextAMF->AllocBuffer(
                mMemoryType,
                _sz * sizeof(T),
                &mBufferAMF
                )
            );
#endif

        mSize = _sz;
        mBufferAllocated = true;
        mFlags = flags;
        mSystemMemory = _buf;

        return ret;
    }

    AMF_RESULT create(size_t size, uint32_t flags)
    {
        return create(nullptr, size, flags);
    }

// TO DO :: CORECT
    AMF_RESULT attach(const T *_buf, size_t _sz)
    {
        AMF_RESULT ret = AMF_OK;
        uint32_t flags = mFlags;
        bool old_sys_own = mSystemMemoryAllocated;
        T * old_ptr = mSystemMemory;

        if ( _sz > mSize ) {
            release();
            AMF_RETURN_IF_FAILED(create(_sz, flags));
        }

        if ( mSystemMemoryAllocated && mSystemMemory  && old_ptr != _buf)
        {
            delete [] mSystemMemory;
            mSystemMemory = 0;
        }

        mSystemMemory = (T*)_buf;
        mSize = _sz;
        mSystemMemoryAllocated = (old_ptr != _buf) ? false : old_sys_own;

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
            mBufferAllocated = false;
        }

        return ret;
    }
#else
    AMF_RESULT attach(
        const amf::AMFBuffer * buffer,
        size_t size
        )
    {
        AMF_RESULT ret = AMF_OK;

        if(buffer != mBufferAMF || size > mSize)
        {
            AMF_RETURN_IF_FAILED(release());

            mBufferAMF = buffer;
            mSize = size;
            mBufferAllocated = false;
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
            mMappingQueueCL = _mappingQ;

                ret = mMappedMemory = (T *)clEnqueueMapBuffer(
                    mMappingQueueCL,
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
            mMappingQueueAMF = mappingQueue;

            ret = mMappedMemory = (T *)clEnqueueMapBuffer(mMappingQueueCL,
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


            mMappingQueueCL = _mappingQ;

            ret = mMappedMemory = (T *)clEnqueueMapBuffer (mMappingQueueCL,
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
        if ( mBufferCL && mMappedMemory && mMappingQueueCL) {

            int n_wait_events = 0;
            cl_event * p_set_event = _set_event;
            cl_event * p_wait_event = _wait_event;
            if (_wait_event != NULL)
            {
                n_wait_events = 1;
            }

            ret = clEnqueueUnmapMemObject(mMappingQueueCL,
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
            mMappingQueueCL = 0;
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
    AMF_RESULT copyToDevice(cl_command_queue commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t _offset = 0)
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

        if ( !mBufferCL )
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
                _offset * sizeof(T),
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
    AMF_RESULT copyToDevice(const amf::AMFComputePtr & commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t _offset = 0)
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
            AMF_RETURN_IF_FAILED(create(mSize * sizeof(T), flags));
        }

        size_t len = (size != -1 )? size : mSize;

        const T * sys_ptr = (size != -1) ? data : mSystemMemory;
        AMF_RETURN_IF_INVALID_POINTER(
            sys_ptr,
            L"Internal error: buffer hasn't been preallocated"
            );

        AMF_RETURN_IF_FAILED(
            commandQueue->CopyBufferToHost(
                mBufferAMF,
                _offset * sizeof(T),
                len * sizeof(T),
                sys_ptr,
                true
                )
            );

        return AMF_OK;
    }
#endif

#ifndef TAN_NO_OPENCL
    AMF_RESULT copyToDeviceNonBlocking(cl_command_queue commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t _offset = 0)
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
                _offset * sizeof(T),
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
    AMF_RESULT copyToDeviceNonBlocking(const amf::AMFComputePtr & commandQueue, uint32_t flags, const T* data = NULL, size_t size = -1, size_t _offset = 0)
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
            AMF_RETURN_IF_FAILED(create(mSize * sizeof(T), flags));
        }

        size_t len = (size!=-1 )? size : mSize;
        const T * sys_ptr = (size!=-1) ? data : mSystemMemory;
        AMF_RETURN_IF_INVALID_POINTER(sys_ptr,
                            L"Internal error: buffer hasn't been preallocated");
        AMF_RETURN_IF_FAILED(commandQueue->CopyBufferFromHost(sys_ptr, len * sizeof(T), mBufferAMF, _offset * sizeof(T), CL_FALSE));

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
                CL_TRUE
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
                src.GetBuffersCL(),
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
                src.GetBuffersAMF(),
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
    AMF_RESULT CopyBufferMemory(cl_mem src, cl_command_queue commandQueue, size_t size)
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
                src,
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
    AMF_RESULT setValue2(cl_command_queue commandQueue, T _val, size_t size = 0, size_t _offset = 0)
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
                    _offset * sizeof(T),
                    len * sizeof(T),
                    0,
                    nullptr,
                    &event
                    )
                );
        }

        for (int i = _offset; mSystemMemory && i < len; i++)
        {
            mSystemMemory[i] = _val;
        }

        return err;
    }
#else
    AMF_RESULT setValue2(const amf::AMFComputePtr & commandQueue, T _val, size_t size = 0, size_t offset = 0)
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

        if ( mBufferAllocated )
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

        mBufferAllocated = false;
        mSize = 0;

        return ret;
    }

    /*
#ifndef TAN_SDK_EXPORTS
    inline void setContext(cl_context _context)
    {
        mContextCL = _context;
    }
#endif

    inline cl_context getContext(void)
    {
        return(mContextCL);
    }
    */

#ifndef TAN_NO_OPENCL
    inline const cl_mem &                   GetBuffersCL() const
    {
        return mBufferCL;
    }
#else
    inline const amf::AMFBufferPtr &        GetBuffersAMF() const
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

            mBufferAllocated = true;
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
    cl_context                  mContextCL;
    cl_command_queue            mMappingQueueCL;
#else
    amf::AMF_MEMORY_TYPE        mMemoryType = amf::AMF_MEMORY_TYPE::AMF_MEMORY_UNKNOWN;
    amf::AMFContextPtr          mContextAMF;
    amf::AMFComputePtr          mMappingQueueAMF;
    amf::AMFComputeKernelPtr    mComputeKernel;
#endif

    T *                         mSystemMemory = nullptr;
    T *                         mMappedMemory = nullptr;

#ifndef TAN_NO_OPENCL
    cl_mem                      mBufferCL = nullptr;
#else
    amf::AMFBufferPtr           mBufferAMF;
#endif

    size_t                      mSize = 0;
    uint32_t                        mFlags = 0;
    bool                        mSystemMemoryAllocated = false;
    bool                        mBufferAllocated = false;
    size_t offset_ = 0;
};

template<typename T>
class CASubBuf : public CABuf<T>
{
public:
    CASubBuf(CABuf<T> & _base) : CABuf< T >(_base), m_base(_base)
    {
        base_buf_ = _base.GetBuffersCL();
        CABuf< T >::mSystemMemory = nullptr;

        assert(base_buf_);
    }

    AMF_RESULT create(size_t _offset, size_t _sz, uint32_t flags)
    {
        AMF_RESULT ret = AMF_OK;

        if (_sz == 0 )
        {
            ret = AMF_FAIL;

           return ret;
        }

        m_offset = _offset;

        cl_buffer_region sub_buf;

        sub_buf.origin = _offset* sizeof(T);
        sub_buf.size = _sz * sizeof(T);

        CABuf< T >::mBufferCL = clCreateSubBuffer (	base_buf_,
                                    flags,
                                    CL_BUFFER_CREATE_TYPE_REGION,
                                    &sub_buf,
                                    &ret);


        if(ret != CL_SUCCESS)
        {
#ifdef _DEBUG_PRINTF
            printf("error creating buffer: %d\n", ret);
#endif
            AMF_ASSERT(false, L"error creating buffer: %d\n", ret);
            return ret;
        }

        CABuf< T >::mSize = _sz;
        CABuf< T >::mBufferAllocated = true;
        CABuf< T >::mFlags = flags;

        return ret;
    }

    int create(size_t _sz, uint32_t flags)
    {
        create(0, _sz, flags);
    }

    T * & getSysMem(void) override
    {
        if (!CABuf< T >::mSystemMemory)
        {
            CABuf< T >::mSystemMemory = m_base.getSysMem();
                                                     //todo: investigate retcode usage
            AMF_RETURN_IF_FALSE(CABuf< T >::mSystemMemory != nullptr, CABuf< T >::mSystemMemory, L"Internal error: subbuffer's parent failed to allocate system memory");
            CABuf< T >::mSystemMemory += m_offset;
        }

        return CABuf< T >::mSystemMemory;
    }
protected:
    cl_mem base_buf_;
    size_t m_offset;
    CABuf<T> &m_base;
};

}