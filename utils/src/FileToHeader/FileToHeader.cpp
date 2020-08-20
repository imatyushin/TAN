#include "stdafx.h"
#include "StringUtility.h"
#include "FileUtility.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

#if defined(_WIN32)
#include <direct.h>
#endif

#define MAX_SIZE_STRING  16384
#define MAX_BLOCK_CHUNK  12000

int main(int argc, char* argv[])
{
	if (argc > 3)
	{
		std::cerr << "Too many parameters" << std::endl;

		return 1;
	}

	if (argc < 3)
	{
		std::cerr << "Not enough parameters" << std::endl;

		return 1;
	}

	std::string kernelFileFullName = argv[1];
	auto kernelFileExtension = getFileExtension(kernelFileFullName);

	//if(!compareIgnoreCase(kernelFileExtension, "cl"))
	//{
		//std::cerr << "File is not a .cl file [" << kernelFileExtension << "]" << std::endl;

		//return 1;
	//}

    std::basic_ifstream<char, std::char_traits<char> > clKernelStream(
        kernelFileFullName,
        std::ios::in | std::ios::binary
        );

	if(clKernelStream.fail())
	{
		std::cerr << "Could not open file " << kernelFileFullName << std::endl;

		return 1;
	}

	auto fileName = getFileNameWithExtension(kernelFileFullName);
	fileName.resize(fileName.length() - kernelFileExtension.length() - 1); //skip extension

	std::string outputName = argv[2];
	auto wideOutputName = toWideString(outputName);

	std::string outputFileName(
		outputName + "." + kernelFileExtension + ".h"
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
		//try to create directory
		//auto separated = getFile outputFileName

		std::cerr << "Could not open output file " << outputFileName << "." << std::endl;
		std::cerr << "Verify that output directory exists." << std::endl;

		return 1;
	}

	std::cout << "Outputing: " << outputFileName << std::endl;

	// copies all data into buffer, not optimal implementation (dynamic resizes will happened)
	std::vector<char> clKernelSource(
		(std::istreambuf_iterator<char, std::char_traits<char>>(clKernelStream)),
		std::istreambuf_iterator<char, std::char_traits<char>>()
		);

	outputStream
	    << L"#pragma once" << std::endl
		<< std::endl
		<< L"const amf_uint8 " << wideOutputName << L"[] =" << std::endl
		<< L"{";

    unsigned counter(0);

	for(auto charIterator = clKernelSource.begin(); charIterator != clKernelSource.end(); )
	{
		outputStream << std::endl << L"    ";

		for(auto column(0); (column < 8) && (charIterator != clKernelSource.end()); ++column, ++charIterator, ++counter)
		{
			outputStream
			  //<< L"0x" << std::setw(2) << std::setfill(L'0') << std::hex
			  << int(*charIterator)
			  << L", ";
		}
	}

	outputStream << std::dec;

	outputStream
	    << std::endl
		<< L"};" << std::endl
		<< std::endl
		<< L"const amf_size " << wideOutputName << L"Count = " << counter/*clKernelSource.length()*/ << L";" << std::endl
		<< std::endl;

	outputStream.flush();

	return 0;
}