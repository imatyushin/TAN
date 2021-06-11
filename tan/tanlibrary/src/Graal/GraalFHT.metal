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

#define _K0_GROUP_SZ 256
#define _K0_LOG2_GROUP_SZ 8
#define _K0_LOG2_N 12
#define _K0_N 4096

#define float              float
#define __FLOAT2__             float2

//It will copy date from the source that is blockLenth long
//to the out, the pad it with padLegnth zeros.  Offsets and
//in/out lengths are to allow for wrap around
kernel void amdPadFFTBlock(
	device const float * 		in,
	device float * 				out,
	device const int * 			inOffset,
	device const int & 			inLength,
	device const int * 			outOffset,
	device const int & 			outLength,
	device const int & 			blockLength,
	device const int & 			padLength,
	device const int & 			channelCount
	,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	int glbl_id = global_id.x;
	int chnl_id = global_id.y;
	if(chnl_id >= channelCount){ return; }
	int blockNumber = (glbl_id) / (blockLength + padLength);
	int inIndex = ((glbl_id + inOffset[chnl_id] - blockNumber * padLength) % inLength);
	int outIndex = ((glbl_id + outOffset[chnl_id]) % outLength);

	bool isPad = (glbl_id % (blockLength + padLength)) >= blockLength;

	out[outIndex] = isPad ? 0 : in[inIndex];
}

void FHTIterationG(
	device float * data,
	constant float * ang,
	int n,
	int n2,
	int k )
{
	float a;
	float b;
	float c;
	float d;
	float e;
	float f;
	int i, j, ang_off;
//	if (k < metal::mad24(n2 , n/2, n2))
	{
			i = (int)( (float)k / (float)(n /2 + 1));
			j =  k - metal::mul24(i , (n/2 + 1));

			int diff = (j==0) ? n : j;

			float flip_sign = (float)((j==0)? -1 : 1);

			ang_off = metal::mul24(j ,(n2 <<1));

			int a_off = metal::mad24((n<<1),i, j);
			a = data[a_off];            // *A
			b = data[a_off + n];        // *B

			int c_off = metal::mad24((n<<1),i, n - diff);
			c=data[c_off];			    // *C
			d=data[c_off + n];          // *D

			float dsin=ang[ang_off];
			float dcos=ang[ang_off + 1];

			e = b*dcos + d*dsin;
			f = b*dsin - d*dcos;

			f *= flip_sign;

			data[a_off + n] = a-e;		// *B
			data[a_off] = a+e;			// *A
			data[c_off + n] = c-f;      // *D
			data[c_off] = c+f;			// *C
	}

//	threadgroup_barrier(metal::mem_flags::mem_none);
}

void FHTIterationG2(device char * data,
	constant char * ang,
	int n,
	int n2,
	int k )
{
	float a;
	float b;
	float c;
	float d;
	float e;
	float f;
	int i, j, ang_off;
//	if (k < metal::mad24(n2 , n/2, n2))
	{

			i = (int)( (float)k / (float)(n /2 + 1));
			j =  k - metal::mul24(i , (n/2 + 1));

			int diff = (j==0) ? n : j;

			float flip_sign = (float)((j==0)? -1 : 1);

			ang_off = metal::mul24(j ,(n2 <<1));

			int a_off = metal::mad24((n<<1),i, j);
			a = *(device float*)&data[(a_off << 2)];            // *A
			b = *(device float*)&data[(a_off + n) << 2];        // *B

			int c_off = metal::mad24((n<<1),i, n - diff);
			c=*(device float*)&data[(c_off) << 2];			    // *C
			d=*(device float*)&data[(c_off + n) << 2];          // *D

			float dsin=*(constant float*)&ang[(ang_off) << 2];
			float dcos=*(constant float*)&ang[(ang_off + 1) << 2];

			e = b*dcos + d*dsin;
			f = b*dsin - d*dcos;

			f *= flip_sign;

			*(device float*)&data[(a_off + n) << 2] = a-e;		// *B
			*(device float*)&data[(a_off) << 2] = a+e;			// *A
			*(device float*)&data[(c_off + n) << 2] = c-f;      // *D
			*(device float*)&data[(c_off) << 2] = c+f;			// *C
	}

//	threadgroup_barrier(metal::mem_flags::mem_none);
}

//#define _GROUP_SZ 256
//#define _N 2048
//#define _LOG2_N 11
#define _N_TO_READ (1 << (_K0_LOG2_N - _K0_LOG2_GROUP_SZ))

/////////////////////////////////////////////////////
//
//
/////////////////////////////////////////////////////

kernel
void amdFHTUploadConv(
	device const char * 					in,
	device float * 							out,
	constant short * 						gbitreverse,
	constant char * 						gsincos,
	device const int & 						kern_ln,
	device const int & 						in_chnl_stride,
	device const int & 						out_chnl_stride,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	int lcl_id = local_id.x;
	int grp_id = group_id.x;
	//int chnl = group_id.y;

 	//char data[_K0_N<<2];
	metal::array<device char, _K0_N<<2 > data;

	for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
	{
		device char * charP = &data[i<<2];
		device float * floatP = (device float *)charP;

		*floatP = 0;
	}

	/*
	threadgroup_barrier(metal::mem_flags::mem_none);

	//read data with bit reverse
	//second half shouled be padded with 0
	for( int i = lcl_id, k = lcl_id + grp_id * _K0_N/2; i < _K0_N/2 && k < kern_ln; i+= _K0_GROUP_SZ, k+= _K0_GROUP_SZ)
	{
		device char * charP = &data[gbitreverse[i]<<2];
		device float * floatP = (device float *)charP;
		device float * inP = (device float *)&in[k<<2];

		*floatP = *inP;
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	int n=1;
	int n2=_K0_N/2;
	for(int log2_n = 0, log2_n2 = _K0_LOG2_N - 1; log2_n < _K0_LOG2_N; log2_n++, log2_n2--)
	{
		n = (1 << log2_n);
		n2 = (1 << log2_n2);

		for ( int k = lcl_id; (k < metal::mad24(n2 , n/2, n2)); k+= _K0_GROUP_SZ)
		{
			FHTIterationG2(&data[0], gsincos, n, n2, k);
		}

		threadgroup_barrier(metal::mem_flags::mem_none);
	}

	for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
	{
		out[i + grp_id * _K0_N] = *(device float*)&data[i<<2];
	}
	*/
}

/////////////////////////////////////////////////////
kernel void amdFHTConvIn(
	device const char * 					in,
	device float * 							out,
	constant short * 						gbitreverse,
	constant char * 						gsincos,
	constant uint * 						channels_map,
	constant uint * 						versions_map,
	constant uint * 						lens_map,
	device uint * 							rount_counters,
	device float * 							history_data,
	device const uint & 					in_version_stride,
	device const uint & 					in_chnl_stride,
	device const uint & 					out_version_stride,
	device const uint & 					out_chnl_stride,
	device const uint & 					version_stride,
	device const uint & 					data_version_stride,
	device const uint & 					data_channel_stride,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	/*
	uint lcl_id = local_id.x;
	//uint glb_id = global_id.x;
	uint grp_id = group_id.x;
	uint chnl = group_id.y;
	uint chnl_id = channels_map[chnl];
	uint upload_id = versions_map[chnl];
	uint kern_ln = lens_map[metal::mad24(version_stride,upload_id, chnl_id)];

	//char data[_K0_N<<2];
	metal::array<device char, _K0_N<<2 > data;

	for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
	{
		device char * charP = &data[i<<2];
		device float * floatP = (device float *)charP;

		*floatP = 0;
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	//read data with bit reverse
	//second half shouled be padded with 0
	uint in_off = upload_id * in_version_stride + chnl_id * in_chnl_stride;

	for
	(
		uint i = lcl_id, k = lcl_id + grp_id * _K0_N/2;
		i < _K0_N/2 && k < kern_ln;
		i+= _K0_GROUP_SZ, k+= _K0_GROUP_SZ
	)
	{
		device char * charP = &data[gbitreverse[i]<<2];
		device float * floatP = (device float *)charP;
		device float * inP = (device float *)&in[(in_off + k)<<2];

		*floatP = *inP;
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	int n=1;
	int n2=_K0_N/2;
	for(int log2_n = 0, log2_n2 = _K0_LOG2_N - 1; log2_n < _K0_LOG2_N; log2_n++, log2_n2--)
	{
		n = (1 << log2_n);
		n2 = (1 << log2_n2);

		for ( int k = lcl_id; (k < metal::mad24(n2 , n/2, n2)); k+= _K0_GROUP_SZ)
		{
			FHTIterationG2(&data[0], gsincos, n, n2, k);
		}

		threadgroup_barrier(metal::mem_flags::mem_none);
	}

	//write out kernel
	uint kern_off = out_version_stride * upload_id + out_chnl_stride * chnl_id;

	for( uint i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
	{
		out[i + grp_id * _K0_N  + kern_off] = *(device float*)&data[i<<2];
	}
	*/
}

kernel
void amdFHTAdvanceTime(
	constant uint * 						channels_map,
	device uint * 							round_counters,
	device const int & 						step,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	uint glbl_id = local_id.x;
	uint chnl_id = channels_map[glbl_id];
	//reset counters
	round_counters[chnl_id] += step;
}

kernel
void amdFHTPushIn(
	device const char * 					in,
	device float * 							out,
	constant short * 						gbitreverse,
	constant char * 						gsincos,
	device const uint & 					n_in_blocks,     // # of blocks kept in input staging
	device const uint & 					in_version_stride,
	device const uint & 					in_chnl_stride,
	device const uint & 					out_version_stride,
	device const uint & 					out_chnl_stride,
	device const uint & 					n_conv_blocks,
	device const uint & 					version_stride,
	constant uint * 						channels_map,
	constant uint * 						versions_map,
	constant uint * 						round_counters,
	device const uint & 					counter2,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	/*
	int lcl_id = local_id.x;
	//int grp_id = group_id.x;
	int chnl_index = group_id.y;

	//char data[(_K0_N) << 2];
	metal::array<device char, _K0_N<<2 > data;

	uint chnl = channels_map[chnl_index];
	//uint version = versions_map[chnl_index];
	uint counter = round_counters[chnl];

	//read data with bit reverse
	uint in_off = (in_chnl_stride * chnl);

	//modulo
	//first half
	uint index0 = counter % n_in_blocks;
	//second half
	uint index1 = (((int)index0 - 1) < 0) ? n_in_blocks - 1 : index0 - 1;

	uint output_index = counter % n_conv_blocks;

	int index[(_N_TO_READ >> 1)];

	for( int i = 0; i < (_N_TO_READ >> 1); i++ )
	{
		index[i] = gbitreverse[lcl_id + (i << _K0_LOG2_GROUP_SZ)];
	}

	for( int i = 0; i < (_N_TO_READ >> 1); i++ )
	{
		//*(device float*)&data[(index[i]<<2)] = *(float*)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index0, (uint)_K0_N/2) + in_off)<<2];
		device char * charP = &data[(index[i]<<2)];
		device float * floatP = (device float *)charP;
		device float * inP = (device float *)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index0, (uint)_K0_N/2) + in_off)<<2];

		*floatP = *inP;
	}

	// previous input block

	for( int i = 0; i < (_N_TO_READ >> 1); i++ )
	{
		index[i] = gbitreverse[lcl_id + (i << _K0_LOG2_GROUP_SZ) + _K0_N/2];
	}

	for( int i = 0; i < (_N_TO_READ >> 1); i++ )
	{
		//*(device float*)&data[(index[i]<<2)] = *(float*)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index1, (uint)_K0_N/2) + in_off)<<2];
		device char * charP = &data[(index[i]<<2)];
		device float * floatP = (device float *)charP;
		device float * inP = (device float *)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index1, (uint)_K0_N/2) + in_off)<<2];

		*floatP = *inP;
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	int n=1;
	int n2=_K0_N/2;

	for(int log2_n = 0, log2_n2 = _K0_LOG2_N - 1; log2_n < _K0_LOG2_N; log2_n++, log2_n2--)
	{
		n = (1 << log2_n);
		n2 = (1 << log2_n2);

		for(int k = lcl_id; (k < metal::mad24(n2 , n/2, n2)); k+= _K0_GROUP_SZ)
		{
			FHTIterationG2(&data[0], gsincos, n, n2, k);
		}

		threadgroup_barrier(metal::mem_flags::mem_none);
	}

	uint out_off = ( out_chnl_stride * chnl) + output_index *_K0_N;

	for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
	{
		out[i + out_off] = *(device float*)&data[i<<2];

		//if ( chnl == 0 )
		//	{
		//		printf("in: %d %f\n", lcl_id, *(device float*)&data[i<<2]);
		//	}
	}
	*/
}

kernel
void amdFHTPushOut(
	device const float * 					in,
	device float * 							out,
	constant short * 						gbitreverse,
	constant float * 						gsincos,
	device const uint & 					in_version_stride,
	device const uint & 					in_chnl_stride,
	device const uint & 					out_version_stride,
	device const uint & 					out_chnl_stride,
	constant float &						scale,
	device const uint & 					counter_version_stride,
	constant uint * 						channels_map,
	constant uint * 						versions_map,
	device uint * 							round_counters,
	device const int & 					 	advance_time,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
	)
{
	/*
	int lcl_id = local_id.x;
	//int grp_id = group_id.x;
	int chnl_index = group_id.y;

	//device float data[_K0_N];
	metal::array<device float, _K0_N > data;

	uint chnl_id = channels_map[chnl_index];  // channel
	uint upload_id = versions_map[chnl_index]; // version

	//read data with bit reverse

	for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
	{
		data[gbitreverse[i]] = in[i + in_chnl_stride * chnl_id + in_version_stride *upload_id];
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	int n=1;
	int n2=_K0_N/2;

	for(int log2_n = 0, log2_n2 = _K0_LOG2_N - 1; log2_n < _K0_LOG2_N; log2_n++, log2_n2--)
	{
		n = (1 << log2_n);
		n2 = (1 << log2_n2);

		for ( int k = lcl_id; k < metal::mad24(n2, n/2, n2); k+= _K0_GROUP_SZ)
		{
			FHTIterationG(&data[0], gsincos, n, n2, k);
		}

		threadgroup_barrier(metal::mem_flags::mem_none);
	}

	for( int i = lcl_id; i < _K0_N/2; i+= _K0_GROUP_SZ)
	{
		out[i + metal::mul24(out_version_stride, upload_id) + metal::mul24(out_chnl_stride, chnl_id)] = data[i] * scale;

		//if ( chnl_id == 0 )
		//{
		//	printf("in: %d %f\n", lcl_id, data[i] * scale);
		//}
	}

	//update counters at the end of the round
	//current counter
	if ( advance_time && lcl_id == 0 )
	{
		uint counter = round_counters[chnl_id];
		round_counters[chnl_id] = counter + 1;
	}
	*/
}

/* DHT convolution
Z[k] =  (X[k] *( Y[k] + Y[N-k]) + X[N-k] * ( Y[k] - Y[N-k] ) ) / 2
Z[N-k] = (X[N-k] * ( Y[k] + Y[N-k]) - X[k] * ( Y[k] - Y[N-k] ) ) / 2
X0 == XN etc
below the division by 2 deffered to the final scaling
*/
void FHTmad(
	device float * z_k,
	device float * z_N_k,
	float x_k,
	float x_N_k,
	float y_k,
	float y_N_k
	)
{
	*z_k = x_k * ( y_k + y_N_k ) + x_N_k * ( y_k - y_N_k);
	*z_N_k = x_N_k * ( y_k + y_N_k ) - x_k * ( y_k - y_N_k);
}

/////////////////////////////////////////////////////
//
// global size is total number of processed paires (N/2 * 2)  -1
/////////////////////////////////////////////////////
kernel
void FHTMultAddHead2(
	device const float * 					IR,
	device const float * 					FHTStore,
	device float * 							Accum,
	device const uint & 					accum_version_stride,
	device const uint & 					accum_chnl_stride,
	device const uint & 					IR_version_stride,
	device const uint & 					chnl_stride,
	device const uint & 					IR_bin_shift,
	device const uint & 					n_bins,        // # bins in the buffer
	device const uint & 					n_loop_bins,
	device const uint & 					counter_version_stride,
	constant uint * 						channels_map,
	constant uint * 						versions_map,
	constant uint * 						round_counters,
	device const int & 					 	STR_bin_shift     // time position, either 0, no shift, -1 previous time

	,

	uint3 				global_id 			[[thread_position_in_grid]],
	uint3 				local_id 			[[thread_position_in_threadgroup]],
	uint3 				group_id 			[[threadgroup_position_in_grid]],
	uint3 				group_size 			[[threads_per_threadgroup]],
	uint3 				grid_size 			[[threads_per_grid]]
	)
{
	/*
	uint k = global_id.x;
	uint N_k = _K0_N - k;
	uint chunk_id = global_id.y;  //  accumulator block we start from
	uint chnl_id = channels_map[global_id.z];  // channel
	uint upload_id = versions_map[global_id.z]; // version
	// current counter
	uint counter = round_counters[chnl_id];

	uint current_bin = (counter + STR_bin_shift) % n_bins;
	uint str_off = chnl_stride * chnl_id;
	uint IR_off = IR_version_stride * upload_id + chnl_stride * chnl_id;
	uint accum_offset = chunk_id << _K0_LOG2_N;
	uint accum_chnl_offset = accum_chnl_stride * chnl_id + accum_version_stride * upload_id;
	uint bin_id = (((int)current_bin - (int)chunk_id) < 0) ? (int)n_bins + ((int)current_bin - (int)chunk_id) : (current_bin - chunk_id);
	uint ir_id = chunk_id + IR_bin_shift;
	uint IR_offset;
	uint store_offset;

	//if ( chnl_id == 0 && k==0 && chunk_id == 0)
	//{
	//	printf("K: %d %d %d %d\n", ir_id, bin_id, n_loop_bins, total_n_bins);
	//}

	float ir_k, ir_N_k;
	float fht_k, fht_N_k;
	float t_accum_k, t_accum_N_k;
	float accum_k = 0, accum_N_k = 0;

	N_k = (k == 0 ) ? _K0_N/2 : N_k;

	for
	(
		uint i = 0;
		i < n_loop_bins && ir_id < n_bins;
		i++, ir_id += grid_size.y, bin_id = (int)bin_id - grid_size.y
	)
	{
		bin_id = ((int)bin_id < 0) ? (int)n_bins + (int)bin_id : bin_id;
		IR_offset = ir_id << _K0_LOG2_N;
		store_offset = bin_id << _K0_LOG2_N;
		uint ir_off = IR_off + IR_offset;
		uint store_off = str_off + store_offset;
		ir_k = IR[(ir_off + k)];
		ir_N_k = IR[(ir_off + N_k)];
		fht_k = FHTStore[(store_off +k)];
		fht_N_k = FHTStore[(store_off + N_k)];

		metal::array<device float, 2> temp1 = {{
			t_accum_k,
			t_accum_N_k}
			};

		FHTmad(&temp1[0], &temp1[1], ir_k, ir_N_k, fht_k, fht_N_k);
		t_accum_k = (k==0) ? ir_k * fht_k * (float)2. : t_accum_k;
		t_accum_N_k = (k==0) ? ir_N_k * fht_N_k  * (float)2. : t_accum_N_k;

		accum_k += t_accum_k;
		accum_N_k += t_accum_N_k;

		//if ( chnl_id == 0 && i== 0 && k < 16)
		//{
		//	printf("in: %d %f %f\n", k, t_accum_k, t_accum_N_k);
		//}
	}

	uint accum_off = (accum_chnl_offset + accum_offset);
	Accum[(accum_off + k)] = accum_k;
	Accum[(accum_off + N_k)] = accum_N_k;
	*/
}

kernel
void FHTMultAddTail(device float * Accum,
					constant uint * versions_map,
					constant int * channels_map,
	device const uint & 					accum_version_stride,
	device const uint & 					accum_chnl_stride,
	device const uint & 					n_loop_bins,
	device const uint & 					total_n_bins
	,

	uint3 				global_id 			[[thread_position_in_grid]],
	uint3 				local_id 			[[thread_position_in_threadgroup]],
	uint3 				group_id 			[[threadgroup_position_in_grid]],
	uint3 				group_size 			[[threads_per_threadgroup]],
	uint3 				grid_size 			[[threads_per_grid]]
	)
{
	/*
	uint k = global_id.x;
	uint chunk_id = global_id.y;
	uint chnl_id = channels_map[global_id.z];
	uint upload_id = versions_map[global_id.z]; // version


	uint channel_off = accum_chnl_stride * chnl_id + accum_version_stride * upload_id;
	uint accum_offset = chunk_id << _K0_LOG2_N;

	float accum = 0;
	for(uint i = 0, c = chunk_id; i < n_loop_bins && c < total_n_bins; i++, c += grid_size.y) {

		uint off = channel_off + (c << _K0_LOG2_N);
		accum += Accum[k + off];
	}

	Accum[channel_off + accum_offset + k] = accum;

	//if ( chnl_id == 0)
	//{
	//	printf("acc out: %d %d %f\n", k, channel_off + accum_offset + k, accum);
	//}
	*/
}

/*-------------------------------------------------------------------------------------------------
Head-Tail algorithms
--------------------------------------------------------------------------------------------------*/
kernel
void amdFHTConvHead1(device const char * in, // pipelone input
				device const float * IR,    // filter
				device const float * Accum, // acuumulator for the tail
				device float * Hist,       // direct transform data history
				device float * out,    // pipeline output
				constant short * gbitreverse,
				constant char * gsincos,
	device const uint & 					n_in_blocks,     // # of blocks kept in input staging
	device const uint & 					n_conv_blocks,  // # of conv blocks (total)
	device const float & 					scale,         // inverse conv scale
	device const int & 						prev_input,    // use previous input
	device const int & 						advance_time,  // advance time counter
	device const uint & 					in_version_stride,
	device const uint & 					in_chnl_stride,
	device const uint & 					hist_version_stride,
	device const uint & 					hist_chnl_stride,
	device const uint & 					IR_version_stride,
	device const uint & 					IR_chnl_stride,
	device const uint & 					accum_version_stride,
	device const uint & 					accum_chnl_stride,
	device const uint & 					counter_version_stride,
	device const uint & 					out_version_stride,
	device const uint & 					out_chnl_stride,
				constant uint * channels_map,
				constant uint * versions_map,
				device uint * round_counters
	,

	uint2 				global_id 			[[thread_position_in_grid]],
	uint2 				local_id 			[[thread_position_in_threadgroup]],
	uint2 				group_id 			[[threadgroup_position_in_grid]],
	uint2 				group_size 			[[threads_per_threadgroup]],
	uint2 				grid_size 			[[threads_per_grid]]
				)
{
	/*
	int lcl_id = local_id.x;
	int grp_id = group_id.x;
	int chnl_index = global_id.y;

	//device constant char data[(_K0_N) << 2];
	metal::array<device char, _K0_N<<2 > data;

	uint chnl = channels_map[chnl_index];
	uint version = versions_map[chnl_index];
	uint counter = round_counters[chnl];
	// position in the history buffer
	uint output_index = counter % n_conv_blocks;
	uint hist_off = ( hist_chnl_stride * chnl) + output_index *_K0_N;

	//-----------------------------------------------------------------------------------------
	//direct transform
	//-----------------------------------------------------------------------------------------
	if ( !prev_input)
	{
		// read data with bit reverse
		// first half
		uint in_off = (in_chnl_stride * chnl);
		// modulo
		uint index0 = counter % n_in_blocks;
		uint index1 = (((int)index0 - 1) < 0) ? n_in_blocks - 1 : index0 - 1;

		int index[(_N_TO_READ >> 1)];

		for( int i = 0; i < (_N_TO_READ >> 1); i++ )
		{
			index[i] = gbitreverse[lcl_id + (i << _K0_LOG2_GROUP_SZ)];
		}

		for( int i = 0; i < (_N_TO_READ >> 1); i++ )
		{
			//*(device float*)&data[(index[i]<<2)] = *(float*)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index0, (uint)_K0_N/2) + in_off)<<2];

			device char * charP = &data[(index[i]<<2)];
			device float * floatP = (device float *)charP;
			device float * inP = (device float *)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index0, (uint)_K0_N/2) + in_off)<<2];

			*floatP = *inP;

			//if ( chnl == 0 && lcl_id == 0 && i < 8)
			//{
				//printf("in0: %d %f\n", i, *(device float*)&data[(index[i]<<2)]);
			//}
		}

		// previous input block

		for( int i = 0; i < (_N_TO_READ >> 1); i++ )
		{
			index[i] = gbitreverse[lcl_id + (i << _K0_LOG2_GROUP_SZ) + _K0_N/2];
		}

		for( int i = 0; i < (_N_TO_READ >> 1); i++ )
		{
			//*(device float*)&data[(index[i]<<2)] = *(float*)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index1, (uint)_K0_N/2) + in_off)<<2];
			device char * charP = &data[(index[i]<<2)];
			device float * floatP = (device float *)charP;
			device float * inP = (device float *)&in[(lcl_id + (i << _K0_LOG2_GROUP_SZ) + metal::mul24(index1, (uint)_K0_N/2) + in_off)<<2];

			*floatP = *inP;

			//if ( chnl == 0  && lcl_id == 0 && i < 8)
			//{
			//	printf("in1: %d %f\n", i, *(device float*)&data[(index[i]<<2)]);
			//}
		}

		threadgroup_barrier(metal::mem_flags::mem_none);

		//forward FHT
		int n=1;
		int n2=_K0_N/2;
		for(int log2_n = 0, log2_n2 = _K0_LOG2_N - 1; log2_n < _K0_LOG2_N; log2_n++, log2_n2--)
		{
			n = (1 << log2_n);
			n2 = (1 << log2_n2);

			for( int k = lcl_id; (k < metal::mad24(n2 , n/2, n2)); k+= _K0_GROUP_SZ)
			{
				FHTIterationG2(&data[0], gsincos,	n, n2, k );
			}

			threadgroup_barrier(metal::mem_flags::mem_none);
		}

		// write to the history buffer
		// this's going to be used in the tail part of the algorithm

		for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
		{
			Hist[i + hist_off] = *(device float*)&data[i<<2];

			//if ( chnl == 0 && i < 8)
			//{
			//	printf("in: %d %f\n", lcl_id, *(device float*)&data[i<<2]);
			//}
		}

	}  // !_prev_input
	else
	{
		// read previous input from the history buffer
		for( int i = lcl_id; i < _K0_N; i+= _K0_GROUP_SZ)
		{
			*(device float*)&data[i<<2] = Hist[i + hist_off];
		}

		threadgroup_barrier(metal::mem_flags::mem_none);
	}

	//------------------------------------------------------------------------------------------
 	//CMAD
 	//Add tail
	//------------------------------------------------------------------------------------------
	uint accum_offset = accum_chnl_stride * chnl + accum_version_stride * version;
	// 0- block
	uint IR_off = IR_version_stride * version + IR_chnl_stride * chnl;

	float2 cmad[(_N_TO_READ>>1)];

	for( uint i = 0, k = lcl_id; i < (_N_TO_READ>>1); i++, k+= _K0_GROUP_SZ)
	{
		float ir_k, ir_N_k;
		float fht_k, fht_N_k;
		float t_k, t_N_k;
		float tail_k, tail_N_k;

		uint N_k = _K0_N - k;
		N_k = (k == 0 ) ? _K0_N/2 : N_k;
		ir_k = IR[(IR_off + k)];
		ir_N_k = IR[(IR_off + N_k)];
		fht_k = *(device float*)&data[k<<2];
		fht_N_k = *(device float*)&data[N_k<<2];
		tail_k = Accum[accum_offset + k];
		tail_N_k = Accum[accum_offset + N_k];

		metal::array<device float, 2> temp1 = {{
			t_k,
			t_N_k}
			};

		FHTmad(&temp1[0], &temp1[1], ir_k, ir_N_k, fht_k, fht_N_k);
		cmad[i].x = (k==0) ? ir_k * fht_k * (float)2. : t_k;
		cmad[i].y = (k==0) ? ir_N_k * fht_N_k  * (float)2. : t_N_k;
		cmad[i].x += tail_k;
		cmad[i].y += tail_N_k;

		//if ( chnl == 0 && k < 8 )
		//{
			//printf("acc in: %d %f %f %f  %d %f %f %f\n", k, ir_k, fht_k, tail_k, N_k, ir_N_k, fht_N_k, tail_N_k);
		//}
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	//-------------------------------------------------------------------------------------------
 	//bit reverse
 	//inverse transform
	//-------------------------------------------------------------------------------------------
	for( uint i = 0, k = lcl_id; i < (_N_TO_READ >> 1); i++, k+= _K0_GROUP_SZ)
	{
		uint N_k = _K0_N - k;
		N_k = (k == 0 ) ? _K0_N/2 : N_k;
		*(device float*)&data[(gbitreverse[k]) <<2] = cmad[i].x;
		*(device float*)&data[(gbitreverse[N_k]) << 2] = cmad[i].y;
	}

	threadgroup_barrier(metal::mem_flags::mem_none);

	{
		int n=1;
		int n2=_K0_N/2;
		for(int log2_n = 0, log2_n2 = _K0_LOG2_N - 1; log2_n < _K0_LOG2_N; log2_n++, log2_n2--)
		{
			n = (1 << log2_n);
			n2 = (1 << log2_n2);

			for ( int k = lcl_id;k < metal::mad24(n2 , n/2, n2); k+= _K0_GROUP_SZ)
			{
				FHTIterationG2(&data[0], gsincos,	n, n2, k );
			}

			threadgroup_barrier(metal::mem_flags::mem_none);
		}
	}

	//write out into pipeline output buffer
	for( int i = lcl_id; i < _K0_N/2; i+= _K0_GROUP_SZ)
	{
		out[i + metal::mul24(out_version_stride, version) + metal::mul24(out_chnl_stride, chnl)] = *(device float*)&data[i<<2] * scale;

		//if ( chnl == 0 )
		//{
		//	printf("in: %d %f\n", lcl_id, *(device float*)&data[i<<2] * scale);
		//}
	}

	// update counters at the end of the round
	// current counter

	if ( advance_time && lcl_id == 0 )
	{
		uint counter = round_counters[chnl];
		round_counters[chnl] = counter + 1;
	}*/
}