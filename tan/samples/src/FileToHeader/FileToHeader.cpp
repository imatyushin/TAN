#include "stdafx.h"
#include <string>
#include <cstring>
#include <fstream>
#include <vector>
#include <sstream>
#include <iostream>
#include <clocale>
#include <cctype>
#include <ios>
#include <iomanip>

#define MAX_SIZE_STRING  16384
#define MAX_BLOCK_CHUNK  12000

std::string getFileName(const std::string & path)
{
   auto lastSeparatorPosition = path.rfind(
#ifdef _WIN32
	   '\\'
#else
	   '/'
#endif
	   ,
	   path.length()
	   );

   if(std::string::npos != lastSeparatorPosition)
   {
      return path.substr(
		  lastSeparatorPosition + 1,
		  std::string::npos
		  );
   }

   return path;
}

bool compareIgnoreCase(const std::string & first, const std::string & second)
{
	if(first.size() != second.size())
	{
        return false;
	}

	for(auto firstChar = first.begin(), secondChar = second.begin(); firstChar != first.end(); ++firstChar, ++secondChar)
	{
		if(std::tolower(*firstChar) != std::tolower(*secondChar))
		{
			return false;
		}
	}

    return true;
}

std::wstring toWideString(const std::string & inputString)
{
	std::mbstate_t state = std::mbstate_t();

	const char *input(inputString.c_str());

    std::size_t length = std::mbsrtowcs(nullptr, &input, 0, &state) + 1;
    std::vector<wchar_t> buffer(length);
    std::mbsrtowcs(buffer.data(), &input, buffer.size(), &state);

	return buffer.data();
}

#include <typeinfo>

int main(int argc, char* argv[])
{
	if (argc > 3)
	{
		std::cout << "Too many parameters" << std::endl;

		return 1;
	}

	if (argc < 3)
	{
		std::cout << "Not enough parameters" << std::endl;

		return 1;
	}

	std::string kernelFileFullName = argv[1];
	auto kernelFileExtension = kernelFileFullName.length() > 3
	    ? kernelFileFullName.substr(kernelFileFullName.length() - 3)
		: "";

	if(!compareIgnoreCase(kernelFileExtension, ".cl"))
	{
		std::cout << "File is not a .cl file" << std::endl;

		return 1;
	}

    std::basic_ifstream</*unsigned*/ char, std::char_traits</*unsigned*/ char> > clKernelStream(
        kernelFileFullName,
        std::ios::in | std::ios::binary
        );

	if(clKernelStream.fail())
	{
		std::cout << "Could not open .cl file" << std::endl;

		return 1;
	}

	auto fileName = getFileName(kernelFileFullName);
	fileName.resize(fileName.length() - 3); //skip extension

	std::string outputName = argv[2];
	auto wideOutputName = toWideString(outputName);

	std::string outputFileName(
		outputName + ".cl.h"
		);
	std::wofstream outputStream(outputFileName);

	if(outputStream.fail())
	{
		std::cout << "Could not open output file " << outputFileName << std::endl;

		return 1;
	}

	std::cout << "Outputing: " << outputFileName << std::endl;

	// copies all data into buffer, not optimal implementation (dynamic resizes will happened)
	// unsigned char version does not work under linux/gcc, todo: why? seems like a gcc/stl bug
    std::vector</*unsigned*/ char> clKernelSource(
		(std::istreambuf_iterator</*unsigned*/ char, std::char_traits</*unsigned*/ char>>(clKernelStream)),
		std::istreambuf_iterator</*unsigned*/ char, std::char_traits</*unsigned*/ char>>()
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
			outputStream << L"0x" << std::setw(2) << std::setfill(L'0') << std::hex << int(*charIterator) << L", ";
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