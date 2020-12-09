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
#include <metal_stdlib>
using namespace metal;

#define Lx		4
#define Ly		4
#define Lz		4

#define SoundSpeed  340.0f
#define Float2Int   67108864.0f

kernel
    //__attribute__((reqd_work_group_size(Lx, Ly, Lz)))
    void GenerateRoomResponse(
		volatile device atomic_int *	response,			///< [out]
		//device int *	response,			///< [out]


		constant float *  				hpF,				//__constant
		constant float *  				lpF, 				// can combine the two filters into one buffers

		//constant int &				globalSizeZ,

		//constant float *            	inputFloats,
		device const float & srcX,
		device const float & srcY,
		device const float & srcZ,
		device const float & headX,
		device const float & headY,
		device const float & headZ,
		device const float & earVX,
		device const float & earVY,
		device const float & earVZ,
		device const float & earV,
		device const float & roomWidth,
		device const float & roomLength,
		device const float & roomHeight,
		device const float & dampRight,
		device const float & dampLeft,
		device const float & dampFront,
		device const float & dampBack,
		device const float & dampTop,
		device const float & dampBottom,
		device const float & maxGain,
		device const float & dMin,

		//constant int *					inputInts,
		device const int & inSampRate,
		device const int & responseLength,
		device const int & hrtfResponseLength,
		device const int & headFilterLength,
		device const int & numRefX,
		device const int & numRefY,
		device const int & numRefZ,

		uint3 							global_id 			[[thread_position_in_grid]],
		uint3 							local_id 			[[thread_position_in_threadgroup]],
		uint3 							group_id 			[[threadgroup_position_in_grid]],
		uint3 							group_size 			[[threads_per_threadgroup]],
		uint3 							grid_size 			[[threads_per_grid]]
    )
{
	int x = global_id.x; 				// reflections along left/right directions
	int y = global_id.y; 				// reflections along top/bottom directions
	int z = global_id.z;				// get_global_id(2); // reflections along front/back directions

	if ( x > numRefX )
		return;

	if ( y > numRefY )
		return;

	if ( z > numRefZ )
		return;

	// x axis
	int indexX = x - (numRefX)/2;
	float a = (indexX % 2 == 0) ? indexX : indexX + 1;
	float b = (indexX % 2 == 0) ? 1.0f : -1.0f;
	float refXPos = a * roomWidth + b * srcX;

	// y axis
	int indexY = y - (numRefY)/2;
	a = (indexY % 2 == 0) ? indexY : indexY + 1;
	b = (indexY % 2 == 0) ? 1.0f : -1.0f;
	float refYPos = a * roomHeight + b * srcY;

	// z axis
	int indexZ = z - (numRefZ)/2;
	a = (indexZ % 2 == 0) ? indexZ : indexZ + 1;
	b = (indexZ % 2 == 0) ? 1.0f : -1.0f;
	float refZPos = a * roomLength + b * srcZ;

	float attenuationXLeft   = (indexX > 0 ) ? indexX/2 : abs(indexX - 1)/2;
	float attenuationYBottom = (indexY > 0 ) ? indexY/2 : abs(indexY - 1)/2;
	float attenuationZBack   = (indexZ > 0 ) ? indexZ/2 : abs(indexZ - 1)/2;

	float attenuationXRight   = (indexX > 0) ? (indexX + 1)/2 : abs(indexX)/2;
	float attenuationYTop     = (indexY > 0) ? (indexY + 1)/2 : abs(indexY)/2;
	float attenuationZFront   = (indexZ > 0) ? (indexZ + 1)/2 : abs(indexZ)/2;


	float amplitude = powr(dampRight,  attenuationXRight)  *
	                  powr(dampLeft,   attenuationXLeft)   *
	                  powr(dampTop,    attenuationYTop)    *
					  powr(dampBottom, attenuationYBottom) *
					  powr(dampFront,  attenuationZFront)  *
					  powr(dampBack,   attenuationZBack);

	float dx  =  refXPos - headX;

	float dy = refYPos - headY;

	float dz =  refZPos - headZ;

	float d = sqrt(dx*dx + dy*dy + dz*dz);


	int filterIndex = 1 + (int)((d/SoundSpeed) * inSampRate);

	if ( filterIndex < responseLength )
	{
		float dr = ( d <= dMin ) ? maxGain : maxGain*dMin / d;
		int length = responseLength - filterIndex;
		int len = headFilterLength > length ? length : headFilterLength;

	    float dp = earVX*dx + earVY*dy + earVZ*dz;
	    float cosA = dp / (earV*d);
	    float hf = (cosA + 1.0f) / 2.0f;

		if ( filterIndex < hrtfResponseLength)
		{
			for ( int i = 0; i < len; i++)
			{
				float inValue = Float2Int*amplitude*dr*(lpF[i] + hf * hpF[i]);
				int out = inValue;

				atomic_fetch_add_explicit(&response[i + filterIndex], out, memory_order_relaxed);
			}
		}
		else
		{
			float inValue = Float2Int*amplitude*dr;
			int out = inValue;

			atomic_fetch_add_explicit(&response[filterIndex], out, memory_order_relaxed);
		}
	}

	/*int iii = 0;
	atomic_fetch_add_explicit(&response[0], 1, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], y, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], z, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], indexX, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], indexY, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], indexZ, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], refZPos, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], attenuationXLeft, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], attenuationXRight, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], attenuationYBottom, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], attenuationYTop, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], attenuationZBack, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], attenuationZFront, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], amplitude, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], dx, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], dy, memory_order_relaxed);
	atomic_fetch_add_explicit(&response[iii++], dz, memory_order_relaxed);*/
}