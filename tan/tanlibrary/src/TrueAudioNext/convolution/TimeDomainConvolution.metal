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

// Max LDS size: 64KB = 65536 Byte = 16384 float element
// Max convolution length for LDS float to work: 16384 - Lx (local size)

kernel
void TimeDomainConvolution(
device    float*  histBuf,	   ///< [in]
int         convLength,            ///< [in] convolution length
int         bufPos,                ///< [in]
int         dataLength,            ///< [in] Size of the buffer processed per kernel run
int         firstNonZero,          ///< [in]
int         lastNonZero,           ///< [in]
device    float*  filter,        ///< [in]
device    float*  pOutput        ///< [out]
)
{
    int grpid = get_group_id(0);
    int thrdid = get_local_id(0);
    int grptot = get_num_groups(0);
    int grpsz = group_size.x;

    int srcOffset = grpid*grpsz;
    int endOffset = srcOffset + grpsz;

    if (endOffset > dataLength)
        endOffset = dataLength;

    //if (global_id.x > inputSize)
     //   return;

    //int ldsSize = FilterLength + LocalSize;
    //__local float localBuffer[FilterLength + LocalSize];

    //int readPerWorkItem = ldsSize / LocalSize;

    //for (int j = 0; j < datalength; j++){
    for (int j = srcOffset; j < endOffset; j++){
            pOutput[j] = 0.0;
        for (int k = firstNonZero; k < lastNonZero; k++){
            pOutput[j] += histBuf[(bufPos + j - k + convLength) % convLength] * filter[k];
        }
    }
}