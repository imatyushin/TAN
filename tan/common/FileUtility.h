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
std::string getCurrentDirectory();
bool setCurrentDirectory(const std::string& directoryName);
std::string joinPaths(const std::string & left, const std::string & right);

FileVersion getFileVersion(const std::string& filePath);
std::string getFileVersionString(const std::string& filePath);
//void getFileVersionAndDate(std::string &logMessage, char *version, size_t maxLength);

std::string getPath2File(const std::string& fileNameWithPath);
std::string getFileNameWithExtension(const std::string& filePath);
std::string getFileNameWithoutExtension(const std::string& filePath);
std::string getFileExtension(const std::string& fileName);
bool compareIgnoreCase(const std::string& first, const std::string& second);
std::wstring toWideString(const std::string& inputString);
bool checkFileExist(const std::string& fileName);

std::string getModuleFileName();