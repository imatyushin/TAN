#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <thread>

//#define SILENT

#ifndef CLQUEUE_REFCOUNT
#define CLQUEUE_REFCOUNT( clqueue ) \
{ \
		cl_uint refcount = 0; \
		clGetCommandQueueInfo(clqueue, CL_QUEUE_REFERENCE_COUNT, sizeof(refcount), &refcount, NULL); \
		printf("\nFILE:%s line:%d Queue %llX ref count: %d\r\n", __FILE__ , __LINE__, clqueue, refcount); \
}
#endif

#ifndef DBG_CLRELEASE_QUEUE
#define DBG_CLRELEASE_QUEUE( clqueue, qname ) \
{ \
		cl_uint refcount = 0; \
		clReleaseCommandQueue(clqueue); \
		clGetCommandQueueInfo(clqueue, CL_QUEUE_REFERENCE_COUNT, sizeof(refcount), &refcount, NULL); \
		printf("\nFILE:%s line:%d %s %llX ref count: %d\r\n", __FILE__ , __LINE__,qname, clqueue, refcount); \
        clqueue = nullptr; \
}
#endif

#ifndef DBG_CLRELEASE_MEMORYOBJECT
#define DBG_CLRELEASE_MEMORYOBJECT( clobject) \
{ \
	clReleaseMemObject(clobject); \
	printf("\nFILE:%s line:%d release memory object %llX\r\n", __FILE__ , __LINE__, clobject); \
    clobject = nullptr; \
}
#endif

static size_t GetMessageNumber()
{
    static std::atomic<size_t> messageNumber;

    return messageNumber.fetch_add(1);
}

static std::ostream & PrintThreadInfo()
{
    return std::cout
        << std::endl
        << "[" << GetMessageNumber() << "] "
        << "thread " << std::this_thread::get_id() << " ";
}

static void PrintDebug(const std::string & hint)
{
#ifdef SILENT
    return;
#endif

    PrintThreadInfo() << hint << std::endl;
}

static void PrintFloatArray(const std::string & hint, const float * array, size_t count, size_t max = 64)
{
#ifdef SILENT
    return;
#endif
    
    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!array)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    const uint8_t *data(reinterpret_cast<const uint8_t *>(array));

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(data[i]) << " ";
    }

    std::cout << std::endl;
}

static void PrintFloatArray(const char * hint, const float * array, size_t count, size_t max = 64)
{
    PrintFloatArray(std::string(hint), array, count, max);
}

static void PrintShortArray(const std::string & hint, const int16_t * array, size_t count, size_t max = 64)
{
#ifdef SILENT
    return;
#endif

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!array)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    const uint8_t *data(reinterpret_cast<const uint8_t *>(array));

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(data[i]) << " ";
    }

    std::cout << std::endl;
}

static void PrintShortArray(const char * hint, const int16_t * array, size_t count, size_t max = 64)
{
    PrintShortArray(std::string(hint), array, count, max);
}

#ifndef TAN_NO_OPENCL
static void PrintCLArray(const std::string & hint, cl_mem array, cl_command_queue queue, size_t count, size_t max = 64)
{
#ifdef SILENT
    return;
#endif

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!array)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    clFlush(queue);
    clFinish(queue);

    std::vector<cl_uchar> out(count);

    auto error = clEnqueueReadBuffer(
        queue,
        array,
        CL_TRUE,
        0,
        count,
        out.data(),
        NULL,
        NULL,
        NULL
        );
    if(CL_SUCCESS != error)
    {
        std::cout << "CL ERROR" << std::endl;
    }
    
    clFlush(queue);
    clFinish(queue);

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << unsigned(cl_uchar(out[i])) << " ";
    }

    std::cout << std::endl;
    
    clFlush(queue);
    clFinish(queue);
}

static void PrintCLArray(const char * hint, cl_mem array, cl_command_queue queue, size_t count, size_t max = 64)
{
    PrintCLArray(std::string(hint), array, queue, count, max);
}

static static void PrintCLArrayWithOffset(const char * hint, cl_mem array, cl_command_queue queue, size_t count, size_t offset = 0, size_t max = 64)
{
#ifdef SILENT
    return;
#endif

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!array)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    std::vector<uint8_t> out(count);

    auto error = clEnqueueReadBuffer(
        queue,
        array,
        CL_TRUE,
        offset,
        count,
        out.data(),
        NULL,
        NULL,
        NULL
        );
    if(CL_SUCCESS != error)
    {
        std::cout << "CL ERROR" << std::endl;
    }

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(out[i]) << " ";
    }

    std::cout << std::endl;
}
#endif

static void PrintAMFArray(const std::string & hint, amf::AMFBuffer * buffer, amf::AMFCompute * compute, size_t count, size_t max = 64)
{
#ifdef SILENT
    return;
#endif

    //compute->FlushQueue();

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!buffer)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    std::vector<uint8_t> out(count);

    auto result = compute->CopyBufferToHost(
        buffer,
        0,
        count,
        out.data(),
        true
        );

    if(result != AMF_OK)
    {
        std::cout << "ERROR" << std::endl;

        return;
    }

    //compute->FlushQueue();

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(out[i]) << " ";
    }

    std::cout << std::endl;
}

static void PrintAMFArray(const char * hint, amf::AMFBuffer * buffer, amf::AMFCompute * compute, size_t count, size_t max = 64)
{
    PrintAMFArray(std::string(hint), buffer, compute, count, max);
}

static void PrintAMFArrayWithOffset(const char * hint, amf::AMFBuffer * buffer, amf::AMFCompute * compute, size_t count, size_t offset = 0, size_t max = 64)
{
#ifdef SILENT
    return;
#endif

    //compute->FlushQueue();

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!buffer)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    std::vector<uint8_t> out(count);

    auto result = compute->CopyBufferToHost(
        buffer,
        offset,
        count,
        out.data(),
        true
        );

    if(result != AMF_OK)
    {
        std::cout << "ERROR" << std::endl;

        return;
    }

    //compute->FlushQueue();

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(out[i]) << " ";
    }

    std::cout << std::endl;
}

static std::string Empty()
{
    return "";
}

#define ES std::string()
