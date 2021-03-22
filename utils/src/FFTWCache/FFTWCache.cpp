#include "stdafx.h"
#include "StringUtility.h"
#include "FileUtility.h"

#include "api/fftw3.h"

#include <iostream>

void cacheFFTWplans(uint32_t maxCachePower, const std::string & path)
{
	int minL2N = 4;
	int maxL2N = maxCachePower;

	float * ppBufferInput = new float[(1 << maxL2N)]; // _aligned_malloc(sizeof(float) * (1 << maxL2N), 32);
	float * ppBufferOutput = new float[(1 << maxL2N)];  // _aligned_malloc(sizeof(float) * (1 << maxL2N), 32);

	std::vector<fftwf_plan> fwdPlans(maxCachePower, nullptr);
	std::vector<fftwf_plan> bwdPlans(maxCachePower, nullptr);
	std::vector<fftwf_plan> fwdRealPlans(maxCachePower, nullptr);
	std::vector<fftwf_plan> bwdRealPlans(maxCachePower, nullptr);
	std::vector<fftwf_plan> fwdRealPlanarPlans(maxCachePower, nullptr);
	std::vector<fftwf_plan> bwdRealPlanarPlans(maxCachePower, nullptr);

	for (int log2len = minL2N; log2len < maxL2N; log2len++)
	{
		std::cout << "FFTWCache: generate for power " << log2len << "..." << std::endl;

		uint32_t fftLength = 1 << log2len;

		fftwf_complex * in = (fftwf_complex *)ppBufferInput;
		fftwf_complex * out = (fftwf_complex *)ppBufferOutput;

		fftw_iodim iod;
		iod.n = fftLength;
		iod.is = 1;
		iod.os = 1;

		fwdRealPlans[log2len] = fftwf_plan_dft_r2c_1d(fftLength, (float *)in, (fftwf_complex *)out, FFTW_MEASURE);//correct flag ??
		bwdRealPlans[log2len] = fftwf_plan_dft_c2r_1d(fftLength, (fftwf_complex *)in, (float *)out, FFTW_MEASURE);
		fwdRealPlanarPlans[log2len] = fftwf_plan_guru_split_dft_r2c(1, &iod, 0, NULL, (float *)in, (float *)(out), (float *)(out)+(8 + fftLength / 2), FFTW_MEASURE);//correct flag ??
		bwdRealPlanarPlans[log2len] = fftwf_plan_guru_split_dft_c2r(1, &iod, 0, NULL, (float *)in, (float *)(in)+(8 + fftLength / 2), (float *)out, FFTW_MEASURE);

		fwdPlans[log2len] = fftwf_plan_dft_1d(fftLength, in, out, FFTW_FORWARD, FFTW_MEASURE);
		bwdPlans[log2len] = fftwf_plan_dft_1d(fftLength, in, out, FFTW_BACKWARD, FFTW_MEASURE);
	}
	fftwf_export_wisdom_to_filename(path.c_str());

	delete(ppBufferInput); // _aligned_free(ppBufferInput);
	delete(ppBufferOutput); // _aligned_free(ppBufferOutput);
}

int main(int argc, char* argv[])
{
	//return;

	if(argc > 4)
	{
		std::cerr << "FFTWCache error: too many parameters" << std::endl;

		return 1;
	}

	if(argc < 4)
	{
		std::cerr << "FFTWCache error: not enough parameters" << std::endl;

		return 1;
	}

	//param 1 - filename to save
	std::string cacheFileName(argv[1]);
	//param 2 - max fft power
	uint32_t fftwPower(0);
	//param 3 - force creation
	bool forceCreation(false);

	try
	{
		fftwPower = std::stoul(argv[2]);
		forceCreation = std::stoi(argv[3]) ? true : false;
	}
	catch(const std::exception& e)
	{
		std::cerr << "Error: invalid params set for FFTWCache utility!" << std::endl;

		return -1;
	}

	auto cacheFileNameWithPath(getTempFolderName());
	cacheFileNameWithPath = joinPaths(cacheFileNameWithPath, "AMD");
	cacheFileNameWithPath = joinPaths(cacheFileNameWithPath, "TAN");

	if(!checkDirectoryExist(cacheFileNameWithPath))
	{
		createPath(cacheFileNameWithPath);
	}

	cacheFileNameWithPath = joinPaths(cacheFileNameWithPath, cacheFileName/*"FFTW_TAN_WISDOM.cache"*/);

	std::cout << "FFTWCache: " << cacheFileNameWithPath << std::endl;

	if(forceCreation || !checkFileExist(cacheFileNameWithPath))
	{
		cacheFFTWplans(fftwPower, cacheFileNameWithPath);

		std::cout << "FFTWCache: create cache file " << cacheFileNameWithPath << std::endl;
	}
	else
	{
		std::cout << "FFTWCache: cache file " << cacheFileNameWithPath << " already exists" << std::endl;
	}

	return 0;
}
