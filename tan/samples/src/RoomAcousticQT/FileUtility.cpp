#include "FileUtility.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

#include <cstring>

FileVersion getFileVersion(const std::string& filepath)
{
	FileVersion ret;
	std::memset(&ret, 0, sizeof(FileVersion));
	
	//todo: port it
	/*DWORD verHandle = NULL;
	DWORD versioninfosize = GetFileVersionInfoSize(filepath.c_str(), &verHandle);
	char* verData = new char[versioninfosize];
	if(GetFileVersionInfo(filepath.c_str(),verHandle,versioninfosize,verData))
	{
		LPVOID lpbuffer = NULL;
		UINT size = 0;
		if (VerQueryValue(verData, "\\", &lpbuffer, &size))
		{
			if(size)
			{
				VS_FIXEDFILEINFO* verInfo = static_cast<VS_FIXEDFILEINFO*>(lpbuffer);
				if(verInfo->dwSignature == 0xfeef04bd)
				{
					ret.m_MajorVersion = (verInfo->dwFileVersionMS >> 16) & 0xffff;
					ret.m_MinorVersion = (verInfo->dwFileVersionMS >> 0) & 0xffff;
					ret.m_BuildVersion = (verInfo->dwFileVersionLS >> 16) & 0xffff;
					ret.m_RevisionVersion = (verInfo->dwFileVersionLS >> 0) & 0xffff;
				}
			}
		}
	}*/
	return ret;
}

std::string getFileVersionString(const std::string& filepath)
{
	FileVersion ret = getFileVersion(filepath);
	return std::to_string(ret.m_MajorVersion) + '.' +
		std::to_string(ret.m_MinorVersion) + '.' +
		std::to_string(ret.m_BuildVersion) + '.' +
		std::to_string(ret.m_RevisionVersion);
}

std::string getFileNameWithExtension(const std::string& filepath)
{
	char sep = '/';
#ifdef _WIN32
	sep = '\\';
#endif
	size_t i = filepath.rfind(sep, filepath.length());
	if (i != std::string::npos) {
		return(filepath.substr(i + 1, filepath.length() - i));
	}
	return("");
}

std::string getFileNameWithoutExtension(const std::string& filepath)
{
	char sep = '/';
	char dot = '.';
#ifdef _WIN32
	sep = '\\';
#endif
	size_t seppos = filepath.rfind(sep, filepath.length());
	size_t dotpos = filepath.rfind(dot, filepath.length());
	if (dotpos > seppos && (dotpos != std::string::npos))
	{
		return(filepath.substr((seppos + 1), dotpos-seppos-1));
	}
}

std::string getCurrentDirectory()
{
#ifdef _WIN32
    char currentDirectory[MAX_PATH] = { 0 };
    GetCurrentDirectory(MAX_PATH, currentDirectory);
#else
	//todo: implement more correct way
	char currentDirectory[256] = {0};
	getcwd(currentDirectory, 256);
#endif
	
	return currentDirectory;
}
