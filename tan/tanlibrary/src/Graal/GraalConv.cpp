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
#include "GraalConv.hpp"
#include "amdFHT.h"
//#include "GraalCLUtil/GraalCLUtil.hpp"

//#include <CL/cl.h>

#ifdef ENABLE_METAL
  #include "MetalKernels/MetalKernel_GraalFHT.h"
#else
  #include "OclKernels/CLKernel_GraalFHT.h"
#endif

#if !defined(__APPLE__) && !defined(__MACOSX)
  #include <malloc.h>
#endif

#include "public/common/Thread.h"
#include "public/common/AMFFactoryHelper.h"           //AMF
#include "OCLHelper.h"
#include "Debug.h"

#include <algorithm>

#ifndef TAN_NO_OPENCL
  #include <CL/cl_ext.h>
#endif

#if defined DEFINE_AMD_OPENCL_EXTENSION
    #ifndef CL_MEM_USE_PERSISTENT_MEM_AMD
        #define CL_MEM_USE_PERSISTENT_MEM_AMD       (1 << 6)
    #endif
#endif

//to allow std::min usage
#ifdef min
#undef min
#endif

namespace graal
{

#if _MSC_VER <= 1800
    static double log2(double n)
    {
        // log(n)/log(2) is log2.
        return log(n) / log(2.);
    }
#endif

CGraalConv:: CGraalConv(
#ifdef TAN_NO_OPENCL
    amf::AMFFactory * factory
#endif
)
{
#ifdef TAN_NO_OPENCL
    mFactory = factory;
#endif
}

CGraalConv:: ~CGraalConv()
{
    cleanup();
}

AMF_RESULT CGraalConv::initializeConv(
#ifdef TAN_SDK_EXPORTS
    const amf::TANContextPtr &pContextTAN,
    const amf::AMFComputePtr &pConvolution,
    const amf::AMFComputePtr &pUpdate,
#endif
    int _n_max_channels,
    int _max_conv_sz,
    int _max_proc_buffer_sz,
    int _n_sets,
    int _algorithm
//#ifndef TAN_SDK_EXPORTS
//    ,
//    cl_context _clientContext,
//    cl_device_id _clientDevice,
//    cl_command_queue _clientQ
//#endif
)
{
    m_pContextTAN = pContextTAN;

	m_useProcessFinalize = (_algorithm & ALG_USE_PROCESS_FINALIZE) != 0;
	algorithm_ = _algorithm = GRAAL_ALG(_algorithm & ~ALG_USE_PROCESS_FINALIZE);
	algorithm_ = (_algorithm == ALG_ANY) ? ALG_UNI_HEAD_TAIL : _algorithm; // ALG_UNIFORMED;
    mMaxChannels = _n_max_channels;
    mMaxConvolutionSize = ((_max_conv_sz + 1023) / 1024 ) * 1024;
    max_proc_buffer_sz_ = _max_proc_buffer_sz;
    n_sets_ = _n_sets;
    processing_log2_ = static_cast<int>(ceil(log2((double)max_proc_buffer_sz_)));

    aligned_proc_bufffer_sz_ = (1 << processing_log2_);
    align_padding_sz_ = aligned_proc_bufffer_sz_ - max_proc_buffer_sz_;
    mAlignedProcessingSize = aligned_proc_bufffer_sz_  * 2;

    n_aligned_proc_blocks_ = (mMaxConvolutionSize + aligned_proc_bufffer_sz_ - 1) / aligned_proc_bufffer_sz_;
    aligned_conv_sz_ = (n_aligned_proc_blocks_ * mAlignedProcessingSize * n_components_);

    conv_log2_ = static_cast<int>(ceil(log2((double)aligned_conv_sz_)));

    AMF_RETURN_IF_FAILED(
        Setup(
            pConvolution,
            pUpdate

#ifndef TAN_SDK_EXPORTS
            _clientContext,
            _clientDevice,
            _clientQ
#endif
            )
        );

    return AMF_OK;
}

AMF_RESULT CGraalConv::updateConv(
    int channelsCount,
    const int *uploadIDs,
    const int *convIDs,
    const float** conv_ptrs,
    const int * conv_lens,
    bool synchronous)
{
    AMF_RESULT ret = AMF_OK;

    //cl_command_queue uploadQ = this->m_pContextTAN->GetOpenCLGeneralQueue();

    for( int j = 0; j < channelsCount; j++ )
    {
        // move data into staging
        int uploadId = uploadIDs[j];
        int convId = convIDs[j];

        assert(false);
        //ret = mKernelStaging[uploadId][convId]->unmap(); --!
    }

    ret = uploadConvControlMaps(channelsCount,
                                uploadIDs,     // upload set IDs
                                convIDs,       // kernel IDs
                                conv_lens);


    ret = updateConvIntnl(channelsCount,
                        uploadIDs,     // upload set IDs
                        convIDs,       // kernel IDs
                        conv_lens,
                        synchronous   // synchronoius call
                        );

    return ret;

}

    /**
     * Upload kernels from arbitrary system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

AMF_RESULT CGraalConv::updateConvHostPtrs(
        int channelsCount,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * conv_lens,
        bool synchronous   // synchronous call
        )
{
    AMF_RESULT ret = AMF_OK;

    ret = uploadConvControlMaps(channelsCount,
                                uploadIDs,     // upload set IDs
                                convIDs,       // kernel IDs
                                conv_lens);
    if (ret != AMF_OK)
    {
        return ret;
    }

    ret = uploadConvHostPtrIntnl(
                            channelsCount,
                            uploadIDs,     // upload set IDs
                            convIDs,       // kernel IDs
                            conv_ptrs,  // arbitrary host ptrs
                            conv_lens
                            );
    if (ret != AMF_OK)
    {
        return ret;
    }

    ret = updateConvIntnl(channelsCount,
                        uploadIDs,     // upload set IDs
                        convIDs,       // kernel IDs
                        conv_lens,
                        synchronous   // synchronoius call
                        );


    return ret;
}

    /**
     * Upload kernels from a previously acquired gpu-friendly system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

AMF_RESULT CGraalConv::updateConv(
    int                     channelsCount,
    const int *             uploadIDs,     // upload set IDs
    const int *             convIDs,       // kernel IDs
#ifndef TAN_NO_OPENCL
    const cl_mem *          clBuffers,
#else
    /*const*/ amf::AMFBuffer ** amfBuffers,
#endif
    const int *             conv_lens,
    bool                    synchronous   // synchronoius call
    )
{
    auto context = m_pContextTAN->GetGeneralContext();
    auto uploadQ = m_pContextTAN->GetGeneralQueue();

    if(!context || !uploadQ)
    {
        return AMF_NOT_INITIALIZED;
    }

    for(int j = 0; j < channelsCount; j++)
    {
        CABuf<float> stg_buf(context);

#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_FAILED(stg_buf.attach(clBuffers[j], conv_lens[j]));
#else
        AMF_RETURN_IF_FAILED(stg_buf.attach(amfBuffers[j], conv_lens[j]));
#endif
        PrintAMFArrayReduced("stg_buf cre", stg_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), stg_buf.GetBuffer()->GetSize());

// do direct transform
        int uploadId = uploadIDs[j];
        int convId = convIDs[j];

        auto transf_buf = mKernelTransformed[uploadId][convId].get();

        AMF_RETURN_IF_FAILED(updateConvOCL(&stg_buf,  transf_buf, conv_lens[j], uploadQ, uploadId, convId));
    }

    if(synchronous)
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_CL_FAILED(clFinish(uploadQ));
#else
        AMF_RETURN_IF_FAILED(uploadQ->FinishQueue());
#endif
    }

    return AMF_OK;
}

AMF_RESULT CGraalConv::updateConv(
    int channelsCount,
    const int *uploadIDs,     // upload set IDs
    const int *convIDs,       // kernel IDs
    const int * conv_lens,
    bool synchronous          // synchronoius call
    )
{
    AMF_RESULT ret = AMF_OK;

    AMF_RETURN_IF_FAILED(
        uploadConvControlMaps(
            channelsCount,
            uploadIDs,     // upload set IDs
            convIDs,       // kernel IDs
            conv_lens
            )
        );

    AMF_RETURN_IF_FAILED(
        updateConvIntnl(
            channelsCount,
            uploadIDs,     // upload set IDs
            convIDs,       // kernel IDs
            conv_lens,
            synchronous   // synchronoius call
            )
        );

    return AMF_OK;
}

/**
 * All kernels will be ready upon the return from the call
 *
 * @return AMF_OK on success and AMF_FAIL on failure
 */
AMF_RESULT CGraalConv::finishUpdate(void)
{
    AMF_RESULT ret = syncUpload();

    return ret;
}

#ifdef TAN_SDK_EXPORTS
AMF_RESULT CGraalConv::copyResponses(
    uint channelsCnt,
    const uint pFromUploadIds[],
    const uint pToUploadIds[],
    const uint pConvIds[],
    const bool synchronous
    )
{
    for (uint channelId = 0; channelId < channelsCnt; channelId++)
    {
        AMF_RETURN_IF_FALSE(pFromUploadIds[channelId] < (uint)n_sets_, AMF_INVALID_ARG,
                            L"pFromUploadIds[channelId] out of range");
        AMF_RETURN_IF_FALSE(pToUploadIds[channelId] < (uint)n_sets_, AMF_INVALID_ARG,
                            L"pToUploadIds[channelId] out of range");
        AMF_RETURN_IF_FALSE(pConvIds[channelId] < (uint)mMaxChannels, AMF_INVALID_ARG,
                            L"pConvIds[channelId] out of range");

        /*const uint inOffset =*/
        mHostCopyRespInOffset[channelId] = (pFromUploadIds[channelId] * mMaxChannels + pConvIds[channelId]) * aligned_conv_sz_;
        /*const uint outOffset =*/
        mHostCopyRespOutOffset[channelId] = (pToUploadIds[channelId] * mMaxChannels + pConvIds[channelId]) * aligned_conv_sz_;
    }

    if( channelsCnt >= 1)
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueWriteBuffer(
                m_pContextTAN->GetGeneralQueue(),
                mCopyResponseInOffset->GetBuffer(),
                CL_FALSE,
                0,
                channelsCnt*sizeof(int),
                mHostCopyRespInOffset,
                0,
                NULL,
                NULL
                )
            );
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueWriteBuffer(
                m_pContextTAN->GetGeneralQueue(),
                mCopyResponseOutOffset->GetBuffer(),
                CL_FALSE,
                0,
                channelsCnt*sizeof(int),
                mHostCopyRespOutOffset,
                0,
                NULL,
                NULL
                )
            );
#else
        AMF_RETURN_IF_FAILED(
            m_pContextTAN->GetGeneralQueue()->CopyBufferFromHost(
                &mHostCopyRespOutOffset.front(),
                channelsCnt * sizeof(int),
                mCopyResponseInOffset->GetBuffer(),
                0,
                false
                )
            );
        AMF_RETURN_IF_FAILED(
            m_pContextTAN->GetGeneralQueue()->CopyBufferFromHost(
                &mHostCopyRespOutOffset.front(),
                channelsCnt * sizeof(int),
                mCopyResponseOutOffset->GetBuffer(),
                0,
                false
                )
            );
#endif

        amf_size n_arg = 0;

#ifndef TAN_NO_OPENCL
        cl_int ret = CL_SUCCESS;
        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(cl_mem), &mKernelTrasformedUnion->GetBuffer() );
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: mKernelTrasformedUnion->");

        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(cl_mem), &mKernelTrasformedUnion->GetBuffer() );
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: mKernelTrasformedUnion->");

        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(cl_mem), &mCopyResponseInOffset->GetBuffer()); //in offset
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: inOffset");

        int in_length = mKernelTrasformedUnion->GetCount();
        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(int), &in_length); //in offset
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: mKernelTrasformedUnion->GetCount()");

        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(cl_mem), &mCopyResponseOutOffset->GetBuffer()); //out offset
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: outOffset");

        int out_length = mKernelTrasformedUnion->GetCount();
        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(int), &out_length ); //out length
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: out length" );

        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(int), &aligned_conv_sz_ );//block length
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: aligned_conv_sz_");

        cl_int tmp = 0;
        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(int), &tmp );//pad length
        //ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(int), 0);//pad length
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: 0");

        ret = clSetKernelArg(mCopyWithPaddingKernel, n_arg++, sizeof(int), &channelsCnt);//channelCount
        AMF_RETURN_IF_CL_FAILED(ret, L"clSetKernelArg failed: 0");
#else
        //in
        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgBuffer(n_arg++, mKernelTrasformedUnion->GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));

        //out
        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgBuffer(n_arg++, mKernelTrasformedUnion->GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));

        //inOffset
        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgBuffer(n_arg++, mCopyResponseInOffset->GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));

        //inLength
        int in_length = mKernelTrasformedUnion->GetCount();
        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgInt32(n_arg++, in_length));

        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgBuffer(n_arg++, mCopyResponseOutOffset->GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));

        int out_length = mKernelTrasformedUnion->GetCount();
        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgInt32(n_arg++, out_length));

        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgInt32(n_arg++, aligned_conv_sz_));

        amf_int32 tmp = 0;
        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgInt32(n_arg++, tmp));

        AMF_RETURN_IF_FAILED(mCopyWithPaddingKernel->SetArgInt32(n_arg++, channelsCnt));
#endif

        size_t l_wk[3] = { size_t(std::min(aligned_conv_sz_, 256)), 1, 1 };
        size_t g_wk[3] = { size_t(aligned_conv_sz_), channelsCnt, 1 }; //divide by 4 as we process a vec4

#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_CL_FAILED(
            clEnqueueNDRangeKernel(
                m_pContextTAN->GetOpenCLGeneralQueue(),
                mCopyWithPaddingKernel,
                2,
                NULL,
                g_wk,
                l_wk,
                0,
                NULL,
                NULL
                )
            );
#else
        AMF_RETURN_IF_FAILED(
            mCopyWithPaddingKernel->Enqueue(
                2,
                nullptr,
                g_wk,
                l_wk
                )
            );
#endif
    }

    if(synchronous)
    {
#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_CL_FAILED(clFinish(this->m_pContextTAN->GetOpenCLGeneralQueue()));
#else
        AMF_RETURN_IF_FAILED(m_pContextTAN->GetAMFGeneralQueue()->FinishQueue());
#endif
    }

    return AMF_OK;
}
#endif // TAN_SDK_EXPORTS

AMF_RESULT CGraalConv:: getConvBuffers(int channelsCount, int *uploadIDs, int *convIDs, float** conv_ptrs)
{
    AMF_RESULT ret = AMF_OK;
    int i = 0;

    auto graalQueue = m_pContextTAN->GetGeneralQueue();

    // first asynchronous
    for( i = channelsCount - 1; i >= 0; i-- )
    {
        //ivm: conv_ptrs[i] = mKernelStaging[uploadIDs[i]][convIDs[i]]->mapA(graalQueue, CL_MAP_WRITE_INVALIDATE_REGION);
        assert(false);
        //--! :
        /*conv_ptrs[i] = mKernelStaging[uploadIDs[i]][convIDs[i]]->map(
            graalQueue,
            CL_MAP_WRITE_INVALIDATE_REGION
            );*/
    }

    return ret;
}

AMF_RESULT CGraalConv::getConvBuffers(
    int channelsCount,
    int * uploadIDs,                // upload set IDs per kernel
    int * convIDs,                  // kernel IDs
#ifndef TAN_NO_OPENCL
    cl_mem* clBuffers               // gpu-frendly system pointers
#else
    amf::AMFBuffer ** buffers       // gpu-frendly system pointers
#endif
    )
{
    AMF_RESULT ret = AMF_OK;

    for(int i = 0; i < channelsCount; i++)
    {
#ifndef TAN_NO_OPENCL
        clBuffers[i] = mKernelStaging[uploadIDs[i]][convIDs[i]]->GetBuffer();
#else
        buffers[i] = mKernelStaging[uploadIDs[i]][convIDs[i]]->GetBuffer();
#endif
    }

    return ret;
}

#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK

bool CGraalConv::checkForContiguousBuffers
(
    int             count,
    const float**   conv_ptrs,
    const int *     conv_lens
)
{
   if (count <= 1) //  don't need to do anything
        return false;
    bool contiguous = true;
    const float *start, *end;
    start = conv_ptrs[0];
    for (int i = 1; i < count; i++){
        end = start + conv_lens[i-1];
        start = conv_ptrs[i];
        if (end != start){
            contiguous = false;
            break;
        }
    }
    return contiguous;
}

#endif

AMF_RESULT CGraalConv::uploadConvHostPtrs(
        int channelsCount,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * conv_lens,
        bool synchronous   // synchronous call
        )
{
    AMF_RESULT ret = AMF_OK;

    auto uploadQ = m_pContextTAN->GetGeneralQueue();

    // If response buffers happen to be allocated contiguously, we can improve copy performance
    // by using a single copy.
#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
    if (checkForContiguousBuffers(channelsCount, conv_ptrs, conv_lens))
    {
        const float * conv_ptr = conv_ptrs[0];
        int len = conv_lens[0] * channelsCount;

                   //UploadIDs [index of current Set ]
        AMF_RETURN_IF_FAILED(
            mKernelInputStore[uploadIDs[0]]->copyToDeviceNonBlocking(
                uploadQ,
                0, //CL_MEM_READ_ONLY, --!
                conv_ptr,
                len
                )
            );
    }
    else
#endif
    {
        for (int j = 0; j < channelsCount; j++)
        {
            // move data into staging
            AMF_RETURN_IF_FAILED(
                    mKernelStaging[uploadIDs[j]][convIDs[j]]->copyToDeviceNonBlocking(
                    uploadQ,
                    0, //CL_MEM_READ_ONLY, --!
                    conv_ptrs[j],
                    conv_lens[j]
                    )
                );
        }
    }

    PrintAMFArrayReduced("mKernelInputUnion", mKernelInputUnion->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mKernelInputUnion->GetBuffer()->GetSize());

    if ( synchronous )
    {
        ret = syncUpload();
    }

    return ret;
}

AMF_RESULT                      CGraalConv::uploadConvGpuPtrs(
    int                         channelsCount,
    const int *                 uploadIDs,      // upload set IDs
    const int *                 convIDs,        // kernel IDs
#ifndef TAN_NO_OPENCL
    const cl_mem *              conv_ptrs,      // arbitrary cl_mem ptrs
#else
    const amf::AMFBuffer **     conv_ptrs,      // arbitrary amf buffers ptrs
#endif
    const int *                 conv_lens,
    bool                        synchronous     // synchronous call
)
{
    //PrintArrayReduced("upl ids", uploadIDs, 128*1024);
    //PrintArrayReduced("conv ids", convIDs, 128*1024);
    //PrintAMFArrayReduced("conv ptrs", conv_ptrs[0], m_pContextTAN->GetGeneralQueue(), 128*1024);
    //PrintAMFArrayReduced("conv ptrs", conv_ptrs[1], m_pContextTAN->GetGeneralQueue(), 128*1024);

    AMF_RESULT ret = AMF_OK;

    auto uploadQ = m_pContextTAN->GetGeneralQueue();

    for (int j = 0; j < channelsCount; j++)
    {
        // move data into staging
        AMF_RETURN_IF_FAILED(
            mKernelStaging[uploadIDs[j]][convIDs[j]]->CopyBufferMemory(conv_ptrs[j], uploadQ, conv_lens[j]*sizeof(float))
            );
    }

    if (synchronous)
    {
        ret = syncUpload();
    }

    return ret;
}

AMF_RESULT CGraalConv::process(
    int                 channelsCount,
    const int *         _uploadID,     // upload set IDs
    const int *         convIDs,       // kernel IDs
    const float*const*  inputs,
    float**             outputs,
    int                 _prev_input,
    int                 _advance_time,
    int                 _skip_stage,
    int                 _crossfade_state
    )
{
    AMF_RESULT ret = AMF_OK;

    PrintArrayReduced("inputs[0]", inputs[0], 32768 * sizeof(float));
    PrintArrayReduced("inputs[1]", inputs[1], 32768 * sizeof(float));

    GraalSampleBuffer outBuf; //!!
    outBuf.ReferHostBuffers(outputs);

    ret = processIntrnl(
            channelsCount,
            _uploadID,     // upload set IDs
            convIDs,       // kernel IDs
            inputs,
            outBuf,
            _prev_input,
            _advance_time,
            _skip_stage,
            _crossfade_state
            );

    PrintArrayReduced("outputs[0]", outputs[0], 32768 * sizeof(float));
    PrintArrayReduced("outputs[1]", outputs[1], 32768 * sizeof(float));

    return ret;
}

AMF_RESULT CGraalConv::process(
    int                 channelsCount,
    const int *         _uploadID,     // upload set IDs
    const int *         convIDs,       // kernel IDs
    const float*const*  inputs,
#ifndef TAN_NO_OPENCL
    cl_mem *            output,
#else
    amf::AMFBuffer **   output,
#endif
    int                 _prev_input,
    int                 _advance_time,
    int                 _skip_stage,
    int                 _crossfade_state
    )
{
    //PrintArrayReduced("CGraalConv::process in[0]", inputs[0], 128 * 1024);
    //PrintArrayReduced("CGraalConv::process in[1]", inputs[1], 128 * 1024);

    AMF_RESULT ret = AMF_OK;

    GraalSampleBuffer outBuf;
#ifndef TAN_NO_OPENCL
    outBuf.ReferCLBuffers(output);
#else
    outBuf.ReferAMFBuffers(output);
#endif

    ret = processIntrnl(
        channelsCount,
        _uploadID,     // upload set IDs
        convIDs,       // kernel IDs
        inputs,
        outBuf,
        _prev_input,
        _advance_time,
        _skip_stage,
        _crossfade_state
        );

#ifndef TAN_NO_OPENCL
    //PrintCLArrayReduced("graal", output[0], m_pContextTAN->GetGeneralQueue(), 8192);
    //PrintCLArrayReduced("graal", output[0], m_pContextTAN->GetGeneralQueue(), 8192);
#else
    //PrintAMFArrayReduced("graal", output[0], m_pContextTAN->GetGeneralQueue(), 8192);
    //PrintAMFArrayReduced("graal", output[0], m_pContextTAN->GetGeneralQueue(), 8192);
#endif

    return ret;
}

AMF_RESULT CGraalConv::flush(amf_uint channelId, const bool synchronous)
{
    AMF_RETURN_IF_FALSE(channelId < static_cast<amf_uint>(mMaxChannels), AMF_INVALID_ARG,
                        L"channelId out of range");

    AMF_RESULT ret = AMF_OK;

    switch (algorithm_)
    {
        case ALG_UNIFORMED:
        case ALG_UNI_HEAD_TAIL:
        {
            AMF_RETURN_IF_FAILED(
                zeroMemory(
                    mHistoryTransformed.get(),
                    channelId * aligned_conv_sz_,
                    aligned_conv_sz_
                    )
                );
            //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

            AMF_RETURN_IF_FAILED(
                zeroMemory(
                    &mProcessInputStaging,
                    channelId * n_input_blocks_ * aligned_proc_bufffer_sz_,
                    n_input_blocks_ * aligned_proc_bufffer_sz_
                    )
                );
            //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());

            for (uint setId = 0; setId < static_cast<uint>(n_sets_); setId++)
            {
                AMF_RETURN_IF_FAILED(
                    zeroMemory(
                        mCmadAccum.get(),
                        setId * mMaxChannels * accum_stride_ + channelId * accum_stride_,
                        accum_stride_
                        )
                    );
                AMF_RETURN_IF_FAILED(
                    zeroMemory(
                        mCmadAccumXF.get(),
                        setId * mMaxChannels * accum_stride_ + channelId * accum_stride_,
                        accum_stride_
                        )
                    );
            }
        }
        break;

        default:
            AMF_RETURN_IF_FAILED(AMF_NOT_IMPLEMENTED);
    }

    if (synchronous)
    {

#ifndef TAN_NO_OPENCL
        clFinish(m_pContextTAN->GetOpenCLGeneralQueue());
#else
        m_pContextTAN->GetAMFGeneralQueue()->FinishQueue();
#endif

        ////////AMF_RETURN_IF_FAILED(m_pComputeEngineUpdate->FinishQueue(), L"FinishQueue() failed");
    }

    return ret;
}

    /**
     * Upload kernels from a previously acquired gpu-friendly system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

AMF_RESULT CGraalConv::getDevInputPtrs(
        int channelsCount,
        int _uploadID,     // upload set IDs
        const int *convIDs,       // kernel IDs
        float** inputs
        )
{
    AMF_RESULT ret = AMF_OK;

    auto inQ = m_pContextTAN->GetGeneralQueue();

    assert(false);
    // --!
    /*float * inp_buf_ptr = mHostInputStaging[_uploadID]->map(inQ, CL_MAP_WRITE_INVALIDATE_REGION); --!

    for (int c = 0; c < channelsCount; c++ )
    {
        int convId = convIDs[c];
        inputs[c] = inp_buf_ptr + convId * aligned_proc_bufffer_sz_;
    }*/

    return ret;
}

    /**
     * Upload kernels from a previously acquired gpu-friendly system pointers.
     * Pointers become invalid after the call.
     *
     * @return AMF_OK on success and AMF_FAIL on failure
     */

AMF_RESULT CGraalConv::processDevPtrs(
        int channelsCount,
        int _uploadID,     // upload set ID
        const int *convIDs,       // kernel IDs
        float** inputs,
        float** outputs
        )
{
    AMF_ASSERT(AMF_NOT_IMPLEMENTED);
    return AMF_NOT_IMPLEMENTED;
}

/*---------------------------------------------------------------------------------------------
Internals
-----------------------------------------------------------------------------------------------*/
AMF_RESULT CGraalConv::processFinalize()
{
	if (!m_useProcessFinalize)
	{
		return AMF_OK;
	}
	switch (algorithm_)
	{
	case ALG_UNI_HEAD_TAIL:
		// accumulate the tail CMAD
		// for the next time step
		if (m_processParams_xf.skip_stage != 2 && m_processParams_xf.n_channels)
		{
			int str_shift = -1;
			int ret = processAccum(m_processParams_xf.n_channels, 1, str_shift, 1);
			m_processParams_xf.n_channels = 0;// Reset the num of channels to zero after being used as the xf flag
			if (ret != AMF_OK)
				return AMF_UNEXPECTED;
		}
		if (m_processParams.skip_stage != 2)
		{
			int str_shift = -1;
			int ret = processAccum(m_processParams.n_channels, 1, str_shift, 0);
			if (ret != AMF_OK)
				return AMF_UNEXPECTED;
		}
		break;
	}
	return AMF_OK;
}

/**
* Upload kernels from a previously acquired gpu-friendly system pointers.
* Pointers become invalid after the call.
*
* @return AMF_OK on success and AMF_FAIL on failure
*/
AMF_RESULT CGraalConv::processIntrnl(
    int                     channelsCount,
    const int *             uploadIDs,     // upload set IDs
    const int *             convIDs,       // kernel IDs
    const float*const*      inputs,
    GraalSampleBuffer &     output,
    int                     _prev_input,
    int                     _advance_time,
    int                     _skip_stage,
    int                     _crossfade_state
)
{
	if (m_useProcessFinalize)
	{
		if (_crossfade_state == 1)
		{
			m_processParams_xf.set(_prev_input, _advance_time, _skip_stage, channelsCount);
		}
		else
		{
			m_processParams.set(_prev_input, _advance_time, _skip_stage, channelsCount);
		}
	}

    AMF_RESULT ret = AMF_OK;

    auto inQ = m_pContextTAN->GetConvQueue();

    {
        // Copying the control and input data needed for the convolution engine,
        // Non-blocking calls has replaced the previous blocking mapping.

        // upload channel map
        CABuf<int> &chnl_map_buf = *mChannelsMap.get();
        // upload set map
        //CABuf<int> &set_map_buf = *mSetsMap.get();
		CABuf<int> &set_map_buf = (_crossfade_state == 1) ? *mSetsMapXf.get() : *mSetsMap.get();
        // upload input data
        CABuf<float> &inp_buf = mProcessInputStaging;
        //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());

        if (!_prev_input && _skip_stage != 1)
        {
            // Copying the channel mapping and the input data only if an output is going to be gnerated
            chnl_map_buf.copyToDeviceNonBlocking(inQ, 0, convIDs, channelsCount);
            for (int i = 0; i < channelsCount; i++)
            {
                int convId = convIDs[i];
                int input_index = getRoundCounter(0, convId) % n_input_blocks_;
                const float *in_ptr = inputs[i];

                //PrintArrayReduced("inptr", in_ptr, max_proc_buffer_sz_);

                //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());
                inp_buf.copyToDeviceNonBlocking(
                    inQ,
                    0,
                    in_ptr,
                    max_proc_buffer_sz_,
                    (convId * n_input_blocks_ + input_index) * aligned_proc_bufffer_sz_
                    );

                //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());

                if(align_padding_sz_!= 0)
                {
                    inp_buf.setValue2(inQ, 0, align_padding_sz_, max_proc_buffer_sz_);
                    //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());
                }
            }
        }
        // Copying the IR index
        set_map_buf.copyToDeviceNonBlocking(inQ, 0, uploadIDs, channelsCount);
    }

    //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());

    CABuf<float> & out_buf = *mProcess2OutputStaging.get();

    float * d_out_ptr(nullptr);

    auto outQ = m_pContextTAN->GetConvQueue();
    auto generalQ = m_pContextTAN->GetGeneralQueue();

    switch (algorithm_)
    {
    case ALG_UNIFORMED:
    default:
            // push data into the pipeline
        processPush(
            channelsCount,
            uploadIDs,     // upload set IDs
            convIDs,       // kernel IDs
            _prev_input
            );

        // accumulate CMAD
        processAccum(channelsCount);

        // push data out of pipeline
        processPull(
            channelsCount,
            uploadIDs,     // upload set IDs
            convIDs,       // kernel IDs
            _advance_time
            );

        // move data upstream
        if(output.IsHost() and false)
        {
            for (int c = 0; c < channelsCount; c++)
            {
                int uploadId = uploadIDs[c];
                int convId = convIDs[c];

                assert(
                    (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float)
                    +
                    max_proc_buffer_sz_ * sizeof(float)
                    <
                    131072
                    );

#ifndef TAN_NO_OPENCL
                int status = clEnqueueReadBuffer(
                    outQ,
                    out_buf.GetBuffer(),
                    (c == (channelsCount - 1)) ? CL_TRUE : CL_FALSE,
                    (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                    max_proc_buffer_sz_ * sizeof(float),
                    output.buffer.host[c],
                    0,
                    NULL,
                    NULL
                    );
                AMF_RETURN_IF_CL_FAILED(status);
#else
                AMF_RETURN_IF_FAILED(
                    outQ->CopyBufferToHost(
                        out_buf.GetBuffer(),
                        (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                        max_proc_buffer_sz_ * sizeof(float),
                        output.buffer.host[c],
                        (c == (channelsCount - 1)) ? true : false
                        )
                    );
#endif
            }
        }
        else if (output.IsComputeBuffer())
        {
			for (int c = 0; c < channelsCount; c++)
			{
				int uploadId = uploadIDs[c];
				int convId = convIDs[c];

#ifndef TAN_NO_OPENCL
				AMF_RETURN_IF_CL_FAILED(
                    clEnqueueCopyBuffer(
                        outQ,
                        out_buf.GetBuffer(),
                        output.buffer.clmem[c],
                        (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                        0,
                        max_proc_buffer_sz_ * sizeof(float),
                        0,
                        NULL,//Set the event for the first copy command only, the rest will line up since all in the same queue
                        NULL
                        )
                    );
#else
                AMF_RETURN_IF_FAILED(
                    outQ->CopyBuffer(
                        out_buf.GetBuffer(),
                        (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                        max_proc_buffer_sz_ * sizeof(float),
                        output.buffer.amfBuffers[c],
                        0
                        )
                    );
#endif
			}
        }


        break;
    case ALG_UNI_HEAD_TAIL:

        if (_skip_stage != 1)
        {

        // direct FHT, MAD + taiMAD, inverse FHT, advance time

            processHead1(
                channelsCount,
                uploadIDs,     // upload set IDs
                convIDs,       // kernel IDs
                _prev_input,	// use previous input data
                _advance_time, // update counters: 1, not: 0
                (_crossfade_state == 2)
                );


        // move data upstream
            if (output.IsHost())
            {
                for (int c = 0; c < channelsCount; c++)
                {
                    int uploadId = uploadIDs[c];
                    int convId = convIDs[c];

#ifndef TAN_NO_OPENCL
                    AMF_RETURN_IF_CL_FAILED(
                        clEnqueueReadBuffer(
                            outQ,
                            out_buf.GetBuffer(),
                            (c == (channelsCount-1)) ? CL_TRUE: CL_FALSE,
                            (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                            max_proc_buffer_sz_ * sizeof(float),
                            output.buffer.host[c],
                            0,
                            NULL,
                            NULL
                            )
                        );
#else
                    AMF_RETURN_IF_FAILED(
                        outQ->CopyBufferToHost(
                            out_buf.GetBuffer(),
                            (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                            max_proc_buffer_sz_ * sizeof(float),
                            output.buffer.host[c],
                            (c == (channelsCount-1)) ? true: false
                            )
                        );
#endif
                }
            }
            else if (output.IsComputeBuffer())
            {
                for (int c = 0; c < channelsCount; c++)
                {
                    int convId = convIDs[c];
                    int uploadId = uploadIDs[c];

#ifndef TAN_NO_OPENCL
                    AMF_RETURN_IF_CL_FAILED(
                        clEnqueueCopyBuffer(
                            outQ,
                            out_buf.GetBuffer(),
                            output.buffer.clmem[c],
                            (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                            0,
                            max_proc_buffer_sz_ * sizeof(float),
                            0,
                            NULL,
                            NULL
                            )
                        );
#else
                    AMF_RETURN_IF_FAILED(
                        outQ->CopyBuffer(
                            out_buf.GetBuffer(),
                            (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_ * sizeof(float),
                            max_proc_buffer_sz_ * sizeof(float),
                            output.buffer.amfBuffers[c],
                            0
                            )
                        );
#endif
                }
            }
        }

		if (!m_useProcessFinalize && _skip_stage != 2)
		{
            // accumulate the tail CMAD
            // for the next time step
            int str_shift = -1;
            processAccum(channelsCount, 1, str_shift, (_crossfade_state == 1));
        }
#if 0
        // update counters
        resetConvState(
            channelsCount,
            uploadIDs,
            convIDs,
            1);
#endif
        //	exit(0);
        break;
    }

    // at this point GPU-side counters has been updated already
    if (_advance_time)
    {
        // update CPU-side counters
        for (int i = 0; i < channelsCount; i++)
        {
            int convId = convIDs[i];

            incRoundCounter(0, convId);
        }

        incRoundCounter();
    }

    return ret;
}

AMF_RESULT CGraalConv:: uploadConvControlMaps(
    int channelsCount,
    const int *uploadIDs,     // upload set IDs
    const int *convIDs,       // kernel IDs
    const int * conv_lens
    )
{
    AMF_RESULT ret = AMF_OK;

    auto inQ = m_pContextTAN->GetGeneralQueue();

    // upload channel map
    CABuf<int> &chnl_map_buf = *mKernelChannelsMap.get();
    // upload set map
    CABuf<int> &set_map_buf = *mKernelSetsMap.get();
    // upload input data
    CABuf<int> &len_map_buf = *mKernelLensMap.get();

    // update only sent blocks area
    chnl_map_buf.copyToDeviceNonBlocking(inQ, 0, convIDs, channelsCount, 0);
    set_map_buf.copyToDeviceNonBlocking(inQ, 0, uploadIDs, channelsCount, 0);
    for (int i = 0; i < channelsCount; i++)
    {
        int convId = convIDs[i];
        int uploadId = uploadIDs[i];
        len_map_buf.copyToDeviceNonBlocking(inQ, 0, conv_lens + i, 1, uploadId * mMaxChannels + convId);
    }

    return ret;
}

AMF_RESULT CGraalConv:: updateConvIntnl(
        int channelsCount,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const int * conv_lens,
        bool synchronous   // synchronoius call
        )
{
    AMF_RESULT ret = AMF_OK;

    auto inQ = m_pContextTAN->GetGeneralQueue();

    // upload channel map
    CABuf<int> &chnl_map_buf = *mKernelChannelsMap.get();
    // upload set map
    CABuf<int> &set_map_buf = *mKernelSetsMap.get();
    // upload input data
    CABuf<int> &len_map_buf = *mKernelLensMap.get();

    CABuf<float> & stg_buf = *mKernelInputUnion.get();
    //todo: ivm: CASubBuf<float> & transf_buf = *mKernelTrasformedUnion.get();
    CABuf<float> & transf_buf = *mKernelTrasformedUnion.get();
    CABuf<float> & sincos = *mSinCos.get();
    CABuf<short> & bit_reverse = *mBitReverse.get();
    CABuf<uint> & round_counters = *mRoundCounters.get();
    CABuf<float> & data_hist = *mHistoryTransformed.get();
    //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

    uint in_version_stride = mMaxChannels * mMaxConvolutionSize;
    uint in_chnl_stride = mMaxConvolutionSize;
    uint out_version_stride = mMaxChannels * aligned_conv_sz_;
    uint out_chnl_stride = aligned_conv_sz_;
    uint version_stride = mMaxChannels;
    uint data_version_stride = mMaxChannels * aligned_conv_sz_;
    uint data_channel_stride = aligned_conv_sz_;

                                //todo: mVerify meaning and sign
    size_t l_wk[3] = { std::min(size_t(mAlignedProcessingSize >> 1), size_t(256)), (size_t)1, (size_t)1 };

    size_t g_wk[3] = {1,1,1};

    g_wk[0] = n_aligned_proc_blocks_* l_wk[0];
    g_wk[1] = channelsCount;
// run direct FHT for the 1st stream
    int n_arg = 0;
// direct FHT

#ifndef TAN_NO_OPENCL

    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &stg_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &transf_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &bit_reverse.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &sincos.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &set_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &len_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &round_counters.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(cl_mem), &data_hist.GetBuffer()));

    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &in_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &in_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &out_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &out_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &data_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel, n_arg++, sizeof(int), &data_channel_stride));

    AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(inQ, mUploadKernel, 2, NULL, g_wk, l_wk, 0, NULL, NULL));

    if(synchronous)
    {
        AMF_RETURN_IF_CL_FAILED(clFinish(inQ));
    }

#else

    PrintAMFArrayReduced("stg_buf",         stg_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(),          stg_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("transf_buf",      transf_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(),       transf_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("bit_reverse",     bit_reverse.GetBuffer(), m_pContextTAN->GetGeneralQueue(),      bit_reverse.GetBuffer()->GetSize());
    PrintAMFArrayReduced("sincos",          sincos.GetBuffer(), m_pContextTAN->GetGeneralQueue(),           sincos.GetBuffer()->GetSize());
    PrintAMFArrayReduced("chnl_map_buf",    chnl_map_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(),     chnl_map_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("set_map_buf",     set_map_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(),      set_map_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("len_map_buf",     len_map_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(),      len_map_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("round_counters",  round_counters.GetBuffer(), m_pContextTAN->GetGeneralQueue(),   round_counters.GetBuffer()->GetSize());
    PrintAMFArrayReduced("data_hist",       data_hist.GetBuffer(), m_pContextTAN->GetGeneralQueue(),        data_hist.GetBuffer()->GetSize());

    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, stg_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, transf_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, bit_reverse.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, sincos.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, set_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, len_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, round_counters.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgBuffer(n_arg++, data_hist.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));

    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, in_version_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, in_chnl_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, out_version_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, out_chnl_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, version_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, data_version_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel->SetArgInt32(n_arg++, data_channel_stride));

    //amdFHTConvIn
    AMF_RETURN_IF_FAILED(
        mUploadKernel->Enqueue(2, NULL, g_wk, l_wk) //+
        );
    m_pContextTAN->GetConvQueue()->FlushQueue();
    m_pContextTAN->GetConvQueue()->FinishQueue();

    if (synchronous)
    {
        AMF_RETURN_IF_FAILED(inQ->FinishQueue());
    }

    PrintAMFArrayReduced("transf_buf", transf_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), transf_buf.GetBuffer()->GetSize());

#endif

    if(mVerify == 1 || mVerify == 3)
    {
        transf_buf.copyToHost(inQ);
        stg_buf.copyToHost(inQ);
        size_t len = aligned_conv_sz_;
        float *ext_stg = new float[len];
        float * src_ptr = stg_buf.GetSystemMemory();
        float * tgt_ptr = transf_buf.GetSystemMemory();
        float * src_buf_ptr, * ext_buf_ptr, *tgt_buf_ptr;
        for(int j = 0; j < channelsCount; j++)
        {
            int n_test_loops = (conv_lens[j] + aligned_proc_bufffer_sz_ - 1)/aligned_proc_bufffer_sz_;
            int err = -1; //AMF_OK;
            int convId = convIDs[j];
            int uploadId = uploadIDs[j];

            src_buf_ptr = src_ptr + (uploadId * mMaxChannels + convId) * in_chnl_stride;
            ext_buf_ptr = ext_stg;
            tgt_buf_ptr = tgt_ptr + (uploadId * mMaxChannels + convId) * out_chnl_stride;
            memset(ext_stg, 0, len * sizeof(float));

            // src moves every block, second half padded with 0s
            for( int i = 0, k = 0, l = 0; i < n_test_loops; i++, l +=mAlignedProcessingSize, k+=aligned_proc_bufffer_sz_)
            {
                for ( int m = 0; m < aligned_proc_bufffer_sz_ && k + m < conv_lens[j]; m++)
                {
                    ext_buf_ptr[l + m] = src_buf_ptr[k + m];
                }
            }

            for (int i = 0; i < n_test_loops; i++)
            {
                err = FHT_verify((const __FLOAT__ *)ext_buf_ptr + mAlignedProcessingSize *i, (const __FLOAT__ *)tgt_buf_ptr + mAlignedProcessingSize *i,
                    mAlignedProcessingSize, 0, mAlignedProcessingSize, (__FLOAT__)1. );
                if ( err >= 0 ) {
#ifdef _DEBUG_PRINTF
                    printf("Kernel update mismatch: at block %d\n", i);
#endif
                    AMF_ASSERT_OK(AMF_UNEXPECTED, L"Kernel update mismatch: at block %d\n", i);
                    break;
                }

            }

#ifdef _DEBUG_PRINTF
#ifdef TAN_SDK_EXPORTS
            AMF_ASSERT(err == -1, L"Kernel update verified: u=%d c=%d len=%d",
                uploadId, convId, conv_lens[j]);
#else
            if ( err == -1 )  {
                std::cout << "Kernel update verified: u=" << uploadId << " c=" << convId << " len=" <<conv_lens[j] << "\n";
            }
#endif
#endif
        }

        delete [] ext_stg;
    }

    return ret;
}

AMF_RESULT CGraalConv::resetConvState(
    size_t channelsCount,
    const int *uploadIDs,
    const int *convIDs,
    int _time_step
    )
{
    AMF_RESULT ret = AMF_OK;
// uload maps

    auto uploadQ = m_pContextTAN->GetGeneralQueue();

// upload channel map
    CABuf<int> &chnl_map_buf = *mKernelChannelsMap.get();
    CABuf<uint> &count_buf = *mRoundCounters.get();

    uint data_version_stride = aligned_conv_sz_ * mMaxChannels;
    uint data_channel_stride = aligned_conv_sz_;
    uint version_stride = mMaxChannels;

    int n_arg = 0;

    size_t l_wk[3] = {256,1,1};
    size_t g_wk[3] = { channelsCount, 1, 1 };

#ifndef TAN_NO_OPENCL
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mResetKernel, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mResetKernel, n_arg++, sizeof(cl_mem), &count_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mResetKernel, n_arg++, sizeof(int), &_time_step));

    AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(uploadQ, mResetKernel, 1, NULL, g_wk, l_wk, 0, NULL, NULL));
#else
    AMF_RETURN_IF_FAILED(mResetKernel->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mResetKernel->SetArgBuffer(n_arg++, count_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mResetKernel->SetArgInt32(n_arg++, _time_step));

    AMF_RETURN_IF_FAILED(mResetKernel->Enqueue(1, NULL, g_wk, l_wk));
#endif

    return ret;
}

int CGraalConv::getRoundCounter(int _uploadId, int _chnl_id)
{
    int ret = (int)round_counter_;
    if ( _uploadId > -1 )
    {
        CABuf<uint> &count_buf = *mRoundCounters.get();
        ret = count_buf.GetSystemMemory()[_uploadId * mMaxChannels + _chnl_id];
    }
    return ret;
}

void CGraalConv::incRoundCounter(int _uploadId, int _chnl_id)
{
    if ( _uploadId > -1 )
    {
        CABuf<uint> &count_buf = *mRoundCounters.get();
        uint curr_round = count_buf.GetSystemMemory()[_uploadId * mMaxChannels + _chnl_id];
        curr_round++;
        count_buf.GetSystemMemory()[_uploadId * mMaxChannels + _chnl_id] = curr_round;
    }
    else
    {
        round_counter_++;
    }
}

AMF_RESULT CGraalConv::processHead1(
    int channelsCount,
    const int *uploadIDs,     // upload set IDs
    const int *convIDs,       // kernel IDs
    int _prev_input,           // uses the dat inserted into the pipeline on the previous round
    int _advance_time,         // advance time pointer (counter) - 1, not advance - 0
    bool _use_xf_buff
)
{
    AMF_RESULT ret = AMF_OK;

    auto inQ = m_pContextTAN->GetConvQueue();

    CABuf<float> &inp_buf = mProcessInputStaging;
    CABuf<float> &sincos = *mSinCos.get();
    CABuf<short> &bit_reverse = *mBitReverse.get();
    CABuf<int> &chnl_map_buf = *mChannelsMap.get();
    CABuf<int> &set_map_buf = *mSetsMap.get();
    CABuf<uint> &count_buf = *mRoundCounters.get();
    CABuf<float> &kern_store_buf = *mKernelTrasformedUnion.get();
    CABuf<float> &data_store_buf = *mHistoryTransformed.get();
    //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());
    //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

    CABuf<float> & accum_buf = _use_xf_buff ? *mCmadAccumXF.get() : *mCmadAccum.get();
    CABuf<float> & out_buf = *mProcess2OutputStaging.get();

    uint32_t n_in_blocks = n_input_blocks_;   // # of blocks kept in input staging
    uint n_conv_blocks = n_aligned_proc_blocks_;  // # of conv blocks (total)
    float scale = (float)0.5 / (float)mAlignedProcessingSize;        // inverse conv scale
    uint in_version_stride = aligned_proc_bufffer_sz_ * mMaxChannels * n_input_blocks_;
    uint in_chnl_stride = aligned_proc_bufffer_sz_* n_input_blocks_;
    uint hist_version_stride = aligned_conv_sz_ * mMaxChannels;
    uint hist_chnl_stride = aligned_conv_sz_;
    uint IR_version_stride = aligned_conv_sz_ * mMaxChannels;
    uint IR_chnl_stride = aligned_conv_sz_;
    uint accum_version_stride = accum_stride_ * mMaxChannels;
    uint accum_chnl_stride = accum_stride_;
    uint counter_version_stride = mMaxChannels;
    uint out_version_stride = aligned_proc_bufffer_sz_ * mMaxChannels;
    uint out_chnl_stride = aligned_proc_bufffer_sz_;

    int n_arg = 0;

    // direct transform
    size_t l_wk[3] = { size_t(std::min(mAlignedProcessingSize / 2, 256)), 1, 1 };
    size_t g_wk[3] = { 1, 1, 1 };
    g_wk[0] = l_wk[0];
    g_wk[1] = channelsCount;

#ifndef TAN_NO_OPENCL
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &inp_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &kern_store_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &accum_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &data_store_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &out_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &bit_reverse.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &sincos.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &n_in_blocks));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &n_conv_blocks));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(float), &scale));        // inverse conv scale
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &_prev_input));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &_advance_time));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &in_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &in_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &hist_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &hist_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &IR_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &IR_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &accum_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &accum_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &counter_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &out_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(int), &out_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &set_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mConvHead1, n_arg++, sizeof(cl_mem), &count_buf.GetBuffer()));

    AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(inQ, mConvHead1, 2, NULL, g_wk, l_wk, 0, NULL, &m_headTailKernelEvent));

    //todo: ivm: may be must be here (after usage)?
    if (m_headTailKernelEvent)
    {
        AMF_RETURN_IF_CL_FAILED(clReleaseEvent(m_headTailKernelEvent));
        m_headTailKernelEvent = NULL;
    }

    //todo: ivm: commented, seems not used!
    /*
    // THIS IS NOT implemented: running on the same Q
    int n_wait_events = 0;
    cl_event * p_wait_event = NULL;
    cl_event * p_set_event = NULL;
    if (eop_event_ != NULL)
    {
        n_wait_events = 1;
        p_wait_event = &eop_event_;
    }

    eoh_event_ = NULL;
    p_set_event = NULL; // &eoh_event_;

    ret = clEnqueueNDRangeKernel(inQ, convHead, 2, NULL, g_wk, l_wk, n_wait_events, p_wait_event, p_set_event);

    if (eop_event_ != NULL)
    {
        clReleaseEvent(eop_event_);
        eop_event_ = 0;
    }
    */
#else
    PrintAMFArrayReduced("inp_buf", inp_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), inp_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("kern_store_buf", kern_store_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), kern_store_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("accum_buf", accum_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), accum_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("data_store_buf", data_store_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), data_store_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("out_buf", out_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), out_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("bit_reverse", bit_reverse.GetBuffer(), m_pContextTAN->GetGeneralQueue(), bit_reverse.GetBuffer()->GetSize());

    PrintAMFArrayReduced("chnl_map_buf", chnl_map_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), chnl_map_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("set_map_buf", set_map_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), set_map_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("count_buf", count_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), count_buf.GetBuffer()->GetSize());

    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, inp_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, kern_store_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, accum_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, data_store_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, out_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, bit_reverse.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, sincos.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READ));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, n_in_blocks));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, n_conv_blocks));  // # of conv blocks (total)
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgFloat(n_arg++, scale));        // inverse conv scale
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, _prev_input));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, _advance_time));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, in_version_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, in_chnl_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, hist_version_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, hist_chnl_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, IR_version_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, IR_chnl_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, accum_version_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, accum_chnl_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, counter_version_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, out_version_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgInt32(n_arg++, out_chnl_stride));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, set_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mConvHead1->SetArgBuffer(n_arg++, count_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));

    //amdFHTConvHead1
    AMF_RETURN_IF_FAILED(mConvHead1->Enqueue(2, NULL, g_wk, l_wk));//+
    m_pContextTAN->GetConvQueue()->FlushQueue();
    m_pContextTAN->GetConvQueue()->FinishQueue();

    PrintAMFArrayReduced("data_store_buf", data_store_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), data_store_buf.GetBuffer()->GetSize());
    PrintAMFArrayReduced("out_buf", out_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), out_buf.GetBuffer()->GetSize());

#endif

     return ret;
}

AMF_RESULT CGraalConv::processPush(
    int channelsCount,
    const int *uploadIDs,     // upload set IDs
    const int *convIDs,       // kernel IDs
    int _prev_input
    )
{
    AMF_RESULT ret = AMF_OK;
// if previous input is uaed do nothing
    if (_prev_input)
    {
        return ret;
    }

    auto inQ = m_pContextTAN->GetConvQueue();

    CABuf<float> &inp_buf = mProcessInputStaging;
    CABuf<float> &dir_fht_buf = *mHistoryTransformed.get();
    //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());
    //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

    CABuf<float> &sincos = *mSinCos.get();
    CABuf<short> &bit_reverse = *mBitReverse.get();
    CABuf<int> &chnl_map_buf = *mChannelsMap.get();
    CABuf<int> &set_map_buf = *mSetsMap.get();
    CABuf<uint> &count_buf = *mRoundCounters.get();

    int in_version_stride = aligned_proc_bufffer_sz_ * mMaxChannels * n_input_blocks_;
    int in_chnl_stride = aligned_proc_bufffer_sz_* n_input_blocks_;
    int out_version_stride = aligned_conv_sz_ * mMaxChannels;
    int out_chnl_stride = aligned_conv_sz_;
    int version_stride = mMaxChannels;

// run direct FHT for the 1st stream
    int n_arg = 0;
    // direct FHT

    // direct transform
    size_t l_wk[3] = { size_t(std::min(mAlignedProcessingSize / 2, 256)), size_t(1), size_t(1) };
    size_t g_wk[3] = {1,1,1};
    g_wk[0] = l_wk[0];
    g_wk[1] = channelsCount;

#ifndef TAN_NO_OPENCL
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &inp_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &dir_fht_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &bit_reverse.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &sincos.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &n_input_blocks_));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &in_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &in_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &out_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &out_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &n_aligned_proc_blocks_));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &set_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(cl_mem), &count_buf.GetBuffer()));

    // TEST::
    int input_index = getRoundCounter();
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mDirectTransformKernel, n_arg++, sizeof(int), &input_index));

    AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(inQ, mDirectTransformKernel, 2, NULL, g_wk, l_wk, 0, NULL, NULL));
#else
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, inp_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, dir_fht_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, bit_reverse.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, sincos.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, n_input_blocks_));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, in_version_stride));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, in_chnl_stride));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, out_version_stride));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, out_chnl_stride));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, n_aligned_proc_blocks_));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, version_stride));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, set_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgBuffer(n_arg++, count_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));

    // TEST::
    int input_index = getRoundCounter();
    AMF_RETURN_IF_FAILED(mDirectTransformKernel->SetArgInt32(n_arg++, input_index));

    AMF_RETURN_IF_FAILED(mDirectTransformKernel->Enqueue(2, NULL, g_wk, l_wk));
#endif

    if ( mVerify > 1)
    {
#ifndef TAN_SDK_EXPORTS
        cl_command_queue inQ =  graalQueue;
#endif
        CABuf<uint> &count_buf = *mRoundCounters.get();
        CABuf<float> &inp_buf = mProcessInputStaging;
        CABuf<float> &dir_fht_buf = *mHistoryTransformed.get();
        //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());
        //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

        inp_buf.copyToHost(inQ);
        dir_fht_buf.copyToHost(inQ);
        float * inp_buf_ptr = inp_buf.GetSystemMemory();
        float * tgt_ptr = dir_fht_buf.GetSystemMemory();
        float * in_b = new float[mAlignedProcessingSize];
        int in_version_stride = aligned_proc_bufffer_sz_ * mMaxChannels * n_input_blocks_;
        int in_chnl_stride = aligned_proc_bufffer_sz_* n_input_blocks_;
        int out_version_stride = aligned_conv_sz_ * mMaxChannels;
        int out_chnl_stride = aligned_conv_sz_;
        int err = -1; //AMF_OK;
        for(int i = 0; i < channelsCount; i++ )
        {
        // current + previous input
            int convId = convIDs[i];
            int  uploadId = uploadIDs[i];
            int input_index_curr = getRoundCounter(0, convId) % n_input_blocks_;
            int input_index_prev = std::abs(getRoundCounter(0, convId)-1) % n_input_blocks_;

            float *in_proc_ptr = inp_buf_ptr + uploadId * in_version_stride + convId  * in_chnl_stride + input_index_curr * aligned_proc_bufffer_sz_;
            memcpy(in_b, in_proc_ptr, aligned_proc_bufffer_sz_ * sizeof(float));
            in_proc_ptr = inp_buf_ptr + uploadId * in_version_stride + convId  * in_chnl_stride + input_index_prev * aligned_proc_bufffer_sz_;
            memcpy(in_b + aligned_proc_bufffer_sz_, in_proc_ptr, aligned_proc_bufffer_sz_ * sizeof(float));
            int output_index = getRoundCounter(0, convId) % n_aligned_proc_blocks_;
            float * tg_b = tgt_ptr + out_chnl_stride * convId + mAlignedProcessingSize * output_index;
            err = FHT_verify((const __FLOAT__ *)in_b, (const __FLOAT__ *)tg_b, mAlignedProcessingSize, 0, mAlignedProcessingSize, (__FLOAT__)1. );
            if ( err >= 0 ) {
#ifdef _DEBUG_PRINTF
                std::cout <<  "Process direct tarnsform mismatch: round " << (int)getRoundCounter(uploadId, convId) << " channel " << i << "\n";
#endif
                AMF_ASSERT_OK(AMF_UNEXPECTED, L"Process direct tarnsform mismatch: "
                                              L"round %d channel %d",
                              (int)getRoundCounter(uploadId, convId), i);
                break;
            }
        }
        delete [] in_b;

#ifdef _DEBUG_PRINTF
#ifdef TAN_SDK_EXPORTS
        AMF_ASSERT(err == -1, L"Process direct tarnsform verified : %d", (int)getRoundCounter(0, 0));
#else
        if ( err == -1 )  {
            std::cout << "Process direct tarnsform verified : " << (int)getRoundCounter(0, 0) << "\n";
        }
#endif
#endif
    }

    return ret;
}

AMF_RESULT CGraalConv::processAccum(
    int channelsCount,
    int _IR_bin_shift,
    int _STR_bin_shift,
    bool _use_xf_buff

#ifndef TAN_SDK_EXPORTS
                        cl_command_queue _graalQ
#endif
                        )
{
    AMF_RESULT ret = AMF_OK;
// starting convolution from bin 0 - uniform, single stage  pipeline, 1 - head tail
    int IR_bin_shift = _IR_bin_shift;//
    int STR_bin_shift = _STR_bin_shift; // time shift 0, uniform -1, head tail

    int total_n_bins = n_aligned_proc_blocks_ - IR_bin_shift;
    int headRun = ((total_n_bins + n_accum_blocks_ - 1) / n_accum_blocks_);

// head
    auto inQ = m_pContextTAN->GetConvQueue();

    auto headAccumKernel = mCMADKernels[0];

    //todo: ivm: CASubBuf<float> &kern_store_buf = *(CASubBuf<float> *)mKernelTrasformedUnion;
    CABuf<float> &kern_store_buf = *mKernelTrasformedUnion;
    CABuf<float> &data_store_buf = *mHistoryTransformed.get();
    //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

    CABuf<float> &accum_buf = _use_xf_buff ? *mCmadAccumXF.get() : *mCmadAccum.get();
    CABuf<int> &chnl_map_buf = *mChannelsMap.get();
   // CABuf<int> &set_map_buf = *mSetsMap.get();
	CABuf<int> &set_map_buf = _use_xf_buff ? *mSetsMapXf.get() : *mSetsMap.get();
	CABuf<uint> &count_buf = *mRoundCounters.get();

    uint store_version_stride = aligned_conv_sz_ * mMaxChannels;
    uint accum_version_stride = accum_stride_ * mMaxChannels;
    int n_arg = 0;

    size_t l_wk[3] = { size_t(std::min(mAlignedProcessingSize / 2, 256)), size_t(1), size_t(1) };
    size_t g_wk[3] = { 1, 1, 1 };
    g_wk[0] = mAlignedProcessingSize / 2;
    g_wk[1] = headRun;
    g_wk[2] = channelsCount;

#ifndef TAN_NO_OPENCL
    if (headRun > 0)
    {
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(cl_mem), &kern_store_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(cl_mem), &data_store_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(cl_mem), &accum_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &accum_version_stride));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &accum_stride_));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &store_version_stride));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &aligned_conv_sz_));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &IR_bin_shift));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &n_aligned_proc_blocks_));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &n_accum_blocks_));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &mMaxChannels));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(cl_mem), &set_map_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(cl_mem), &count_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(headAccumKernel, n_arg++, sizeof(int), &STR_bin_shift));

        //todo: ivm: seems unused, investigate
        /*
        //	eoh_event_ is set only in the head stage of the head -tail algorithm
        // the classic algorithms runs with a single queue and does not use events at all
        int n_wait_events = 0;
        cl_event * p_wait_event = NULL;
        cl_event * p_set_event = NULL;

        eop_event_ = NULL;

        // THIS IS NOT implemented: running on the same Q
        // if event has been setup in the 1st tsage wiat for it
        if (eoh_event_ != NULL)
        {
            n_wait_events = 1;
            p_wait_event = &eoh_event_;
        }

        // THIS IS NOT implemented: running on the same Q
        // setup an even the next raound will wait for
        if (eoh_event_ != NULL && headRun == 1)
        {
            p_set_event = &eop_event_;
        }

        if (headRun > 0)
        {
            ret = clEnqueueNDRangeKernel(inQ, headAccumKernel, 3, NULL, g_wk, l_wk, n_wait_events, p_wait_event, p_set_event);
        }
        */
        AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(inQ, headAccumKernel, 3, NULL, g_wk, l_wk, 0, NULL, NULL));
    }
#else
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgBuffer(n_arg++, kern_store_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgBuffer(n_arg++, data_store_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgBuffer(n_arg++, accum_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, accum_version_stride));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, accum_stride_));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, store_version_stride));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, aligned_conv_sz_));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, IR_bin_shift));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, n_aligned_proc_blocks_));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, n_accum_blocks_));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, mMaxChannels));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgBuffer(n_arg++, set_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgBuffer(n_arg++, count_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->SetArgInt32(n_arg++, STR_bin_shift));

    //FHTMultAddHead2
    AMF_RETURN_IF_FAILED(mCMADKernels[0]->Enqueue(3, NULL, g_wk, l_wk));//+

    m_pContextTAN->GetConvQueue()->FlushQueue();
    m_pContextTAN->GetConvQueue()->FinishQueue();

    PrintAMFArrayReduced("accum_buf", accum_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), accum_buf.GetBuffer()->GetSize());
#endif

    total_n_bins = headRun;
    int last_tail = 0;

// tail
    if (total_n_bins > 1)
    {
        int tailRun = headRun;

        int n_arg = 0, last_arg;

        auto inQ = m_pContextTAN->GetConvQueue();

        auto tailAccumKernel = mCMADKernels[1];

        CABuf<float> &accum_buf = _use_xf_buff ? *mCmadAccumXF.get() : *mCmadAccum.get();
		CABuf<int> &set_map_buf = _use_xf_buff ? *mSetsMapXf.get() : *mSetsMap.get();
		CABuf<int> &chnl_map_buf = *mChannelsMap.get();
        uint accum_version_stride = accum_stride_ * mMaxChannels;

#ifndef TAN_NO_OPENCL
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, n_arg++, sizeof(cl_mem), &accum_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, n_arg++, sizeof(cl_mem), &set_map_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, n_arg++, sizeof(int), &accum_version_stride));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, n_arg++, sizeof(int), &accum_stride_));
        AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, n_arg++, sizeof(int), &n_accum_blocks_));

        do {
            last_arg = n_arg;
            total_n_bins = tailRun;

            AMF_RETURN_IF_CL_FAILED(clSetKernelArg(tailAccumKernel, last_arg++, sizeof(int), &total_n_bins));

            tailRun = (total_n_bins + n_accum_blocks_ - 1) / n_accum_blocks_;
            size_t l_wk[3] = {size_t(std::min(mAlignedProcessingSize, 256)),1,1};
            size_t g_wk[3] = {1,1,1};

            g_wk[0] = mAlignedProcessingSize;
            g_wk[1] = tailRun;
            g_wk[2] = channelsCount;

            last_tail = (int)(tailRun == 1);

            /*
            // THIS IS NOT implemented: running on the same Q
            // setup an even the next round will wait for
            if (eoh_event_ != NULL && last_tail)
            {
                p_set_event = &eop_event_;
            }

            ret = clEnqueueNDRangeKernel(inQ, tailAccumKernel, 3, NULL, g_wk, l_wk, 0, NULL, p_set_event);
            */

            AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(inQ, mCMADKernels[1], 3, NULL, g_wk, l_wk, 0, NULL, NULL));
        } while (!last_tail);

        // Pushing the jobs to be flushed from command buffer and be submitted to the device in a non-blocking way
        AMF_RETURN_IF_CL_FAILED(clEnqueueReadBuffer(inQ, accum_buf.GetBuffer(), CL_FALSE,0, 8 * sizeof(float), m_dataBuff, 0 , NULL, NULL));
#else
        AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgBuffer(n_arg++, accum_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
        AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgBuffer(n_arg++, set_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
        AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
        AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgInt32(n_arg++, accum_version_stride));
        AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgInt32(n_arg++, accum_stride_));
        AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgInt32(n_arg++, n_accum_blocks_));

        do
        {
            last_arg = n_arg;
            total_n_bins = tailRun;

            AMF_RETURN_IF_FAILED(mCMADKernels[1]->SetArgInt32(last_arg++, total_n_bins));

            tailRun = (total_n_bins + n_accum_blocks_ - 1) / n_accum_blocks_;
            size_t l_wk[3] = {size_t(std::min(mAlignedProcessingSize, 256)),1,1};
            size_t g_wk[3] = {1,1,1};

            g_wk[0] = mAlignedProcessingSize;
            g_wk[1] = tailRun;
            g_wk[2] = channelsCount;

            last_tail = (int)(tailRun == 1);

            /*
            // THIS IS NOT implemented: running on the same Q
            // setup an even the next round will wait for
            if (eoh_event_ != NULL && last_tail)
            {
                p_set_event = &eop_event_;
            }

            ret = clEnqueueNDRangeKernel(inQ, tailAccumKernel, 3, NULL, g_wk, l_wk, 0, NULL, p_set_event);
            */

            //FHTMultAddTail
            AMF_RETURN_IF_FAILED(mCMADKernels[1]->Enqueue(3, NULL, g_wk, l_wk));//+

            m_pContextTAN->GetConvQueue()->FlushQueue();
            m_pContextTAN->GetConvQueue()->FinishQueue();

            PrintAMFArrayReduced("accum_buf", accum_buf.GetBuffer(), m_pContextTAN->GetGeneralQueue(), accum_buf.GetBuffer()->GetSize());
        }
        while(!last_tail);

        // Pushing the jobs to be flushed from command buffer and be submitted to the device in a non-blocking way
        AMF_RETURN_IF_FAILED(
            inQ->CopyBufferToHost(
                accum_buf.GetBuffer(),
                0,
                8 * sizeof(float),
                m_dataBuff,
                false
                )
            );
#endif
    }

    /*
    if (eoh_event_ != NULL)
    {
        clReleaseEvent(eoh_event_);
        eoh_event_ = NULL;
    }
    */

    return ret;
}

AMF_RESULT CGraalConv::processPull(
        int channelsCount,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        int _advance_time
        )
{
    AMF_RESULT ret = AMF_OK;
#ifndef TAN_SDK_EXPORTS
    cl_kernel outFhtKernel = mInverseTransformKernel;
#endif

    uint out_version_stride = aligned_proc_bufffer_sz_ * mMaxChannels;
    uint counter_version_stride = mMaxChannels;

    int in_version_stride = accum_stride_ * mMaxChannels;
    int in_chnl_stride = accum_stride_;
    int out_chnl_stride = aligned_proc_bufffer_sz_;
    float scale = (float)0.5 / (float)mAlignedProcessingSize;

    auto outQ = m_pContextTAN->GetConvQueue();

    CABuf<float> & sum_buf = *mCmadAccum.get();
    CABuf<float> & out_buf = *mProcess2OutputStaging.get();
    CABuf<float> & sincos = *mSinCos.get();
    CABuf<short> & bit_reverse = *mBitReverse.get();

    CABuf<int> &chnl_map_buf = *mChannelsMap.get();
    CABuf<int> &set_map_buf = *mSetsMap.get();
    CABuf<uint> &count_buf = *mRoundCounters.get();


    int n_arg = 0;
    // inverse FHT

    size_t l_wk[3] = { size_t(std::min(mAlignedProcessingSize / 2, 256)), 1, 1 };
    size_t g_wk[3] = { 1, 1, 1 };

    g_wk[0] = l_wk[0];
    g_wk[1] = channelsCount;

#ifndef TAN_NO_OPENCL
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &sum_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &out_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &bit_reverse.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &sincos.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(int), &in_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(int), &in_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(int), &out_version_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(int), &out_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(float), &scale));
    //@todo: why are we passing uint as a float?
    printf("todo: why are we passing uint as a float?\n");
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(float), &counter_version_stride));        // inverse conv scale
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &chnl_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &set_map_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(cl_mem), &count_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mInverseTransformKernel, n_arg++, sizeof(int), &_advance_time));

	AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(outQ, mInverseTransformKernel, 2, NULL, g_wk, l_wk, 0, NULL, &m_pullKernelEvent));

    if (m_pullKernelEvent)
    {
        clReleaseEvent(m_pullKernelEvent); m_pullKernelEvent = NULL;
    }
#else
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, sum_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, out_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, bit_reverse.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, sincos.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgInt32(n_arg++, in_version_stride));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgInt32(n_arg++, in_chnl_stride));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgInt32(n_arg++, out_version_stride));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgInt32(n_arg++, out_chnl_stride));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgFloat(n_arg++, scale));

    //@todo: why are we passing uint as a float??
#pragma message("why are we passing uint as a float??")
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgFloat(n_arg++, *reinterpret_cast<float*>(&counter_version_stride)));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, chnl_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, set_map_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgBuffer(n_arg++, count_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mInverseTransformKernel->SetArgInt32(n_arg++, _advance_time));

    AMF_RETURN_IF_FAILED(mInverseTransformKernel->Enqueue(2, NULL, g_wk, l_wk), L"Enqueue() failed");

    //todo: ivm: seems unused
    /*
    if (m_pullKernelEvent)
    {
        clReleaseEvent(m_pullKernelEvent);
        m_pullKernelEvent = NULL;
    }
    */
#endif

    if (mVerify > 1)
    {
        int in_version_stride = accum_stride_ * mMaxChannels;
        int in_chnl_stride = accum_stride_;
        int out_chnl_stride = aligned_proc_bufffer_sz_;
        float scale = (float)0.5 / (float)mAlignedProcessingSize;

        auto outQ = m_pContextTAN->GetConvQueue();

        CABuf<float> & sum_buf = *mCmadAccum.get();
        CABuf<float> & out_buf = *mProcess2OutputStaging.get();

        sum_buf.copyToHost(outQ);

        assert(false);
        //float * tgt_ptr = out_buf.map(outQ, CL_MAP_READ); --!
        float * src_ptr = sum_buf.GetSystemMemory();
        int err = -1;

        // --!
        /*for (int i = 0; i < channelsCount; i++)
        {
            int convId = convIDs[i];
            int uploadId = uploadIDs[i];
            err = FHT_verify((const __FLOAT__ *)src_ptr + uploadId * in_version_stride + convId * accum_stride_, (const __FLOAT__ *)tgt_ptr + (uploadId * mMaxChannels + convId) * aligned_proc_bufffer_sz_, mAlignedProcessingSize, 1, aligned_proc_bufffer_sz_, 0.5f);
            if (err >= 0) {
#ifdef _DEBUG_PRINTF
                std::cout << "Process invert transform mismatch: round " << (int)getRoundCounter(uploadId, convId) << " channel " << i << "\n";
#endif
                AMF_ASSERT_OK(AMF_UNEXPECTED, L"Process invert transform mismatch: "
                                              L"round %d channel %d",
                              (int)getRoundCounter(uploadId, convId), i);
                break;
            }
        }*/

        assert(false);
        //out_buf.unmap(); --!

#ifdef _DEBUG_PRINTF
        if (err == -1)  {
            std::cout << "Process inverse tarnsform verified : " << (int)(int)getRoundCounter(0, 0) << "\n";
        }
#endif
        AMF_ASSERT(err == -1, L"Process inverse tarnsform verified : %d",
                   (int)(int)getRoundCounter(0, 0));
    }

        return ret;
}

AMF_RESULT CGraalConv::updateConvOCL(
    CABuf<float> *      _stg_buf,
    CASubBuf<float> *   _transf_buf,
    int                 _conv_len,
#ifndef TAN_NO_OPENCL
    cl_command_queue    _uploadQ,
#else
    amf::AMFCompute *    _uploadQ,
#endif
    int                 _uploadID,
    int                 _convID
    )
{
    AMF_RESULT ret = AMF_OK;

    CABuf<float> & stg_buf = *(CABuf<float>*)_stg_buf;
    CASubBuf<float> & transf_buf = *(CASubBuf<float> *)_transf_buf;
    CABuf<float> & sincos = *mSinCos.get();
    CABuf<short> & bit_reverse = *mBitReverse.get();

    size_t l_wk[3] = { size_t(std::min(mAlignedProcessingSize / 2, 256)), 1, 1 };

    size_t g_wk[3] = {1,1,1};

    g_wk[0] = n_aligned_proc_blocks_* l_wk[0];

// run direct FHT for the 1st stream
    int n_arg = 0;
// direct FHT

    int in_chnl_stride = mMaxConvolutionSize;
    int out_chnl_stride = aligned_conv_sz_;

#ifndef TAN_NO_OPENCL
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(cl_mem), &stg_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(cl_mem), &transf_buf.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(cl_mem), &bit_reverse.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(cl_mem), &sincos.GetBuffer()));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(int), &_conv_len));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(int), &in_chnl_stride));
    AMF_RETURN_IF_CL_FAILED(clSetKernelArg(mUploadKernel2, n_arg++, sizeof(int), &out_chnl_stride));

    AMF_RETURN_IF_CL_FAILED(clEnqueueNDRangeKernel(_uploadQ, mUploadKernel2, 1, NULL, g_wk, l_wk, 0, NULL, NULL));
#else
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgBuffer(n_arg++, stg_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgBuffer(n_arg++, transf_buf.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgBuffer(n_arg++, bit_reverse.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgBuffer(n_arg++, sincos.GetBuffer(), amf::AMF_ARGUMENT_ACCESS_READWRITE));
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgInt32(n_arg++, _conv_len));
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgInt32(n_arg++, in_chnl_stride));
    AMF_RETURN_IF_FAILED(mUploadKernel2->SetArgInt32(n_arg++, out_chnl_stride));

    AMF_RETURN_IF_FAILED(mUploadKernel2->Enqueue(1, NULL, g_wk, l_wk));
#endif

    if ( mVerify  == 1 || mVerify == 3)
    {
        transf_buf.copyToHost(_uploadQ);
        stg_buf.copyToHost(_uploadQ);
        size_t len = transf_buf.GetCount();
        float *ext_stg = new float[len];
        memset(ext_stg, 0, len * sizeof(float));
        float * src_ptr = stg_buf.GetSystemMemory();
        float * tgt_ptr = transf_buf.GetSystemMemory();
        float * src_buf_ptr, * ext_buf_ptr, *tgt_buf_ptr;

        int n_test_loops = (_conv_len + aligned_proc_bufffer_sz_ - 1)/aligned_proc_bufffer_sz_;
        int err = -1; //AMF_OK;
        src_buf_ptr = src_ptr; // + j * in_chnl_stride;
        ext_buf_ptr = ext_stg; // + j * out_chnl_stride;
        tgt_buf_ptr = tgt_ptr;

            // src moves every block, second half padded with 0s
            for( int i = 0, k = 0, l = 0; i < n_test_loops; i++, l +=mAlignedProcessingSize, k+=aligned_proc_bufffer_sz_)
            {
                for ( int j = 0; j < aligned_proc_bufffer_sz_ && k + j < _conv_len; j++)
                {
                    ext_buf_ptr[l + j] = src_buf_ptr[k + j];
                }
            }

            for (int i = 0; i < n_test_loops; i++)
            {
                err = FHT_verify((const __FLOAT__ *)ext_buf_ptr + mAlignedProcessingSize *i, (const __FLOAT__ *)tgt_buf_ptr + mAlignedProcessingSize *i,
                    mAlignedProcessingSize, 0, mAlignedProcessingSize, (__FLOAT__)1. );
                if ( err >= 0 ) {
#ifdef _DEBUG_PRINTF
                    printf("Kernel update mismatch: at block %d\n", i);
#endif
                    AMF_ASSERT_OK(AMF_UNEXPECTED, L"Kernel update mismatch: at block %d", i);
                    break;
                }

            }


#ifdef _DEBUG_PRINTF
        if ( err == -1 )  {
            std::cout << "Kernel update verified: u=" << _uploadID << " c=" << _convID << " len=" <<_conv_len << "\n";
        }
#endif
        AMF_ASSERT(err == -1, L"Kernel update verified: u=%d c=%d len=%d",
                   _uploadID, _convID, _conv_len);
    }
    return ret;
}

AMF_RESULT CGraalConv::uploadConvHostPtrIntnl(
        int channelsCount,
        const int *uploadIDs,     // upload set IDs
        const int *convIDs,       // kernel IDs
        const float** conv_ptrs,  // arbitrary host ptrs
        const int * conv_lens
        )
{
    AMF_RESULT ret = AMF_OK;

// move data into staging
    auto uploadQ = m_pContextTAN->GetGeneralQueue();

    for(int j = 0; j < channelsCount; j++)
    {

        int convId = convIDs[j];
        int uploadId = uploadIDs[j];
        int conv_len = conv_lens[j];
        const float * conv_ptr = conv_ptrs[j];

        // move data into staging
        //	ret = stg_buf->attach(_conv_ptr, _conv_len);
        AMF_RETURN_IF_FAILED(
            mKernelStaging[uploadId][convId]->copyToDeviceNonBlocking(
                uploadQ,
                0, //CL_MEM_READ_ONLY, --!
                conv_ptr,
                conv_len
                )
            );
        // do direct transform
    }

    return ret;
}

AMF_RESULT CGraalConv::syncUpload( void )
{
    AMF_RESULT ret = AMF_OK;

#ifndef TAN_NO_OPENCL
    clFinish(m_pContextTAN->GetGeneralQueue());
#else
    m_pContextTAN->GetGeneralQueue()->FinishQueue();
#endif

    return ret;
}

//-cl-fp32-correctly-rounded-divide-sqrt
void CGraalConv::selectOptions(std::string & kernelFileName, std::string & kernelOptions)
{
    int group_sz = std::min(mAlignedProcessingSize / 2, 256);
    int log2_group_sz = static_cast<int>(ceil(log2((double)group_sz)));

    kernelOptions =
        //std::string("-cl-fp32-correctly-rounded-divide-sqrt ") +
        std::string("-D _K0_GROUP_SZ=") + std::to_string((long long)group_sz) +
        std::string(" -D _K0_LOG2_GROUP_SZ=") + std::to_string((long long)log2_group_sz) +
        std::string(" -D _K0_LOG2_N=") + std::to_string((long long)(processing_log2_ + 1)) +
        std::string(" -D _K0_N=") + std::to_string((long long)mAlignedProcessingSize)
        ;
}

AMF_RESULT CGraalConv::selectUploadOptions(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName = "amdFHTConvIn";

//	kernelOptions = "-x clc++ -D _K0_GROUP_SZ=256 -D _K0_N=2048 -D _K0_LOG2_N=11";
    selectOptions(kernelFileName, kernelOptions);
    return ret;
}

AMF_RESULT CGraalConv::selectUpload2Options(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName = "amdFHTUploadConv";

    selectOptions(kernelFileName, kernelOptions);
    return ret;
}

AMF_RESULT CGraalConv::selectResetOptions(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName = "amdFHTAdvanceTime";
    selectOptions(kernelFileName, kernelOptions);
    return ret;
}

AMF_RESULT CGraalConv::selectDirectFHTOptions(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName = "amdFHTPushIn";
    selectOptions(kernelFileName, kernelOptions);
    return ret;
}

AMF_RESULT CGraalConv::selectFHT_CMADOptions(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::vector<std::string> & kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName.resize(2);
    kernelName[0] = "FHTMultAddHead2";
    kernelName[1] = "FHTMultAddTail";

    selectOptions(kernelFileName, kernelOptions);
    return ret;
}

AMF_RESULT CGraalConv::selectInverseFHTOptions(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string& kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName = "amdFHTPushOut";
    selectOptions(kernelFileName, kernelOptions);

    return ret;
}

AMF_RESULT CGraalConv::selectConvHead1Options(std::string & kernelFileName, std::vector<amf_uint8> &kernelSource, size_t &kernelSourceSize, std::string &kernelName, std::string & kernelOptions)
{
    AMF_RESULT ret = AMF_OK;
    kernelFileName = "GraalFHT";
    kernelSource = std::vector<amf_uint8>(GraalFHT, GraalFHT + GraalFHTCount);
    kernelSourceSize = GraalFHTCount;
    kernelName = "amdFHTConvHead1";
    selectOptions(kernelFileName, kernelOptions);
    return ret;
}

#ifdef TAN_SDK_EXPORTS

AMF_RESULT CGraalConv::zeroMemory(CABuf<float> *pBuf, amf_uint offset, amf_uint amount)
{
    AMF_RETURN_IF_FAILED(
        pBuf->setValue2(
            m_pContextTAN->GetGeneralQueue(),
            0.0f,
            amount,
            offset
            )
            );

    return AMF_OK;
}

#endif

AMF_RESULT CGraalConv::Setup
(
    const amf::AMFComputePtr & pComputeConvolution,
    const amf::AMFComputePtr & pComputeUpdate

#ifndef TAN_SDK_EXPORTS
    cl_context          _clientContext,
    cl_device_id        _clientDevice,
    cl_command_queue    _clientQ
#endif
)
{
    AMF_RESULT ret = AMF_OK;
	//?? cl_queue_properties prop[] = { 0 };

    auto context = m_pContextTAN->GetGeneralContext();
    auto graalQueue = m_pContextTAN->GetGeneralQueue();

    mKernelInputUnion.reset(new CABuf<float>(context));
    assert(mKernelInputUnion);                                                  //pComputeConvolution or pComputeUpdate?
    ret = mKernelInputUnion->create(n_sets_* mMaxChannels * mMaxConvolutionSize, 0, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mKernelInputUnion.get(), graalQueue);
    PrintAMFArrayReduced("mKernelInputUnion", mKernelInputUnion->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mKernelInputUnion->GetBuffer()->GetSize());

    mKernelTrasformedUnion.reset(new CABuf<float>(context));
    assert(mKernelTrasformedUnion);
    ret = mKernelTrasformedUnion->create(n_sets_* mMaxChannels *aligned_conv_sz_, 0, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mKernelTrasformedUnion.get(), graalQueue);

    AMFTraceInfo(
        AMF_FACILITY,
        L"Kernel storage size = %6.2fMB\n",
        (float)(mKernelTrasformedUnion->GetCount() * sizeof(float)) / 1024 / 1024
        );

    // kernel channels map
    mKernelChannelsMap.reset(new CABuf<int>(context));
    assert(mKernelChannelsMap);
    ret = mKernelChannelsMap->create(mMaxChannels, CL_MEM_USE_PERSISTENT_MEM_AMD, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mKernelChannelsMap.get(), graalQueue);

    // kernel sets map
    mKernelSetsMap.reset(new CABuf<int>(context));
    assert(mKernelSetsMap);
	ret = mKernelSetsMap->create(mMaxChannels, CL_MEM_USE_PERSISTENT_MEM_AMD, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mKernelSetsMap.get(), graalQueue);

    // kernel sets map
    mKernelLensMap.reset(new CABuf<int>(context));
    assert(mKernelLensMap);
	ret =  mKernelLensMap->create(n_sets_ * mMaxChannels, CL_MEM_USE_PERSISTENT_MEM_AMD, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mKernelLensMap.get(), graalQueue);

    mKernelTransformed.resize(n_sets_);
    mKernelStaging.resize(n_sets_);
    mKernelTransformedStore.resize(n_sets_);
#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
    mKernelInputStore.resize(n_sets_);
#endif

    for( int i = 0; i < n_sets_; i++ )
    {
        mKernelTransformedStore[i].reset(new CASubBuf<float>(*mKernelTrasformedUnion));
        assert(mKernelTransformedStore[i]);
		ret = mKernelTransformedStore[i]->Create(mMaxChannels *aligned_conv_sz_ *i, mMaxChannels *aligned_conv_sz_, 0);
		AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
        initBuffer(mKernelTransformedStore[i].get(), graalQueue);

#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
        mKernelInputStore[i].reset(new CASubBuf<float>(*mKernelInputUnion.get()));
        assert(mKernelInputStore[i]);
		ret = mKernelInputStore[i]->Create((i*mMaxChannels) *mMaxConvolutionSize, mMaxConvolutionSize*mMaxChannels, conv_mem_alloc_flags_);
		AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
        initBuffer(mKernelInputStore[i].get(), graalQueue);
#endif

        PrintAMFArrayReduced("mKernelInputUnion", mKernelInputUnion->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mKernelInputUnion->GetBuffer()->GetSize());

        mKernelStaging[i].resize(mMaxChannels);
        mKernelTransformed[i].resize(mMaxChannels);

        for(int j = 0; j < mMaxChannels; j++)
        {
            mKernelStaging[i][j].reset(new CASubBuf<float>(*mKernelInputUnion.get()));
            assert(mKernelStaging[i][j]);
			ret = mKernelStaging[i][j]->Create(
                (i * mMaxChannels + j) * mMaxConvolutionSize,
                mMaxConvolutionSize,
                conv_mem_alloc_flags_
                );
			AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
            initBuffer(mKernelStaging[i][j].get(), graalQueue);

            mKernelTransformed[i][j].reset(new CASubBuf<float>(*mKernelTrasformedUnion));
            assert(mKernelTransformed[i][j]);
			ret = mKernelTransformed[i][j]->Create((i*mMaxChannels + j) * aligned_conv_sz_, aligned_conv_sz_, 0);
			AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
            initBuffer(mKernelTransformed[i][j].get(), graalQueue);
        }

        PrintAMFArrayReduced("mKernelInputUnion", mKernelInputUnion->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mKernelInputUnion->GetBuffer()->GetSize());
    }

    // get fht lookup tables
    mSinCos.reset(new CABuf<float>(context));
    mBitReverse.reset(new CABuf<short>(context));

    AMF_RETURN_IF_FAILED(mSinCos->CreateSystemMemory(mAlignedProcessingSize));
    AMF_RETURN_IF_FAILED(mBitReverse->CreateSystemMemory(mAlignedProcessingSize));

    FHTInit(
        mSinCos->GetSystemMemory(),
        mBitReverse->GetSystemMemory(),
        (FHT_FUNC *)&mFHTTransformCPUFunction,
        mAlignedProcessingSize
        );

    mSinCos->copyToDevice(graalQueue, 0);
    mBitReverse->copyToDevice(graalQueue, 0);

    std::string kernel_file, kernel_name, comp_options;
    std::vector<amf_uint8> kernelSource;
    size_t kernelSourceSize = 0;

    ////////////////////////////////////
    selectUploadOptions(kernel_file, kernelSource, kernelSourceSize, kernel_name, comp_options);

    {
        auto kernel = GetOclKernel(
            pComputeUpdate,

            kernel_file,
            kernelSource.data(),
            kernelSourceSize,
            kernel_name,
            comp_options

#ifdef TAN_NO_OPENCL
            , mFactory
#endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mUploadKernel = cl_kernel(kernel->GetNative());
        }
#else
        mUploadKernel = kernel;
#endif
        AMF_RETURN_IF_FALSE(mUploadKernel != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

    ////////////////////////////////////
    selectUpload2Options(kernel_file, kernelSource, kernelSourceSize, kernel_name, comp_options);

    {
        auto kernel = GetOclKernel(
            pComputeUpdate,

            kernel_file,
            kernelSource.data(),
            kernelSourceSize,
            kernel_name,
            comp_options

    #ifdef TAN_NO_OPENCL
            , mFactory
    #endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mUploadKernel2 = cl_kernel(kernel->GetNative());
        }
#else
        mUploadKernel2 = kernel;
#endif
        AMF_RETURN_IF_FALSE(mUploadKernel2 != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

    selectResetOptions(kernel_file, kernelSource, kernelSourceSize, kernel_name, comp_options);

    ////////////////////////////////////
    {
        auto kernel = GetOclKernel(
            pComputeConvolution,

            kernel_file,
            kernelSource.data(),
            kernelSourceSize,
            kernel_name,
            comp_options

#ifdef TAN_NO_OPENCL
            , mFactory
#endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mResetKernel = cl_kernel(kernel->GetNative());
        }
#else
        mResetKernel = kernel;
#endif
        AMF_RETURN_IF_FALSE(mResetKernel != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

    ////////////////////////////////////
    {
        auto kernel = GetOclKernel(
            pComputeConvolution,

            "GraalFHT.cl",
            (amf_uint8 *)GraalFHT,
            GraalFHTCount,
            "amdPadFFTBlock",
            comp_options

#ifdef TAN_NO_OPENCL
            , mFactory
#endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mCopyWithPaddingKernel = cl_kernel(kernel->GetNative());
        }
#else
        mCopyWithPaddingKernel = kernel;
#endif
        AMF_RETURN_IF_FALSE(mCopyWithPaddingKernel != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

    // per stream counters
    mRoundCounters.reset(new CABuf<uint>(context));
    assert(mRoundCounters);

	ret = mRoundCounters->create(mMaxChannels * n_sets_, 0, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    mRoundCounters->setValue2(graalQueue, 0);
    mRoundCounters->CreateSystemMemory(mRoundCounters->GetCount());   // allocate backing system memory block.
    initBuffer(mRoundCounters.get(), graalQueue);

    // channels map
    mChannelsMap.reset(new CABuf<int>(context));
    assert(mChannelsMap);
	ret = mChannelsMap->create(n_sets_ * mMaxChannels,  CL_MEM_USE_PERSISTENT_MEM_AMD, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mChannelsMap.get(), graalQueue);

    // sets map
    mSetsMap.reset(new CABuf<int>(context));
    assert(mSetsMap);
	ret = mSetsMap->create(n_sets_ * mMaxChannels,  CL_MEM_USE_PERSISTENT_MEM_AMD, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mSetsMap.get(), graalQueue);

    mSetsMapXf.reset(new CABuf<int>(context));
	assert(mSetsMapXf);
	ret = mSetsMapXf->create(n_sets_ * mMaxChannels, CL_MEM_USE_PERSISTENT_MEM_AMD, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
	initBuffer(mSetsMapXf.get(), graalQueue);

    // process input
    // fill input and keep previous inputs in a proper  places
    mProcessInputStaging = CABuf<float>(context);
	ret = mProcessInputStaging.create(
        aligned_proc_bufffer_sz_ * n_input_blocks_ * mMaxChannels,
        CL_MEM_USE_PERSISTENT_MEM_AMD,
        pComputeConvolution->GetMemoryType()
        );
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(&mProcessInputStaging, graalQueue);
    //PrintAMFArrayReduced("mProcessInputStaging", mProcessInputStaging.GetBuffer(), m_pContextTAN->GetGeneralQueue(), mProcessInputStaging.GetBuffer()->GetSize());

    // history, transformed input data cyclic array
    mHistoryTransformed.reset(new CABuf<float>(context));
    assert(mHistoryTransformed);
	ret = mHistoryTransformed->create(mMaxChannels *aligned_conv_sz_/* n_sets_*/,  0, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mHistoryTransformed.get(), graalQueue);
    //PrintAMFArrayReduced("mHistoryTransformed", mHistoryTransformed->GetBuffer(), m_pContextTAN->GetGeneralQueue(), mHistoryTransformed->GetBuffer()->GetSize());

    AMFTraceInfo(
        AMF_FACILITY,
        L"Graal(1) history storage size = %6.2fMB\n",
        (float)(mHistoryTransformed->GetCount() * sizeof(float)) / 1024 / 1024
        );

    // output data
    // inverse transform output
    mProcess2OutputStaging.reset(new CABuf<float>(context));
    assert(mProcess2OutputStaging);
	ret = mProcess2OutputStaging->create(
        aligned_proc_bufffer_sz_ * mMaxChannels * n_sets_,
        0, //CL_MEM_ALLOC_HOST_PTR, --!
        pComputeConvolution->GetMemoryType()
        );
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mProcess2OutputStaging.get(), graalQueue);

    // accumulator
    accum_stride_ = ((n_aligned_proc_blocks_ + n_accum_blocks_ - 1) / n_accum_blocks_) * mAlignedProcessingSize;

    size_t accum_len = accum_stride_ * mMaxChannels * n_sets_;

    mCmadAccum.reset(new CABuf<float>(context));
    assert(mCmadAccum);
	ret = mCmadAccum->create(accum_len, 0, pComputeConvolution->GetMemoryType());
	AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mCmadAccum.get(), graalQueue);

    mCmadAccumXF.reset(new CABuf<float>(context));
    assert(mCmadAccumXF);
    ret = mCmadAccumXF->create(accum_len, 0, pComputeConvolution->GetMemoryType());
    AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mCmadAccumXF.get(), graalQueue);

    mCopyResponseInOffset.reset(new CABuf<int>(context));
    assert(mCopyResponseInOffset);
    ret = mCopyResponseInOffset->create(mMaxChannels, 0, pComputeConvolution->GetMemoryType());
    AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mCopyResponseInOffset.get(), graalQueue);

    mCopyResponseOutOffset.reset(new CABuf<int>(context));
    assert(mCopyResponseOutOffset);
    ret = mCopyResponseOutOffset->create(mMaxChannels, 0, pComputeConvolution->GetMemoryType());
    AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
    initBuffer(mCopyResponseOutOffset.get(), graalQueue);

    mHostCopyRespInOffset.resize(mMaxChannels);
    mHostCopyRespOutOffset.resize(mMaxChannels);
    mHostInputStaging.resize(n_sets_);

    //input_transformed_.resize(n_sets_);
    //mCmadAccum.resize(n_sets_);
    for(int i = 0; i < n_sets_; i++)
    {
        // queued input in cyclic array
        //AMD_PERSISTENT
        mHostInputStaging[i].reset(new CABuf<float>(context));
        assert(mHostInputStaging[i]);
		ret = mHostInputStaging[i]->create(
            aligned_proc_bufffer_sz_ * mMaxChannels,
            0, //CL_MEM_ALLOC_HOST_PTR, --!
            pComputeConvolution->GetMemoryType()
            );
		AMF_RETURN_IF_FALSE(AMF_OK == ret, ret, L"Failed to create buffer: %d", ret);
        initBuffer(mHostInputStaging[i].get(), graalQueue);
    }

    selectDirectFHTOptions(kernel_file, kernelSource, kernelSourceSize, kernel_name, comp_options);

    ////////////////////////////////////
    {
        auto kernel = GetOclKernel(
            pComputeConvolution,

            kernel_file,
            kernelSource.data(),
            kernelSourceSize,
            kernel_name,
            comp_options

#ifdef TAN_NO_OPENCL
            , mFactory
#endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mDirectTransformKernel = cl_kernel(kernel->GetNative());
        }
#else
        mDirectTransformKernel = kernel;
#endif
        AMF_RETURN_IF_FALSE(mDirectTransformKernel != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

    std::vector<std::string> kernel_names;
    selectFHT_CMADOptions(kernel_file, kernelSource, kernelSourceSize, kernel_names, comp_options);

#ifndef TAN_NO_OPENCL
    mCMADKernels.resize(kernel_names.size());
#else
    mCMADKernels.resize(kernel_names.size());
#endif

    for(int i = 0; i < mCMADKernels.size(); i++)
    {
        ////////////////////////////////////
        {
            auto kernel = GetOclKernel(
                pComputeConvolution,

                kernel_file,
                kernelSource.data(),
                kernelSourceSize,
                kernel_names[i],
                comp_options

#ifdef TAN_NO_OPENCL
                , mFactory
#endif
                );

#ifndef TAN_NO_OPENCL
            if(kernel)
            {
                mCMADKernels[i] = cl_kernel(kernel->GetNative());
            }
#else
            mCMADKernels[i] = kernel;
#endif
            AMF_RETURN_IF_FALSE(mCMADKernels[i] != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
        }
    }

    selectInverseFHTOptions(kernel_file, kernelSource, kernelSourceSize, kernel_name, comp_options);

    ////////////////////////////////////
    {
        auto kernel = GetOclKernel(
            pComputeConvolution,

            kernel_file,
            kernelSource.data(),
            kernelSourceSize,
            kernel_name,
            comp_options

#ifdef TAN_NO_OPENCL
            , mFactory
#endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mInverseTransformKernel = cl_kernel(kernel->GetNative());
        }
#else
        mInverseTransformKernel = kernel;
#endif
        AMF_RETURN_IF_FALSE(mInverseTransformKernel != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

    selectConvHead1Options(kernel_file, kernelSource, kernelSourceSize, kernel_name, comp_options);

    ////////////////////////////////////
    {
        auto kernel = GetOclKernel(
            pComputeConvolution,

            kernel_file,
            kernelSource.data(),
            kernelSourceSize,
            kernel_name,
            comp_options

#ifdef TAN_NO_OPENCL
            , mFactory
#endif
            );

#ifndef TAN_NO_OPENCL
        if(kernel)
        {
            mConvHead1 = cl_kernel(kernel->GetNative());
        }
#else
        mConvHead1 = kernel;
#endif
        AMF_RETURN_IF_FALSE(mConvHead1 != nullptr, AMF_FAIL, L"failed: GetOclKernel %s", kernel_name.c_str());
    }

#ifndef TAN_NO_OPENCL
    clFinish(m_pContextTAN->GetOpenCLConvQueue());
    clFinish(m_pContextTAN->GetOpenCLGeneralQueue());
#else
    m_pContextTAN->GetAMFConvQueue()->FinishQueue();
    m_pContextTAN->GetAMFGeneralQueue()->FinishQueue();
#endif

    return ret;
}

AMF_RESULT CGraalConv::cleanup()
{
    AMF_RESULT ret = AMF_OK;

#ifndef TAN_NO_OPENCL
    if (m_pContextTAN->GetOpenCLConvQueue() )
    {
        clFlush(m_pContextTAN->GetOpenCLConvQueue());
    }

    if (m_pContextTAN->GetOpenCLGeneralQueue() )
    {
        clFlush(m_pContextTAN->GetOpenCLGeneralQueue());
    }
#else
    if (m_pContextTAN->GetAMFConvQueue() )
    {
        m_pContextTAN->GetAMFConvQueue()->FlushQueue();
    }

    if (m_pContextTAN->GetAMFGeneralQueue() )
    {
        m_pContextTAN->GetAMFGeneralQueue()->FlushQueue();
    }
#endif

    mRoundCounters.reset();
    mChannelsMap.reset();
    mSetsMap.reset();
	mSetsMapXf.reset();
    mHistoryTransformed.reset();
    mProcess2OutputStaging.reset();

    for (int i = 0; i < n_sets_; i++)
    {
        mKernelTransformedStore[i].reset();

#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
        mKernelInputStore[i].reset();
#endif

        for (int j = 0; j < mMaxChannels; j++)
        {
            mKernelStaging[i][j].reset();
            mKernelTransformed[i][j].reset();
        }

        mHostInputStaging[i].reset();
    }

#ifndef TAN_NO_OPENCL
    if (m_headTailKernelEvent) {
        clReleaseEvent(m_headTailKernelEvent);
        m_headTailKernelEvent = NULL;
    }
    if (m_pullKernelEvent) {
        clReleaseEvent(m_pullKernelEvent);
        m_pullKernelEvent = NULL;
    }
#endif

    mKernelStaging.clear();
    mKernelTransformed.clear();
    mKernelTransformedStore.clear();
#ifdef COPY_CONTIGUOUS_IRS_IN_ONE_BLOCK
    mKernelInputStore.clear();
#endif
    mHostInputStaging.clear();

    mCmadAccum.reset();
    mCmadAccumXF.reset();

    mKernelChannelsMap.reset();
    mKernelSetsMap.reset();
    mKernelLensMap.reset();

    mKernelInputUnion.reset();
    mKernelTrasformedUnion.reset();

    mSinCos.reset();
    mBitReverse.reset();

    mCopyResponseOutOffset.reset();

    mHostCopyRespInOffset.resize(0);
    mHostCopyRespOutOffset.resize(0);

#ifndef TAN_SDK_EXPORTS
    mUploadKernel.reset();
    mUploadKernel2.reset();

    mResetKernel.reset();

    mDirectTransformKernel.reset();

    for (int i = 0; i < mCMADKernels.size(); i++)
    {
#ifndef TAN_NO_OPENCL
        clReleaseKernel(mCMADKernels[i]);
#else
        mCMADKernels[i] = nullptr;
#endif
    }
    mCMADKernels.clear();

    if (mInverseTransformKernel)
    {
#ifndef TAN_NO_OPENCL
        clReleaseKernel(mInverseTransformKernel);
#else
#endif
        mInverseTransformKernel = nullptr;
    }

    if (mConvHead1)
    {
        clReleaseKernel(mConvHead1);
        mConvHead1 = 0;
    }

    /*if (graalTailQ_)
    {
        clReleaseCommandQueue(graalTailQ_);
    }
    graalTailQ_ = NULL;*/

    if (own_queue_ && graalQueue)
    {
        clReleaseCommandQueue(graalQueue);
    }

    //graalQueue = 0;
    own_queue_ = false;

    ret = graal::getGraalOCL().cleanup();
#endif // !TAN_SDK_EXPORTS

    return ret;
}

}
