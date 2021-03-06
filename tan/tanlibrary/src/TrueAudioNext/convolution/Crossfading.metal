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

kernel void crossfade(
	device const float*	fadeBuffer,			///< [in]
	device float*		outputBuffer,   	///< [in/out]
	constant int32_t &	channelStride,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	int sampleId = global_id.x;
	int numSamples = grid_size.x;
	int chId = global_id.y;

	int sampleOffset = sampleId + chId * channelStride;
	outputBuffer[sampleOffset] = (outputBuffer[sampleOffset] * sampleId + fadeBuffer[sampleOffset] * (numSamples - sampleId)) / (float)numSamples;
}