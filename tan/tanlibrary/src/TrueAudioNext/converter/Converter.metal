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

#define OVERFLOW_WARNING 1

kernel void shortToShort(
	device	short*	inputBuffer,	///< [in]
				long	inputStep,		///< [in]
				long	inputOffset,	///< [in]
	device	short*	outputBuffer,	///< [out]
				long	outputStep,		///< [in]
				long	outputOffset	///< [in]
				,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]

	)
{
	int gid = global_id.x;
	outputBuffer[(gid * outputStep) + outputOffset] = inputBuffer[(gid * inputStep) + inputOffset];
}

kernel void floatToFloat(
	device	float*	inputBuffer,	///< [in]
				long	inputStep,		///< [in]
				long	inputOffset,	///< [in]
	device	float*	outputBuffer,	///< [out]
				long	outputStep,		///< [in]
				long	outputOffset	///< [in]
	)
{
	int gid = global_id.x;
	outputBuffer[(gid * outputStep) + outputOffset] = inputBuffer[(gid * inputStep) + inputOffset];
}

kernel void shortToFloat(
	device	short*	inputBuffer,	///< [in]
				long	inputStep,		///< [in]
				long	inputOffset,	///< [in]
	device	float*	outputBuffer,	///< [out]
				long	outputStep,		///< [in]
				long	outputOffset,	///< [in]
				float	conversionGain	///< [in]
	)
{
	float scale = conversionGain / SHRT_MAX;

	int gid = global_id.x;
	outputBuffer[(gid * outputStep) + outputOffset] = convert_float( inputBuffer[(gid * inputStep) + inputOffset] ) * scale;
}

kernel void floatToShort(
	device	float*	inputBuffer,	///< [in]
				long	inputStep,		///< [in]
				long	inputOffset,	///< [in]
	device	short*	outputBuffer,	///< [out]
				long	outputStep,		///< [in]
				long	outputOffset,	///< [in]
				float	conversionGain,	///< [in]
	device	int*	overflowError	///< [out]
	)
{
	int gid = global_id.x;

	float scale = SHRT_MAX * conversionGain;
	float f = inputBuffer[(gid * inputStep) + inputOffset] * scale;

	if ( f > SHRT_MAX)
	{
		f = SHRT_MAX;
		*overflowError = OVERFLOW_WARNING;
	}

	if ( f < SHRT_MIN)
	{
		f = SHRT_MIN;
		*overflowError = OVERFLOW_WARNING;
	}

	outputBuffer[(gid * outputStep) + outputOffset] = convert_short( f );
}
