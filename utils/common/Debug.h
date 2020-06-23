#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <thread>

#ifndef CLQUEUE_REFCOUNT
#define CLQUEUE_REFCOUNT( clqueue ) { \
		cl_uint refcount = 0; \
		clGetCommandQueueInfo(clqueue, CL_QUEUE_REFERENCE_COUNT, sizeof(refcount), &refcount, NULL); \
		printf("\nFILE:%s line:%d Queue %llX ref count: %d\r\n", __FILE__ , __LINE__, clqueue, refcount); \
}
#endif

#ifndef DBG_CLRELEASE
#define DBG_CLRELEASE( clqueue, qname ) { \
		cl_uint refcount = 0; \
		clReleaseCommandQueue(clqueue); \
		clGetCommandQueueInfo(clqueue, CL_QUEUE_REFERENCE_COUNT, sizeof(refcount), &refcount, NULL); \
		printf("\nFILE:%s line:%d %s %llX ref count: %d\r\n", __FILE__ , __LINE__,qname, clqueue, refcount); \
}
#endif

static std::ostream & PrintThreadInfo()
{
    return std::cout << std::endl << "thread " << std::this_thread::get_id() << " ";
}

static void PrintDebug(const std::string & hint)
{
    //return;

    PrintThreadInfo() << hint << std::endl;
}

static void PrintFloatArray(const char * hint, float * array, size_t count, size_t max = 64)
{
    //return;

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!array)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    uint8_t *data(reinterpret_cast<uint8_t *>(array));

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(data[i]) << " ";
    }

    std::cout << std::endl;
}

static void PrintShortArray(const char * hint, int16_t * array, size_t count, size_t max = 64)
{
    //return;

    PrintThreadInfo() << hint << ": " << count << std::endl;

    if(!array)
    {
        std::cout << "NULL" << std::endl;

        return;
    }

    uint8_t *data(reinterpret_cast<uint8_t *>(array));

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(data[i]) << " ";
    }

    std::cout << std::endl;
}

#ifndef TAN_NO_OPENCL
static void PrintCLArray(const char * hint, cl_mem array, cl_command_queue queue, size_t count, size_t max = 64)
{
    //return;

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

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(out[i]) << " ";
    }

    std::cout << std::endl;
}

static void PrintCLArrayWithOffset(const char * hint, cl_mem array, cl_command_queue queue, size_t count, size_t offset = 0, size_t max = 64)
{
    //return;

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

static void PrintAMFArray(const char * hint, amf::AMFBuffer * buffer, amf::AMFCompute * compute, size_t count, size_t max = 64)
{
    //return;

    compute->FlushQueue();

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

    compute->FlushQueue();

    for(size_t i(0); i < (count < max ? count : max); ++i)
    {
        std::cout << int(out[i]) << " ";
    }

    std::cout << std::endl;
}

static void PrintAMFArrayWithOffset(const char * hint, amf::AMFBuffer * buffer, amf::AMFCompute * compute, size_t count, size_t offset = 0, size_t max = 64)
{
    //return;

    compute->FlushQueue();

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

    compute->FlushQueue();

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
