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

#define Lx		256
#define Float2Int   67108864.0f

kernel
    //__attribute__((reqd_work_group_size(Lx, 1, 1)))
    void Fill(
        device uint4*  intResponse,		   ///< [in,out]
	    device float4* floatResponse,       ///< [out ]

        uint2 							global_id 			[[thread_position_in_grid]],
		uint2 							local_id 			[[thread_position_in_threadgroup]],
		uint2 							group_id 			[[threadgroup_position_in_grid]],
		uint2 							group_size 			[[threads_per_threadgroup]],
		uint2 							grid_size 			[[threads_per_grid]]
    )
{
	int x = global_id.x;

	int in0 = intResponse[x][0];
	int in1 = intResponse[x][1];
	int in2 = intResponse[x][2];
	int in3 = intResponse[x][3];

	float out0 = float(in0) / Float2Int;
	float out1 = float(in1) / Float2Int;
	float out2 = float(in2) / Float2Int;
	float out3 = float(in3) / Float2Int;

	//floatResponse[x] = as_type<float4>(intResponse[x])/Float2Int;
	floatResponse[x][0] = out0;
	floatResponse[x][1] = out1;
	floatResponse[x][2] = out2;
	floatResponse[x][3] = out3;

	intResponse[x] = 0;
}