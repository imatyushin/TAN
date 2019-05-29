#pragma once

#include <string>

#ifdef _WIN32

#include <Windows.h>

#else

#ifndef MAX_PATH
#define MAX_PATH 254
#endif

#endif

struct FileVersion
{
	int m_MajorVersion = 0;
	int m_MinorVersion = 0;
	int m_BuildVersion = 0;
	int m_RevisionVersion = 0;
};

std::string getDirectorySeparator();

FileVersion getFileVersion(const std::string& filePath);
std::string getFileVersionString(const std::string& filePath);
std::string getFileNameWithExtension(const std::string& filePath);
std::string getFileNameWithoutExtension(const std::string& filePath);
std::string getFileExtension(const std::string& fileName);
bool compareIgnoreCase(const std::string& first, const std::string& second);
std::wstring toWideString(const std::string& inputString);
std::string getCurrentDirectory();
bool checkFileExist(const std::string& fileName);

std::string getModulePath();
bool setCurrentDirectory(const std::string& directoryName);