#include "stdafx.h"
#include "FileUtility.h"
#include "StringUtility.h"

#include <fstream>
#include <iostream>

#define MAX_SIZE_STRING  16384
#define MAX_BLOCK_CHUNK  12000

int main(int argc, char* argv[])
{
	if (argc > 3)
	{
		std::cerr << "Too many parameters" << std::endl;

		return 1;
	}

	if (argc <= 1)
	{
		std::cerr << "Not enough parameters" << std::endl;

		return 1;
	}

	std::string kernelFileFullName = argv[1];
	auto kernelFileExtension = getFileExtension(kernelFileFullName);

	std::wifstream clKernelStream(kernelFileFullName);

	if(clKernelStream.fail())
	{
		std::cerr << "Could not open file " << kernelFileFullName << std::endl;

		return 1;
	}

	auto fileName = getFileNameWithExtension(kernelFileFullName);
	fileName.resize(fileName.length() - kernelFileExtension.length() - 1); //skip extension

	std::string outputFileName(
		argc > 2
		    ? argv[2]
			: (toUpper(kernelFileExtension) + "Kernel_" + fileName + ".h")
		);

	auto path2File(getPath2File(outputFileName));

	if(path2File.length() && !checkDirectoryExist(path2File))
	{
		if(!createPath(path2File))
		{
			std::cerr << "Could not create path " << path2File << std::endl;

			return 1;
		}
	}

	std::wofstream outputStream(outputFileName);

	if(outputStream.fail())
	{
		std::cerr << "Could not open output file " << outputFileName << std::endl;

		return 1;
	}

	std::cerr << "Outputing: " << outputFileName << std::endl;

	std::wstring clKernelSource(
		(std::istreambuf_iterator<wchar_t>(clKernelStream)),
		std::istreambuf_iterator<wchar_t>()
		);

	std::wstring headerdoc =
	    L"#pragma once\n"
		"#include <string>\n"
		"// This file is generated by CLKernelPreprocessor, any changes made in this file will be lost\n"
		;
	outputStream << headerdoc << std::endl;

	// Since MS has a maximum size for string literal, we need to check the file size
	size_t incrementChunk = MAX_BLOCK_CHUNK;

	size_t partitionCount = 0;
	std::wstring concatenatedSource;
	std::wstring fileNameWide = toWideString(fileName);

	for
	(
		size_t blockIndex = 0;
		blockIndex < clKernelSource.length();
		blockIndex += incrementChunk, ++partitionCount
	)
	{
		if((clKernelSource.size() - blockIndex) < MAX_BLOCK_CHUNK)
		{
			incrementChunk = clKernelSource.size() - blockIndex;
		}
		else
		{
			incrementChunk = MAX_BLOCK_CHUNK;
		}

		outputStream
			<< L"const std::string " << fileNameWide << partitionCount
			<< L" = R\"(" << clKernelSource.substr(blockIndex, incrementChunk) << L")\";"
			<< std::endl;

		if(incrementChunk < MAX_BLOCK_CHUNK)
		{
			concatenatedSource += fileNameWide + std::to_wstring(partitionCount);
		}
		else
		{
			concatenatedSource += fileNameWide + std::to_wstring(partitionCount) + L"+";
		}
	}

	outputStream
	    << "const std::string " << fileNameWide << "_Str = " << concatenatedSource << ";" << std::endl
		<< "static const char* " << fileNameWide << " = &" << fileNameWide << "_Str[0u];" << std::endl
		<< "const size_t " << fileNameWide << "Count = " << fileNameWide << "_Str.size();" << std::endl
		;

	outputStream.flush();

	return 0;
}