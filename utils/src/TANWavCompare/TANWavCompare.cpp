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
#include "TANWavCompare.h"

double calculateCorrelation(const std::vector<uint8_t> &firstData, const std::vector<uint8_t>& secondData)
{

    double correlation(0);
    size_t data_size = firstData.size();

    if ((data_size > 1) && (data_size == secondData.size()))
    {
        // Spearman correlation coefficient. x - one element in firstData, y in secondData
        std::vector<uint8_t> multXY;
        std::vector<uint8_t> squareX;
        std::vector<uint8_t> squareY;

        double sumMultXY(0);
        double sumSquareX(0);
        double sumSquareY(0);
        double sumX = std::accumulate(firstData.begin(), firstData.end(), 0);
        double sumY = std::accumulate(secondData.begin(), secondData.end(), 0);


        std::transform(firstData.begin(), firstData.end(),
            secondData.begin(),
            std::back_inserter(multXY),
            [](int i, int j) {return i * j; });

        std::transform(firstData.begin(), firstData.end(),
            std::back_inserter(squareX),
            [](int i) {return pow(i, 2); });

        std::transform(secondData.begin(), secondData.end(),
            std::back_inserter(squareY),
            [](int i) {return pow(i, 2); });

        sumMultXY = std::accumulate(multXY.begin(), multXY.end(), 0);

        sumSquareX = std::accumulate(squareX.begin(), squareX.end(), 0);

        sumSquareY = std::accumulate(squareY.begin(), squareY.end(), 0);

        correlation = (data_size * sumMultXY - sumX * sumY)
            / (std::abs(std::pow(data_size * sumSquareX - std::pow(sumX, 2) * (data_size * sumSquareY - std::pow(sumY, 2)), 0.5)));
    }
    
   return correlation;
}

int readWavFile(WavContent *content, const std::string &fileName)
{
    // todo find out mBufferSizeInBytes
    uint32_t mBufferSizeInSamples = 0;
    uint32_t mBufferSizeInBytes = 0;

    if (content->ReadWaveFile(fileName))
    {
        if (content->SamplesPerSecond != 48000) //FILTER_SAMPLE_RATE
        {
            std::cerr
                << "TANWavCompare error: file " << fileName << " has an unsupported frequency! Currently only "
                << std::to_string(48000) << " frequency is supported!" << std::endl;

            return -1;// AMF_FAIL;
        }

        bool converted = content->Convert2Stereo16Bit();

        if (!converted && content->BitsPerSample != 16)
        {
            std::cerr
                << "TANWavCompare error: file " << fileName << " has an unsupported bits per sample count. Currently only "
                << std::to_string(16) << " bits is supported!" << std::endl;

            return -1;// AMF_FAIL;
        }
        else if (!converted && content->ChannelsCount != STEREO_CHANNELS_COUNT)
        {
            std::cerr
                << "TANWavCompare error: file " << fileName
                << " is not a stereo file. Currently only stereo files are supported!" << std::endl;

            return -1;// AMF_FAIL;
        }

        if (content->SamplesCount < mBufferSizeInSamples)
        {
            std::cerr
                << "TANWavCompare error: file "
                << fileName << " are too short (SamplesCount < single buffer size)." << std::endl;

            return -1;// AMF_FAIL;
        }

        //make mono
        content->JoinChannels();
    }
    else
    {
        std::cerr << "TANWavCompare error: could not load WAV data from file " << fileName << std::endl;

        return -1;// AMF_FAIL;
    }
}



int main(int argc, char* argv[])
{
    if (argc > 4)
    {
        std::cerr << "TANWavCompare error: too many parameters" << std::endl;
        return EXIT_FAILURE;
    }

    if (argc < 4)
    {
        std::cerr << "TANWavCompare error: not enough parameters" << std::endl;
        return EXIT_FAILURE;
    }

    //param 1 - First wav filename
    std::string firstWavFilePath(argv[1]);
    //param 2 - Second wav filename
    std::string secondWavFilePath(argv[2]);
    //param 3 - max correlation percent
    uint32_t passRate(0);

    try
    {
        passRate = std::stoul(argv[3]);
    }
    catch (const std::exception& e)
    {
        std::cerr << "TANWavCompare error: invalid parameter - passRate!" << std::endl;
        return EXIT_FAILURE;
    }

    WavContent firstFile;
    WavContent secondFile;
    if (!readWavFile(&firstFile, firstWavFilePath))
    {
        std::cerr << "TANWavCompare error: invalid parameter - firstFile!" << std::endl;
        return EXIT_FAILURE;
    }
    if (!readWavFile(&secondFile, secondWavFilePath))
    {
        std::cerr << "TANWavCompare error: invalid parameter - secondFile!" << std::endl;
        return EXIT_FAILURE;
    }
    
    if (!firstFile.IsSameFormat(secondFile))
    {
        std::cerr
            << "Error: file " << secondWavFilePath.c_str()
            << " has a different format with firstFile." << std::endl;

    }


    std::cout
        << "TANWavCompare result: correlation = "
        << calculateCorrelation(firstFile.Data, secondFile.Data) << std::endl;

    return EXIT_SUCCESS;
}