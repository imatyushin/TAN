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

double calculateCorrelation(const std::vector<uint8_t>& firstData, const std::vector<uint8_t>& secondData)
{
    double correlation(0);
    size_t data_size(firstData.size());

    if (data_size > 1 && data_size == secondData.size())
    {
        double sumX = 0;
        double sumY = 0;
        double sumXmultY = 0;
        double sumSquareX = 0;
        double sumSquareY = 0;

        for (int idx = 0; idx < data_size; idx++)
        {
            uint8_t elementInFirstData = firstData.at(idx);
            uint8_t elementInSecondData = secondData.at(idx);

            sumX += elementInFirstData;
            sumY += elementInSecondData;
            sumXmultY += elementInFirstData * elementInSecondData;
            sumSquareX += elementInFirstData * elementInFirstData;
            sumSquareY += elementInSecondData * elementInSecondData;
        }

        correlation = abs((data_size * sumXmultY - sumX * sumY)
            / std::sqrt((data_size * sumSquareX - sumX * sumX) * (data_size * sumSquareY - sumY * sumY)));
    }
    else
    {
        std::cerr << "TANWavCompare error: incorrect array size!" << std::endl;
    }
    return correlation * 100;
}

bool readWavFile(WavContent *content, const std::string &fileName)
{
    // todo find out mBufferSizeInBytes
    uint32_t mBufferSizeInSamples = 0;
    uint32_t mBufferSizeInBytes = 0;
    bool result(true);

    if (content->ReadWaveFile(fileName))
    {
        if (content->SamplesPerSecond != 48000) //FILTER_SAMPLE_RATE
        {
            std::cerr
                << "TANWavCompare error: file " << fileName << " has an unsupported frequency! Currently only "
                << std::to_string(48000) << " frequency is supported!" << std::endl;

            result = false;
        }

        bool converted = content->Convert2Stereo16Bit();

        if (!converted && content->BitsPerSample != 16)
        {
            std::cerr
                << "TANWavCompare error: file " << fileName << " has an unsupported bits per sample count. Currently only "
                << std::to_string(16) << " bits is supported!" << std::endl;

            result = false;
        }
        else if (!converted && content->ChannelsCount != STEREO_CHANNELS_COUNT)
        {
            std::cerr
                << "TANWavCompare error: file " << fileName
                << " is not a stereo file. Currently only stereo files are supported!" << std::endl;

            result = false;
        }

        if (content->SamplesCount < mBufferSizeInSamples)
        {
            std::cerr
                << "TANWavCompare error: file "
                << fileName << " are too short (SamplesCount < single buffer size)." << std::endl;

            result = false;
        }

        //make mono
        content->JoinChannels();
    }
    else
    {
        std::cerr << "TANWavCompare error: could not load WAV data from file " << fileName << std::endl;

        result = false;
    }

    return result;
}

int main(int argc, char* argv[])
{
    int result = ArgumentsError;

    if (argc > 4)
    {
        std::cerr << "TANWavCompare error: too many parameters" << std::endl;
        result = ArgumentsError;
    }
    else if (argc < 4)
    {
        std::cerr << "TANWavCompare error: not enough parameters" << std::endl;
        result = ArgumentsError;
    }
    else
    {
        std::string firstWavFilePath(argv[1]);
        std::string secondWavFilePath(argv[2]);
        uint32_t correlationPassRate(0);

        try
        {
            correlationPassRate = std::stoul(argv[3]);
        }
        catch (const std::exception& e)
        {
            std::cerr << "TANWavCompare error: invalid parameter - passRate, " << e.what() << std::endl;
            result = ArgumentsError;
        }

        WavContent firstFile;
        WavContent secondFile;
        if (!readWavFile(&firstFile, firstWavFilePath))
        {
            std::cerr << "TANWavCompare error: invalid parameter - firstFile!" << std::endl;
            result = FirstFileError;
        }
        if (!readWavFile(&secondFile, secondWavFilePath))
        {
            std::cerr << "TANWavCompare error: invalid parameter - secondFile!" << std::endl;
            result = SecondFileError;
        }
    
        if (firstFile.IsSameFormat(secondFile))
        {
            double correlation(calculateCorrelation(firstFile.Data, secondFile.Data));

            std::cout << "TANWavCompare result: correlation = " << correlation << "%" << std::endl;
            (correlation < correlationPassRate) ? result = CorrelationLow : result = CorrelationPass;
        }
        else
        {
            std::cerr
                << "Error: file " << secondWavFilePath.c_str()
                << " has a different format with firstFile." << std::endl;
        }
    }

    std::cout << "TANWavCompare result = " << result << std::endl;
    return result;
}
