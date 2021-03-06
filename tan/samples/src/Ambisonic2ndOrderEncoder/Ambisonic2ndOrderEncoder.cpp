//
// Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
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


// Ambisonic2ndOrderEncoder.cpp : Defines the entry point for the console application.
//
#include "wav.h"

#include <stdio.h>
#include <memory.h>
#include <math.h>

int main(int argc, char* argv[])
{
    if (argc < 5) {
        printf("Syntax:\n");
        printf("%s outfile.wav s1.wav theta1, phi1, [s2.wav theta2 phi2 ...] ", argv[0]);
        return -1;
    }

    int nFiles = (argc - 2) / 3;
    float *theta = new float[nFiles];
    float *phi = new float[nFiles];

    printf("nFiles = %d\n", nFiles);
    printf("outfile=%s\n", argv[1]);
    for (int i = 2; i < argc; i += 3){
        int idx = 1 + (i - 2) / 3;
        sscanf(argv[i + 1], "%f", &theta[idx - 1]);
        sscanf(argv[i + 2], "%f", &phi[idx - 1]);
        printf("source%d: %s theta%d: %s phi%d: %s\n", idx, argv[i], idx, argv[i + 1], idx, argv[i + 2]);
    }

    for (int n = 0; n < nFiles; n++){
        theta[n] *= 3.1415926535 / 180.0;
        phi[n] *= 3.1415926535 / 180.0;
    }

    float **pfSamples = new float *[nFiles];
    float **tSamples;

    int SamplesPerSec, BitsPerSample, NChannels;
    uint32_t *NSamples = new uint32_t[nFiles];
    unsigned char *pSsamples;
    long maxLen = 0;

    for (int i = 0; i < nFiles; i++){
        uint16_t tBitsPerSample(0), tNChannels(0);
        uint32_t tSamplesPerSec(0);

        if (ReadWaveFile(argv[2 + i * 3], tSamplesPerSec, tBitsPerSample, tNChannels, NSamples[i], &pSsamples, &tSamples)) {
            if (i == 0){
                SamplesPerSec = tSamplesPerSec;
                BitsPerSample = tBitsPerSample;
                NChannels = tNChannels;
            }
            maxLen = maxLen < NSamples[i] ? NSamples[i] : maxLen;

            if (tSamplesPerSec != SamplesPerSec){
                printf("Error %s rate %d != %s rate %d\n", argv[2 + i * 3], tSamplesPerSec, argv[2], SamplesPerSec);
                return (0);
            }
            if (tBitsPerSample != BitsPerSample){
                printf("Error %s bits %d != %s bits %d\n", argv[2 + i * 3], tBitsPerSample, argv[2], BitsPerSample);
                return (0);
            }
            if (tNChannels != NChannels){
                printf("Error %s channels %d != %s channels %d\n", argv[2 + i * 3], tNChannels, argv[2], NChannels);
                return (0);
            }
            delete pSsamples;

            //if more than mono, mix all channels down:
            if (tNChannels > 1){
                for (int ch = 1; ch < tNChannels; ch++){
                    for (int n = 0; n < NSamples[i]; n++){
                        tSamples[0][n] += tSamples[ch][n];
                    }
                    delete tSamples[ch];
                }
            }
            pfSamples[i] = tSamples[0];
        }
    }

    // 2nd order channels:
    float * R = new float[maxLen];
    float * S = new float[maxLen];
    float * T = new float[maxLen];
    float * U = new float[maxLen];
    float * V = new float[maxLen];
    // 1st order channels:
    float * W = new float[maxLen];
    float * X = new float[maxLen];
    float * Y = new float[maxLen];
    float * Z = new float[maxLen];

    memset(R, 0, maxLen*sizeof(float));
    memset(S, 0, maxLen*sizeof(float));
    memset(T, 0, maxLen*sizeof(float));
    memset(U, 0, maxLen*sizeof(float));
    memset(V, 0, maxLen*sizeof(float));
    memset(W, 0, maxLen*sizeof(float));
    memset(X, 0, maxLen*sizeof(float));
    memset(Y, 0, maxLen*sizeof(float));
    memset(Z, 0, maxLen*sizeof(float));

    for (int n = 0; n < nFiles; n++){
        for (int i = 0; i < maxLen && i < NSamples[n]; i++){
            R[i] += 0.5*(3 * sin(phi[n])*sin(phi[n]) - 1.0)* pfSamples[n][i];
            S[i] += cos(theta[n]) * sin(2 * phi[n])* pfSamples[n][i];
            T[i] += sin(theta[n]) * sin(2 * phi[n])* pfSamples[n][i];
            U[i] += cos(2 * theta[n])*cos(phi[n])*cos(phi[n])* pfSamples[n][i];
            V[i] += sin(2 * theta[n])*cos(phi[n])*cos(phi[n])* pfSamples[n][i];
            W[i] += (1.0 / sqrt(2.0)) * pfSamples[n][i];
            X[i] += cos(theta[n])*cos(phi[n]) * pfSamples[n][i];
            Y[i] += sin(theta[n])*cos(phi[n]) * pfSamples[n][i];
            Z[i] += sin(phi[n]) * pfSamples[n][i];
        }
    }

    float *ambisonicOut[9];
    ambisonicOut[0] = R;
    ambisonicOut[1] = S;
    ambisonicOut[2] = T;
    ambisonicOut[3] = U;
    ambisonicOut[4] = V;
    ambisonicOut[5] = W;
    ambisonicOut[6] = X;
    ambisonicOut[7] = Y;
    ambisonicOut[8] = Z;

    WriteWaveFileF(argv[1], SamplesPerSec, 9, BitsPerSample, maxLen, ambisonicOut);

    return 0;
}

