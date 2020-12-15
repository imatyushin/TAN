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

#define Lx		4
#define Ly		4
#define Lz		4

#define SoundSpeed  340.0f
#define Float2Int   67108864.0f

__kernel
    __attribute__((reqd_work_group_size(Lx, Ly, Lz)))
    void GenerateRoomResponse(
    volatile __global int*  response,		   ///< [out]
	__global float*  hpF,//__constant
	__global float*  lpF, // can combine the two filters into one buffers

	float srcX,
	float srcY,
	float srcZ,
	float headX,
	float headY,
	float headZ,
	float earVX,
	float earVY,
	float earVZ,
	float earV,
	float roomWidth,
	float roomLength,
	float roomHeight,
	float dampRight,
	float dampLeft,
	float dampFront,
	float dampBack,
	float dampTop,
	float dampBottom,
	float maxGain,
	float dMin,
    int inSampRate,
	int responseLength,
	int hrtfResponseLength,
	int headFilterLength,
	int numRefX,
	int numRefY,
	int numRefZ
    )
{
	int x = get_global_id(0); // reflections along left/right directions
	int y = get_global_id(1); // reflections along top/bottom directions
	int z = get_global_id(2); // reflections along front/back directions

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


	float amplitude = native_powr(dampRight,  attenuationXRight)  *
	                  native_powr(dampLeft,   attenuationXLeft)   *
	                  native_powr(dampTop,    attenuationYTop)    *
					  native_powr(dampBottom, attenuationYBottom) *
					  native_powr(dampFront,  attenuationZFront)  *
					  native_powr(dampBack,   attenuationZBack);

	float dx  =  refXPos - headX;

	float dy = refYPos - headY;

	float dz =  refZPos - headZ;

	float d = native_sqrt(dx*dx + dy*dy + dz*dz);


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
				int out = convert_int(Float2Int*amplitude*dr*(lpF[i] + hf * hpF[i]));
				atomic_add(&response[ i + filterIndex], out);

				/*atomic_add(&response[0], 1);
				atomic_add(&response[2], convert_int(out));
				atomic_add(&response[3], convert_int(out));
				atomic_add(&response[4], convert_int(amplitude));
				atomic_add(&response[5], convert_int(dr));
				atomic_add(&response[6], convert_int(lpF[i]));
				atomic_add(&response[7], convert_int(hf));
				atomic_add(&response[8], convert_int(hpF[i]));*/

			}
		}
		else
		{
			int out = convert_int(Float2Int*amplitude*dr);
			atomic_add(&response[ filterIndex], out);

			//atomic_add(&response[1], 1);
		}
	}

	/** /
	//to test
	int test = 0;
	atomic_add(&response[test++], inSampRate);
	atomic_add(&response[test++], responseLength);
	atomic_add(&response[test++], hrtfResponseLength);
	atomic_add(&response[test++], headFilterLength);
	atomic_add(&response[test++], numRefX);
	atomic_add(&response[test++], numRefY);
	atomic_add(&response[test++], numRefZ);
	/**/
}