//
// Copyright (c) 2016 Advanced Micro Devices, Inc. All rights reserved.
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

#include <math.h>

#ifdef OMP_ENABLED
  #include <omp.h>
#endif

int DirectConv(float * out, float * in, int index, int block_sz, float * kernel, int kernel_sz, int n_blocks)
{
    int ret = 0;
    int b, j, c;
    int in_sz = n_blocks * block_sz;
#pragma omp parallel for private(j,c)
    for (b = 0; b < block_sz; b++) {
        out[b] = 0;
        double o = 0;
        // limited by real kernel length
        for (j = index * block_sz + b, c = 0; c < kernel_sz; c++, j--) {
            j = (j < 0) ? in_sz - 1 : j;

            o += in[j] * kernel[c];

        }

        out[b] = (float)o;
    }

    return ret;
}