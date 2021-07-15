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
#ifndef GRAALCONV_H_
#define GRAALCONV_H_

#include "TrueAudioNext.h"

#ifndef TAN_NO_OPENCL
  #include <CL/cl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include <map>

#ifdef WIN32

#include <io.h>
#include <windows.h>
#include <BaseTsd.h>

typedef unsigned int uint;

static double mach_absolute_time()
{
    double ret = 0;
    int64_t frec;
    int64_t clocks;
    QueryPerformanceFrequency((LARGE_INTEGER *)&frec);
    QueryPerformanceCounter((LARGE_INTEGER *)&clocks);
    ret = (double)clocks * 1000. / (double)frec;
    return ret;
}

#else

#if defined(__APPLE__) || defined(__MACOSX)
#include <mach/mach_time.h>
#endif

#include<strings.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/time.h>
#include <sys/resource.h>

#endif

#include "public/common/TraceAdapter.h"
#define AMF_FACILITY L"GraalConv"

#include "CABuffer.h"
#include "GraalSampleBuffer.h"

#define __FLOAT__ float
typedef unsigned int uint;

static double subtractTimes(double endTime, double startTime)
{
    double difference = endTime - startTime;
    static double conversion = 0.0;

    if (conversion == 0.0)
    {
#if __APPLE__
        mach_timebase_info_data_t info = {0};
        kern_return_t err = mach_timebase_info(&info);

        //Convert the timebase into seconds
        if (err == 0)
            conversion = 1e-9 * (double)info.numer / (double)info.denom;
#endif
#ifdef _WIN32
        conversion = 1.;
#endif
    }

    return conversion * (double)difference;
}


/**
 */
namespace graal
{
// implemented pipelines
    enum GRAAL_ALG {
// selected by Graal
        ALG_ANY,
// uniform "classical" pipeline:
// direct transform ->MAD->inverse transform
        ALG_UNIFORMED,
// uniform, spltted into 2 stages: head and tail
// 1st stage done in a curret round and returns the filtered block
// direct transform-> MAD (with the 0th conv block) + tail sum->inverse transform
// 2nd stage is done in background, bulids the "tail" sum.
// MAD (with all conv blocks except 0th)
        ALG_UNI_HEAD_TAIL,
		ALG_USE_PROCESS_FINALIZE = 0x8000 // use ProcessFinalize() optimization for HEAD_TAIL mode
	};

class CGraalConv
{
public:
    /**
     * Constructor
     * Initialize member variables
     */
    CGraalConv(
#ifdef TAN_NO_OPENCL
        amf::AMFFactory * factory
#endif
        );

    /**
     * Destructor
     * @param name name of sample (string)
     */
     virtual ~CGraalConv(void);

    /**
     * Allocate and initialize convolution class
     *
     * @param n_max_channels		max number of channels tp be processed
     * @param max_conv_sz			max number of samples of IR
     * @param max_proc_buffer_sz	max size of the input buffers to be proccessed
     * @param n_upload_sets			max number of IR versions per 1 channnel (for double buffeering)
     * @param algorithm				algorthm to be used (see enum above)
     * @param clientContext			user defined context (if this parameter is not 0, next 2 should be non-0 also)
     * @param clientDevice			user defined device
     * @param clientQ				user defined queue
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    virtual AMF_RESULT initializeConv(
#ifdef TAN_SDK_EXPORTS
        const amf::TANContextPtr & pContextTAN,
        const amf::AMFComputePtr & pConvolution,
        const amf::AMFComputePtr & pUpdate,
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
        );

    /**
     * Returns a set of gpu_friendly system pointers.
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param conv_ptrs					gpu-frendly system pointers
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    virtual AMF_RESULT getConvBuffers(
        int n_channels,				// number of channels
        int *uploadIDs,             // upload set IDs per kernel
        int *convIDs,               // kernel IDs
        float** conv_ptrs           // gpu-frendly system pointers
        );

    /**
     * Returns a array of library-managed OCL buffers
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param ocl_bufffers				Graal library-managed ocl-buffers
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    virtual AMF_RESULT getConvBuffers(
        int n_channels,				// number of channels
        int *uploadIDs,             // upload set IDs per kernel
        int *convIDs,               // kernel IDs

#ifndef TAN_NO_OPENCL
        cl_mem* clBuffers           // gpu-frendly system pointers
#else
        amf::AMFBuffer ** buffers   // gpu-frendly system pointers
#endif
        );


    /**
     * Upload kernels from a previously acquired gpu-friendly system pointers.
     * Pointers become invalid after the call.
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param conv_ptrs					gpu-frendly system pointers
     * @param conv_lens					length of the IR per channel
     * @param synchronous				if 0, the call is asynchronous and returns immideatly,
     *									if 1, it's an asynchronous call and it returns only after the IR is ready for use.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    virtual AMF_RESULT updateConv(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,
        const int * conv_lens,
        bool synchronous = false   // synchronoius call
        );

    /**
     * Upload kernels from arbitrary system pointers.
     * It's the slowest upload.
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param conv_ptrs					arbitrary system pointers
     * @param conv_lens					length of the IR per channel
     * @param synchronous				if 0, the call is asynchronous and returns immideatly,
     *									if 1, it's an asynchronous call and it returns only after the IR is ready for use.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    virtual AMF_RESULT updateConvHostPtrs(
        int n_channels,
        const int *uploadIDs,
        const int *convIDs,
        const float** conv_ptrs,
        const int * conv_lens,
        bool synchronous = false
        );


    /**
     * Upload kernels from client-managed OCL buffers.
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param ocl_buffers				client managed OCL buffers
     * @param conv_lens					length of the IR per channel
     * @param synchronous				if 0, the call is asynchronous and returns immideatly,
     *									if 1, it's an asynchronous call and it returns only after the IR is ready for use.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

    virtual AMF_RESULT updateConv(
        int                             channelsCount,
        const int *                     uploadIDs,
        const int *                     convIDs,
#ifndef TAN_NO_OPENCL
        const cl_mem *                  clBuffers,
#else
        /*const*/ amf::AMFBuffer **         amfBuffers,
#endif
        const int *                     conv_lens,
        bool                            synchronous = false
        );


    /**
     * Upload kernels from library-managed OCL buffers.
     * this is the fastest upload.
     * ocl buffers has to be obtain with the getConvBuffers interface.
     *
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param conv_lens					length of the IR per channel
     * @param synchronous				if 0, the call is asynchronous and returns immideatly,
     *									if 1, it's an asynchronous call and it returns only after the IR is ready for use.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

    virtual AMF_RESULT updateConv(
        int _n_channels,
        const int *_uploadIDs,
        const int *_convIDs,
        const int * _conv_lens,
        bool synchronous = false
        );

    /**
     * All previously uploaded IRs will be ready for use upon the return from the call
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */
     virtual AMF_RESULT finishUpdate(void);

#ifdef TAN_SDK_EXPORTS
     /**
      * Blocks until all the currently submitted processing tasks are done.
      *
      * @return AMF_OK on success.
      */
    virtual AMF_RESULT finishProcess(void)
    {
#ifndef TAN_NO_OPENCL
        cl_int status = CL_SUCCESS;
        status = clFlush(m_pContextTAN->GetOpenCLConvQueue());
        cl_command_queue uploadQ = m_pContextTAN->GetOpenCLGeneralQueue();
        AMF_RETURN_IF_CL_FAILED(status, L"failed: finishProcess clFlush" );
#else
        AMF_RETURN_IF_FAILED(m_pContextTAN->GetAMFConvQueue()->FlushQueue());
#endif

#ifndef TAN_NO_OPENCL
        status = clFinish(m_pContextTAN->GetOpenCLConvQueue());
        AMF_RETURN_IF_CL_FAILED(status, L"failed: finishProcess clFinish" );
#else
        AMF_RETURN_IF_FAILED(m_pContextTAN->GetAMFConvQueue()->FinishQueue());
#endif

        return AMF_OK;
    }

    /**
     * Response copying utility function.
     *
     * @return AMF_OK on success.
     */
    virtual AMF_RESULT copyResponses(
        uint n_channels,
        const uint pFromUploadIds[],
        const uint pToUploadIds[],
        const uint pConvIds[],
        const bool synchronous = true
        );
#endif

// more efficient dma copy
#define COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK 1

#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
    virtual bool checkForContiguousBuffers(
        int count,
        const float** conv_ptrs,
        const int * _conv_lens
        );
#endif

    /**
     * Process audio blocks from arbitrary system pointers.
     *
     * @param n_channels				number of channels in the request
     * @param *uploadIDs				version id
     * @param *convIDs					channel id
     * @param inputs					input pointers (size of buffers must be eaqual to max_proc_buffer_sz)
     * @param outputs					output pointer (size of buffers must be eaqual to max_proc_buffer_sz)
     * @param prev_input				1 - reuse previous input, 0 - process curent input (used in "mix")
     * @param advance_time				1 - advance internal round counter, 0 - do not advance (used in "mix")
     * @param skip_stage				1 - skip the 1st stager, 2 - skipo 2nd stage, 0 - full algorithm (used in "mix")
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

    virtual AMF_RESULT process
    (
        int             n_channels,
        const int *     uploadID,     // upload set IDs
        const int *     convIDs,       // kernel IDs
        const float* const*   inputs,
        float**         outputs,
        int             prev_input = 0,
        int             advance_time = 1,
        int             skip_stage = 0,
        int             _crossfade_state = 0
    );

    /**
    *
    * @param n_channels				number of channels in the request
    * @param *uploadIDs				version id
    * @param *convIDs				channel id
    * @param inputs					input pointers (size of buffers must be eaqual to max_proc_buffer_sz)
    * @param outputBuf				pointers to the client-managed OCL buffers.
    * @param prev_input				1 - reuse previous input, 0 - process curent input (used in "mix")
    * @param advance_time			1 - advance internal round counter, 0 - do not advance (used in "mix")
    * @param skip_stage				1 - skip the 1st stager, 2 - skipo 2nd stage, 0 - full algorithm (used in "mix")
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    virtual AMF_RESULT process(
        int n_channels,
        const int *uploadID,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float*const* inputs,
#ifndef TAN_NO_OPENCL
		cl_mem *            output,
#else
        amf::AMFBuffer **   output,
#endif
        int prev_input = 0,
        int advance_time = 1,
        int skip_stage = 0,
        int _crossfade_state = 0
        );

	virtual AMF_RESULT processFinalize();

     /**
      * Flushes history.
      */
     virtual AMF_RESULT flush(amf_uint channelId, const bool synchronous = true);


    /*************************************************************************************************
    * emulation/verification helper interfaces
    *************************************************************************************************/
    virtual int getRoundCounter(int _uploadId = -1, int _chnl_id = -1);

    virtual AMF_RESULT uploadConvHostPtrs(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * conv_lens,
        bool synchronous = false   // synchronous call
        );

    virtual AMF_RESULT uploadConvGpuPtrs(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
#ifndef TAN_NO_OPENCL
        const cl_mem * conv_ptrs,  // arbitrary host ptrs
#else
        const amf::AMFBuffer ** conv_ptrs,  // arbitrary amf buffers ptrs
#endif
        const int * conv_lens,
        bool synchronous = false   // synchronous call
        );

    inline int getInputBlockSz(void)
    {
        return(aligned_proc_bufffer_sz_);
    }


    /*************************************************************************************************
    * not implemented
    *************************************************************************************************/

    /**
    * Acquires gpu-friendly system pointers.
    * Pointers become invalid after the call.
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    AMF_RESULT getDevInputPtrs(
        int n_channels,				// # of channels
        int uploadID,				// upload set ID
        const int *convIDs,       // kernel IDs
        float** inputs			// array of gpu-frendly system pointers
        );

    /**
    * Process audio blocks from gpu_friendly pointers.
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    AMF_RESULT processDevPtrs(
        int n_channels,
        int uploadID,     // upload set ID
        const int *convIDs,       // kernel IDs
        float** inputs,
        float** outputs
        );


protected:
#ifdef TAN_NO_OPENCL
    amf::AMFFactory             *mFactory = nullptr;
#endif

#ifdef TAN_SDK_EXPORTS
    amf::TANContextPtr          m_pContextTAN;

    virtual AMF_RESULT zeroMemory(CABuf<float> *pBuf, amf_uint offset, amf_uint amount);
#endif

    void incRoundCounter(int _uploadId = -1, int _chnl_id = -1);

    /**
     * OpenCL related initialisations.
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT Setup(const amf::AMFComputePtr & pComputeConvolution, const amf::AMFComputePtr & pComputeUpdate );

    /**
     * Cleanup
     * @return AMF_OK on success and AMF_FAIL on failure
     */
    AMF_RESULT cleanup();

    /*
        kernel source files, kernel names, build options
    */
    void selectOptions(std::string & kernelFileName, std::string & kernelOptions);
    AMF_RESULT selectUploadOptions      (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions);
    AMF_RESULT selectUpload2Options     (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions);
    AMF_RESULT selectDirectFHTOptions   (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions);
    AMF_RESULT selectFHT_CMADOptions    (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::vector<std::string> & kernelName, std::string & kernelOptions);
    AMF_RESULT selectInverseFHTOptions  (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions);
    AMF_RESULT selectResetOptions       (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions);
    AMF_RESULT selectConvHead1Options   (std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions);

    /*
     advance round counterd
    */
    AMF_RESULT resetConvState(
                size_t n_channels,
                const int *uploadIDs,
                const int *convIDs,
                int time_step
            );
    /*
        upload control maps
    */
    AMF_RESULT uploadConvControlMaps(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const int * conv_lens
        );


    /**
        upload in 1 shot, Graal managed OCL buffers
    */
    AMF_RESULT updateConvOCL(
        CABuf<float> *      stg_buf,
        CASubBuf<float> *   transf_buf,
        int                 conv_len,
#ifndef TAN_NO_OPENCL
        cl_command_queue    uploadQ,
#else
        amf::AMFCompute *   uploadQ,
#endif
        int                 uploadID,
        int                 convID
        );

    /*
        upload in a loop
    */
    AMF_RESULT updateConvIntnl(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const int *conv_lens,
        bool _synchronous   // synchronoius call
        );

    /*
        uitility, upload time domain data
    */
    AMF_RESULT uploadConvHostPtrIntnl(
        int _n_channels,
        const int *_uploadIDs,     // upload set IDs
        const int *_convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * _conv_lens
        );

    /**
    * internal processing routine
    *
    * @return AMF_OK on success and AMF_FAIL on failure
    */
    AMF_RESULT processIntrnl(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float * const * inputs,
        GraalSampleBuffer& output,
        int prev_input = 0,
        int advance_time = 1,
        int skip_stage = 0,
        int _crossfade_state = 0
        );

    /*
        head-tail, head stage
    */
    AMF_RESULT processHead1(
        int _n_channels,
        const int *_uploadIDs,     // upload set IDs
        const int *_convIDs,       // kernel IDs
        int prev_input = 0,
        int advance_time = 1,
        bool _use_xf_buff = false
        );

	struct ProcessParams {
		int prev_input;
		int advance_time;
		int skip_stage;
		int n_channels;
		ProcessParams() {
			n_channels = 0;
		}
		void set(
			int _prev_input,
			int _advance_time,
			int _skip_stage,
			int _n_channels
		) {
			prev_input = _prev_input;
			advance_time = _advance_time;
			skip_stage = _skip_stage;
			n_channels = _n_channels;
		}
	} m_processParams, m_processParams_xf;

    /*
        classic
    */
    /*
        push input into the pipeline
    */
    AMF_RESULT processPush(
        int n_channels,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        int prev_input
        );

    /*
        MAD. classic and the second stage of the head
        in head-tail case, the round counter is shifted 1 step since it's alread advance in the head stage
        also IR shist set to 1, since the 2nd stage convolve with all blocks except 0th
    */
    AMF_RESULT processAccum(int n_channels,
        int IR_bin_shift = 0,
        int _STR_bin_shift = 0,
        bool _use_xf_buff = false
#ifndef TAN_SDK_EXPORTS
        ,
        cl_command_queue graalQ = NULL
#endif
        );

    AMF_RESULT processPull(
        int _n_channels,
        const int *_uploadIDs,     // upload set IDs
        const int *_convIDs,       // kernel IDs
        int advance_time);

    AMF_RESULT syncUpload( void );

    template<typename T>
    void initBuffer(
        CABuf<T> * buf,
#ifndef TAN_NO_OPENCL
        cl_command_queue queue
#else
        amf::AMFCompute * queue
#endif
        )
    {
        buf->setValue2(queue, 0);
    }

    //   alg config
    int n_sets_ = 0;             // number of kerenl sets to implement double-buffering
    int n_components_ = 1;      // real = 1, complex = 2
    int n_input_blocks_ = 2;  // # of block to keep in staging buffer
    int conv_mem_alloc_flags_ = 0; // not use
    int n_upload_qs_ = 1;   // n IR upload queues 1
    int n_input_qs_ = 0;    // n process queues 2
    int n_accum_blocks_ = 8; // n blocks accumulated at one CMAD invokation
    int n_stages_ = 0;    // classic  = 1, head tail 2
	int m_useProcessFinalize = 0;

// internal state
    int algorithm_ = ALG_UNI_HEAD_TAIL;
    int mMaxChannels = 0;
    int mMaxConvolutionSize = 0;
    int conv_log2_ = 0;
    int aligned_conv_sz_ = 0;  // size of convolution buffer with 0 padding if neded
    int max_proc_buffer_sz_ = 0;
    int processing_log2_ = 1;
    int aligned_proc_bufffer_sz_ = 0; // size of aligned input block
    int align_padding_sz_ = 0; // size of the padding or dif(aligned_proc_bufffer_sz_, max_proc_buffer_sz_)
    int n_aligned_proc_blocks_ = 0; // number of aligned blocks in aligned kernel buffer
    int mAlignedProcessingSize = 0;  // size of freq domain block
    int accum_stride_ = 0;  // stride of accum buffer per channel

    // single Graal OCL Q ???
#ifndef TAN_SDK_EXPORTS
    bool own_queue_ = false;
    //cl_command_queue graalQ_;
    //cl_command_queue graalTailQ_;
    // end of pipeline event
    cl_event eop_event_;
    // end of head event
    cl_event eoh_event_;
#endif

    // FHT
    std::unique_ptr<CABuf<float> >          mSinCos;  // precomputeted sincos table
    std::unique_ptr<CABuf<short> >          mBitReverse;  // reverse bit table

    std::vector<std::vector< std::shared_ptr< CASubBuf<float> > > >
                                            mKernelStaging;
    std::vector<std::vector< std::shared_ptr< CASubBuf<float> > > >
                                            mKernelTransformed; // per channel
    std::vector<std::shared_ptr<CASubBuf<float> > >
                                            mKernelTransformedStore; // per set

#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
    std::vector<std::shared_ptr< CASubBuf<float> > >
                                            mKernelInputStore; // per set
#endif

    // kernel channel map
    std::unique_ptr<CABuf<int> >            mKernelChannelsMap;
    // kernel set map
    std::unique_ptr<CABuf<int> >            mKernelSetsMap;
    // kernel length map
    std::unique_ptr<CABuf<int> >            mKernelLensMap;

    // kernel input
    std::unique_ptr<CABuf<float> >          mKernelInputUnion;  // base union store
    // kernel storage
    std::unique_ptr<CABuf<float> >          mKernelTrasformedUnion;  // base union store

    std::vector<std::shared_ptr<CABuf<float> > >
                                            mHostInputStaging;

    // round counter per channel and set
    std::unique_ptr<CABuf<uint>>            mRoundCounters;
    // channel map
    std::unique_ptr<CABuf<int>>             mChannelsMap;

    // set map
    std::unique_ptr<CABuf<int>>             mSetsMap;
    std::unique_ptr<CABuf<int>>             mSetsMapXf;

    // input data
    CABuf<float>                            mProcessInputStaging;

    // input transformed history
    std::unique_ptr<CABuf<float>>           mHistoryTransformed;
    // output data
    std::unique_ptr<CABuf<float>>           mProcess2OutputStaging;

    //cmad accumulator
    std::unique_ptr<CABuf<float> >          mCmadAccum;
    std::unique_ptr<CABuf<float> >          mCmadAccumXF; //used to store data needed for crossfade
    std::unique_ptr<CABuf<int> >            mCopyResponseInOffset;
    std::unique_ptr<CABuf<int> >            mCopyResponseOutOffset;

#ifndef TAN_NO_OPENCL
    //upload in a single run
    cl_kernel                               mUploadKernel               = nullptr;
    //upload per stream
    cl_kernel                               mUploadKernel2              = nullptr;

    cl_kernel                               mResetKernel                = nullptr;
    cl_kernel                               mCopyWithPaddingKernel      = nullptr;
#else
    amf::AMFComputeKernelPtr                mUploadKernel;
    amf::AMFComputeKernelPtr                mUploadKernel2;
    amf::AMFComputeKernelPtr                mResetKernel;
    amf::AMFComputeKernelPtr                mCopyWithPaddingKernel;
#endif

    int64_t round_counter_ = 0;

    std::vector<int>                        mHostCopyRespInOffset;
    std::vector<int>                        mHostCopyRespOutOffset;

#ifndef TAN_NO_OPENCL
    cl_kernel                               mDirectTransformKernel      = nullptr;
    cl_kernel                               mInverseTransformKernel     = nullptr;
    std::vector<cl_kernel>                  mCMADKernels;
    cl_kernel                               mConvHead1                  = nullptr;
#else
    amf::AMFComputeKernelPtr                mDirectTransformKernel;
    amf::AMFComputeKernelPtr                mInverseTransformKernel;
    std::vector<amf::AMFComputeKernelPtr>
                                            mCMADKernels;
    amf::AMFComputeKernelPtr                mConvHead1;
#endif

#ifndef TAN_NO_OPENCL
    cl_event                                m_headTailKernelEvent       = nullptr;
    cl_event                                m_pullKernelEvent           = nullptr;
#else
    //todo:
    //events seems not needed in Graal1
#endif

    void *                                  mFHTTransformCPUFunction    = nullptr;
    float                                   m_dataBuff[32] = {0};

    //verification/log
    int                                     mVerify = 0;
};

};

#endif
