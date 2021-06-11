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

//#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL EXTENSION cl_amd_printf : enable
#pragma OPENCL EXTENSION cl_amd_fp64 : enable

#define float              float
#define __FLOAT2__             float2
//#define __TYPE__               float


kernel
void amdSetValue(device  __TYPE__ * Buffer,
              __TYPE__ Value,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
			)
{
    uint id = global_id.x;
	Buffer[id] = Value;

/*
	if ( id == 0 )
	{
		printf("Sz=%d, v=%d\n", grid_size.x, Buffer[id]);
	}
*/
}

kernel
void CopyIRKernel(const device float * IR_In,
                  device __FLOAT2__ *      IR_Out,
				  uint Block_log2,
				  uint IR_In_sz,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]])
{
    uint id = global_id.x;
	uint id_rem = id - ((id >> Block_log2) << Block_log2);
	uint id_out = ((id >> Block_log2) << (Block_log2+1)) + id_rem;
    __FLOAT2__ IR = (__FLOAT2__)(0,0);
	IR_Out[id_out + (1<<Block_log2)] = IR;
	IR.s0 = (id < IR_In_sz) ? IR_In[id] : 0;
	IR_Out[id_out] = IR;

//	printf("%d %d %f %f\n", id, id_out, IR_Out[id_out].s0, IR_Out[id_out + (1<<Block_log2)].s0);
}

kernel
void CopyIRKernel16bitInter(const device uint * IR_In,
                  device __FLOAT2__ *  IR_Out,
				   uint Block_log2,
				   uint IR_In_sz,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]])
{
    uint id = global_id.x;
	uint glbl_sz = grid_size.x;
	uint id_rem = id - ((id >> Block_log2) << Block_log2);
	uint id_out = ((id >> Block_log2) << (Block_log2+1)) + id_rem;
    __FLOAT2__ IR_L = (__FLOAT2__)(0,0);
	__FLOAT2__ IR_R = (__FLOAT2__)(0,0);

	IR_Out[id_out + (1<<Block_log2)] = IR_L;
	IR_Out[id_out + (1<<Block_log2) + (glbl_sz<<1)] = IR_L;

	uint IR = (id < IR_In_sz) ? IR_In[id] : 0;

	int i_L = (((int)(IR & 0xffff) << 16) >> 16);
	int i_R = ((int)IR  >> 16);
	IR_L.s0 = (float)i_L;
	IR_R.s0 = (float)i_R;
	IR_Out[id_out] = IR_L;
// double each
	IR_Out[id_out + (glbl_sz<<1)] = IR_R;
/*
	if ( id < 64 ) {
	   printf("GPU in: %d %d   %f %f\n", id, id_out + (glbl_sz <<1), IR_L.s0, IR_R.s0);
	}
*/

}

kernel
void CopyIRKernel16bitInterSum(const device uint * IR_In,
//                               device __FLOAT2__ * IR_Out,
							   device __FLOAT2__ * sum_out,
				              uint Block_log2,
				              uint IR_In_sz,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]])
{
    uint id = global_id.x;
	uint glbl_sz = grid_size.x;
	uint groupID = group_id.x;
	uint lcl_id = local_id.x;
	uint id_rem = id - ((id >> Block_log2) << Block_log2);
	uint id_out = ((id >> Block_log2) << (Block_log2+1)) + id_rem;

	device __FLOAT2__ sum_buffer[256];

	sum_buffer[lcl_id] = (__FLOAT2__)(0,0);

	threadgroup_barrier(metal::mem_flags::mem_none);

    __FLOAT2__ IR_L = (__FLOAT2__)(0,0);
	__FLOAT2__ IR_R = (__FLOAT2__)(0,0);

//	IR_Out[id_out + (1<<Block_log2)] = IR_L;
//	IR_Out[id_out + (1<<Block_log2) + (glbl_sz<<1)] = IR_L;

	uint IR = (id < IR_In_sz) ? IR_In[id] : 0;

	int i_L = (((int)(IR & 0xffff) << 16) >> 16);
	int i_R = ((int)IR  >> 16);
	IR_L.s0 = (float)i_L ;
	IR_R.s0 = (float)i_R ;
// prefix sum
    __FLOAT2__ my_sum = (__FLOAT2__)(IR_L.s0, IR_R.s0);

	sum_buffer[lcl_id] = my_sum;
	threadgroup_barrier(metal::mem_flags::mem_none);
//
    if ( lcl_id < 128 ) {
         my_sum += sum_buffer[lcl_id + 128];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 64 ) {
         my_sum += sum_buffer[lcl_id + 64];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 32 ) {
         my_sum += sum_buffer[lcl_id + 32];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 16 ) {
         my_sum += sum_buffer[lcl_id + 16];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 8 ) {
         my_sum += sum_buffer[lcl_id + 8];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 4 ) {
         my_sum += sum_buffer[lcl_id + 4];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 2 ) {
         my_sum += sum_buffer[lcl_id + 2];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id == 0 ) {
		my_sum += sum_buffer[1];
		sum_out[groupID] = my_sum;
	}

//	IR_Out[id_out] = IR_L;
// double each
//	IR_Out[id_out + (glbl_sz<<1)] = IR_R;
#if 0
	if ( groupID == 193 /*&& id < 256*/ ) {
	   printf("GPU in: %d %d  %d  %f %f\n", lcl_id, id, id_out + (glbl_sz <<1), IR_L.s0, IR_R.s0);
	}
#endif
#if 0
	if ( /*(groupID & 1) == 0 &&*/lcl_id == 0) {
	   printf("GPU sum: %d %f %f\n", groupID, sum_out[groupID].s0, sum_out[groupID].x);
	}
#endif

}

kernel
void ItermPrefixSum(device __FLOAT2__ * sum_out, uint loop_limit, uint total_limit,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]])
{
    uint id = global_id.x;
	uint glbl_sz = grid_size.x;
	uint groupID = group_id.x;
	uint lcl_id = local_id.x;

	device __FLOAT2__ sum_buffer[256];
// prefix sum
    __FLOAT2__ my_sum = (__FLOAT2__)(0, 0);
// all previous sums
	for (int i = id, k = 0; i < total_limit && k < loop_limit; i += 256, k++ ) {
	     my_sum += sum_out[i];
	}

	sum_buffer[lcl_id] = my_sum;

	threadgroup_barrier(metal::mem_flags::mem_none);
//
    if ( lcl_id < 128 ) {
         my_sum += sum_buffer[lcl_id + 128];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 64 ) {
         my_sum += sum_buffer[lcl_id + 64];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 32 ) {
         my_sum += sum_buffer[lcl_id + 32];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 16 ) {
         my_sum += sum_buffer[lcl_id + 16];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 8 ) {
         my_sum += sum_buffer[lcl_id + 8];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 4 ) {
         my_sum += sum_buffer[lcl_id + 4];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id < 2 ) {
         my_sum += sum_buffer[lcl_id + 2];
		 sum_buffer[lcl_id] = my_sum;
	}
	threadgroup_barrier(metal::mem_flags::mem_none);
    if ( lcl_id == 0 ) {
		my_sum += sum_buffer[1];
		sum_out[groupID] = my_sum;;
	}

}

//////////////////////////////////////////////////
// the left shift by 1 due to a complex part that has to be generated , with 0 value

kernel
void KernelNormalizer(const device uint * IR_In,
                    device __FLOAT2__ * IR_Out,
					  const device __FLOAT2__ * sum_out,
				      uint Block_log2,
				      uint IR_In_sz,
					  uint IR_Mem_sz,
					  uint IR_Mem_offset,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
                    )
{
    uint id = global_id.x;
	uint glbl_sz = grid_size.x;
	uint groupID = group_id.x;
	uint lcl_id = local_id.x;
	uint id_rem = id - ((id >> Block_log2) << Block_log2);
	uint id_out = ((id >> Block_log2) << (Block_log2+1)) + id_rem + (IR_Mem_offset << (1 + Block_log2));


	__FLOAT2__ normalizer = sum_out[0];

    __FLOAT2__ IR_L = (__FLOAT2__)(0,0);
	__FLOAT2__ IR_R = (__FLOAT2__)(0,0);

	IR_Out[id_out + (1<<Block_log2)] = IR_L;
	IR_Out[id_out + (1<<Block_log2) + (IR_Mem_sz<<1)] = IR_L;

	uint IR = (id < IR_In_sz) ? IR_In[id] : 0;

	int i_L = (((int)(IR & 0xffff) << 16) >> 16);
	int i_R = ((int)IR  >> 16);
// normalize
	IR_L.s0 = (normalizer.s0 == 0 )? 0 : (float)i_L / normalizer.s0;
	IR_R.s0 = (normalizer.x == 0 )? 0 : (float)i_R / normalizer.x;

// write out
	IR_Out[id_out] = IR_L;
// double each
	IR_Out[id_out + (IR_Mem_sz<<1)] = IR_R;
/*
	if ( id == 0 )
	{
	    printf("GPU norm: %f %f %f %f\n", normalizer.s0, normalizer.x, IR_L.s0, IR_R.s0);
	}
*/

}




