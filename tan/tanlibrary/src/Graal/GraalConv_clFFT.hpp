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


#ifndef GRAALCONV_CLFFT_H_
#define GRAALCONV_CLFFT_H_

#include <CL/cl.h>
#include "clFFT.h"
#include "GraalConv.hpp"
#include <string>
#include <mutex>  // not available in VS2010

namespace graal
{

/*//todo: implement with std::mutex
class RecursiveBenaphore
{
private:
    long m_counter;
    unsigned long m_owner;
    unsigned long m_recursion;
    //HANDLE m_semaphore;
    std::mutex m_mutex;


public:
    RecursiveBenaphore::RecursiveBenaphore()
    {
        m_counter = 0;
        m_owner = 0;            // an invalid thread ID
        m_recursion = 0;
        m_semaphore = CreateSemaphore(NULL, 0, 1, NULL);
    }

    RecursiveBenaphore::~RecursiveBenaphore()
    {
        CloseHandle(m_semaphore);
    }

    void Lock()
    {
        DWORD tid = GetCurrentThreadId();
        if (_InterlockedIncrement(&m_counter) > 1) // x86/64 guarantees acquire semantics
        {
            if (tid != m_owner)
                WaitForSingleObject(m_semaphore, INFINITE);
        }
        //--- We are now inside the Lock ---
        m_owner = tid;
        m_recursion++;
    }

    void Unlock()
    {
        DWORD tid = GetCurrentThreadId();
        DWORD recur = --m_recursion;
        if (recur == 0)
            m_owner = 0;
        DWORD result = _InterlockedDecrement(&m_counter); // x86/64 guarantees release semantics
        if (result > 0)
        {
            if (recur == 0)
                ReleaseSemaphore(m_semaphore, 1, NULL);
        }
        //--- We are now outside the Lock ---
    }
};*/

class CGraalConv_clFFT: public CGraalConv
{
    public:
    /**
     * Constructor
     * Initialize member variables
     * @param name name of sample (string)
     */
     CGraalConv_clFFT(
#ifdef TAN_NO_OPENCL
        amf::AMFFactory * factory
#endif
        );

    /**
     * Destructor
     * @param name name of sample (string)
     */
     virtual ~CGraalConv_clFFT(void);

    /**
     * Allocate and initialize convolution class
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT initializeConv(
#ifdef TAN_SDK_EXPORTS
        const amf::TANContextPtr &pContextTAN,
        const amf::AMFComputePtr &pConvolution,
        const amf::AMFComputePtr &pUpdate,
#endif
        int n_max_channels,
        int max_conv_sz,
        int max_proc_buffer_sz,
        int n_upload_sets = 2,         // number of shadow buffers for double buffering
        int algorithm = ALG_ANY
//#ifndef TAN_SDK_EXPORTS
//        ,
//        cl_context clientContext = 0,
//        cl_device_id clientDevice = 0,
//        cl_command_queue clientQ = 0
//#endif
        ) override;

    void cleanup();
    /**
     * Returns a set of gpu_friendly system pointers - any upload set and kernel ID
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT getConvBuffers(
        int n_channels,
        int *uploadIDs,             // upload set IDs per kernel
        int *convIDs,               // kernel IDs
        float** conv_ptrs           // gpu-frendly system pointers
        ) override;

    /**
    * Returns a array of libraray-managed OCL buffers
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    AMF_RESULT getConvBuffers(
        int n_channels,				// number of channels
        int *uploadIDs,             // upload set IDs per kernel
        int *convIDs,               // kernel IDs

#ifndef TAN_NO_OPENCL
        cl_mem* clBuffers           // gpu-frendly system pointers
#else
        amf::AMFBuffer ** buffers   // gpu-frendly system pointers
#endif
        ) override;

    /*
    todo ivm: seems unused
    /**
    * Obtain library-managed OCL buffers.
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    * /
    AMF_RESULT getLibConvBuffers(
        int n_channels,
        int * uploadIDs,             // upload set IDs per kernel
        int * convIDs,               // kernel IDs
        cl_mem* ocl_mems           // gpu-frendly system pointers
        )
    {
        return getConvBuffers(n_channels, uploadIDs, convIDs, ocl_mems);
    }
    */

    AMF_RESULT uploadConvHostPtrs(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * conv_lens,
        bool synchronous   // synchronous call
        ) override;

    /**
     * Upload kernels from a previously acquired gpu-friendly system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

	AMF_RESULT uploadConvGpuPtrs(
		int n_channels,
		const int *_uploadIDs,
		const int *_convIDs,
#ifndef TAN_NO_OPENCL
        const cl_mem * conv_ptrs,  // arbitrary host ptrs
#else
        const amf::AMFBuffer ** conv_ptrs,  // arbitrary amf buffers ptrs
#endif
		const int * _conv_lens,
		bool synchronous
		) override;

    AMF_RESULT updateConv(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,
        const int * conv_lens,
        bool synchronous = false   // synchronoius call
        ) override;

    /**
     * Upload kernels from arbitrary system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT updateConvHostPtrs(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * conv_lens,
        bool synchronous = false   // synchronous call
        ) override;

    /**
    * Upload kernels from library-managed OCL buffers.
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    virtual AMF_RESULT updateConv(
        int _n_channels,
        const int *_uploadIDs,     // upload set IDs
        const int *_convIDs,       // kernel IDs
        const int * _conv_lens,
        bool synchronous = false   // synchronoius call
        ) override;

    /**
     * Upload kernels from opencl mem objects
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT updateConv(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
#ifndef TAN_NO_OPENCL
        const cl_mem *                  clBuffers,
#else
        /*const*/ amf::AMFBuffer **         amfBuffers,
#endif
        const int * conv_lens,
        bool synchronous = false   // synchronoius call
        ) override;

    /**
     * All kernels will be ready upon the return from the call
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT finishUpdate(void) override;

#ifdef TAN_SDK_EXPORTS
    /**
    * Response copying utility function.
    *
    * @return AMF_OK on success.
    */
    AMF_RESULT copyResponses(
        uint n_channels,
        const uint pFromUploadIds[],
        const uint pToUploadIds[],
        const uint pConvIds[],
        const bool synchronous = true
        ) override;
#endif

    /**
     * Upload kernels from a previously acquired gpu-friendly system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT processHostPtrs(
        int n_channels,
        int uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        float** inputs,
        float** outputs
        );

    /**
    * Upload kernels from a previously acquired gpu-friendly system pointers.
    * Pointers become invalid after the call.
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    AMF_RESULT process(
        int n_channels,
        const int *uploadID,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float*const* inputs,
        float** outputs,
        int prev_input = 0,
        int advance_time = 1,
        int skip_stage = 0,
        int crossfade_state = 0
        ) override;

    /**
     * Flushes history.
     */
    AMF_RESULT flush(amf_uint channelId, const bool synchronous = true) override;

private:
    AMF_RESULT setupCL(
        const amf::AMFComputePtr & pComputeConvolution,
        const amf::AMFComputePtr & pComputeUpdate
        );

    AMF_RESULT setupCLFFT();

    //for debugging, these print portions of the buffer to stdout
    void printBlock(CABuf<float>* buf, std::string name = "", int blockLength = 1024, int offset = 0, int count = 5);
    void printAllBlocks(CABuf<float>* buf, std::string name = "", int blockLength = 1024, int blockCount = 5, int count = 5);
    //These functions are to write the buffers out to a file that can be imported by octave (an open source matlab clone)
    void writeComplexToOctaveFile(CABuf<float>* buf, std::string filename, int blockLength);
    void writeToOctaveFile(CABuf<float>* buf, std::string filename, int blockLength);

    void incRoundCounter(int set, int ch)
    {
        roundCounter_[ch][set]++;
    }

    int getRoundCounter(int set, int ch) override
    {
        int ret = (int)round_counter_;

        if(set > -1)
        {
            ret = roundCounter_[ch][set];
        }

        return ret;
    }

    uint n_max_channels_;
    int max_conv_sz_;			//the largest impulse response size
    int max_conv_sz_freq_;//number of float values to represent the IR in freq domain
    size_t max_conv_sz_freq_aligned_;//number of float values to represent the IR in freq domain
    int block_sz_;			//the largest impulse response size
    int double_block_sz_;			//the largest impulse response size
    int freq_block_sz_;			// size in floats (complex is two floats)
    size_t max_proc_buffer_sz_;		//input block size
    int freq_block_num_elements_;  //number of complex numbers per block in frequency domain

    uint n_sets_;
    uint num_blocks_;

    //For the reverb impulse response (sometimes called kernel elsewhere)
    std::vector<CABuf<float>*> clIRInputBuf;
    std::vector<CABuf<float>*> clIRInputPaddedBuf;
    CABuf<float>* clIRBlocksBaseBuf;
    std::vector<std::vector<CASubBuf<float>*>> clIRBlocksBuf;

    //for the signal to be convoluted
    CABuf<float>* clSignalHistBaseBuf;
    //the new input signal
    CABuf<float>* clInputBaseBuf;
    //the new input signal in blocks for frequency domain
    CABuf<float>* clInputBlockBaseBuf;

    CABuf<float>* clOutputBaseBuf;
    CABuf<float>* clOutputComplexBaseBuf;

    CABuf<int>* clChannelMap;
    CABuf<int>* clSetMap;
    CABuf<int>* clCurrInputMap;
    CABuf<int>* clPrevInputMap;
    CABuf<int>* clBlockNumberMap;

    std::vector<std::vector<int>> roundCounter_;

    cl_command_queue    m_commandqueue_Update;

    cl_kernel padKernel_;
    cl_kernel interleaveKernel_;
    cl_kernel deinterleaveKernel_;
    cl_kernel madaccMultiChanKernel_;
    cl_kernel interleaveMultiChanKernel_;
    cl_kernel sigHistInsertMultiChanKernel_;

#ifdef TAN_NO_OPENCL
    amf::AMFComputeKernelPtr mPadKernel;
    amf::AMFComputeKernelPtr mInterleaveKernel;
    amf::AMFComputeKernelPtr mDeinterleaveKernel;
    amf::AMFComputeKernelPtr mMadaccMultiChanKernel;
    amf::AMFComputeKernelPtr mInterleaveMultiChanKernel;
    amf::AMFComputeKernelPtr mSigHistInsertMultiChanKernel;
#endif

#ifndef TAN_SDK_EXPORTS
    cl_context clientContext_;
    cl_command_queue clientQ_;
#endif


    clfftPlanHandle clfftPlanFwdIR;
    clfftPlanHandle clfftPlanFwdAllChannels;
    clfftPlanHandle clfftPlanBackAllChannels;

    std::recursive_mutex processLock;
};

};

#endif
