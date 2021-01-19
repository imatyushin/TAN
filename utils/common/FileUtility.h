#pragma once

#include <string>
#include <vector>

#ifdef _WIN32
  #include <Windows.h>
#else
  #include <errno.h>
  #include <stdio.h>

  #ifndef MAX_PATH
    #define MAX_PATH 254
  #endif

  #if !defined(__APPLE__) && !defined(__MACOSX)
	#ifndef errno_t
	  #define errno_t char
	#endif
  #endif

  #define sscanf_s sscanf
  #define sprintf_s sprintf
#endif

struct FileVersion
{
	int m_MajorVersion = 0;
	int m_MinorVersion = 0;
	int m_BuildVersion = 0;
	int m_RevisionVersion = 0;
};

std::vector<std::string> getDirectorySeparatorVariants();
size_t getDefaultSeparatorIndex();
size_t getLastSeparatorPosition(const std::string & path, std::string & foundSeparator);

std::string getCurrentDirectory();
bool setCurrentDirectory(const std::string& directoryName);
std::string joinPaths(const std::string & left, const std::string & right);

FileVersion getFileVersion(const std::string& filePath);
std::string getFileVersionString(const std::string& filePath);
//void getFileVersionAndDate(std::string &logMessage, char *version, size_t maxLength);

std::string getPath2File(const std::string& fileNameWithPath);
bool createPath(const std::string & path);
std::string getFileNameWithExtension(const std::string& filePath);
std::string getFileNameWithoutExtension(const std::string& filePath);
std::string getFileExtension(const std::string& fileName);

bool checkFileExist(const std::string& fileName);
bool checkDirectoryExist(const std::string& path);

bool compareIgnoreCase(const std::string& first, const std::string& second);

std::string getModuleFileName();

#ifndef _WIN32
  errno_t fopen_s(FILE **f, const char *name, const char *mode);
#endif