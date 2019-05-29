#include "FileUtility.h"

#include <string>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

std::string getDirectorySeparator()
{
	return
#ifdef _WIN32
	   "\\"
#else
	   "/"
#endif
		;
}

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
	auto lastSeparatorPosition = filepath.rfind(
		getDirectorySeparator(),
		filepath.length()
	   	);

   if(std::string::npos != lastSeparatorPosition)
   {
      return filepath.substr(
		  lastSeparatorPosition + 1,
		  std::string::npos
		  );
   }

   return filepath;
}

std::string getFileNameWithoutExtension(const std::string& filepath)
{
	size_t seppos = filepath.rfind(getDirectorySeparator(), filepath.length());
	size_t dotpos = filepath.rfind('.', filepath.length());

	if (dotpos > seppos && (dotpos != std::string::npos))
	{
		return(filepath.substr((seppos + 1), dotpos-seppos-1));
	}
}

std::string getFileExtension(const std::string & fileName)
{
    auto lastDot = fileName.rfind('.');

    if(std::string::npos != lastDot)
    {
        return fileName.substr(lastDot);
    }

    return "";
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

bool checkFileExist(const std::string& filename)
{
	if (FILE *file = fopen(filename.c_str(), "r"))
	{
		fclose(file);
		return true;
	}
	else
	{
		return false;
	}
}

/*bool isDirExist(const std::string& path)
{
#if defined(_WIN32)
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & _S_IFDIR) != 0;
#else
    struct stat info;
    if (stat(path.c_str(), &info) != 0)
    {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
#endif
}*/

/*
bool makePath(const std::string& path)
{
	int createError =

#if defined(_WIN32)
		_mkdir(path.c_str())
#else
		mkdir(path.c_str(), 0755)
#endif
		;

    if(!createError)
	{
        return true;
	}

    switch(errno)
    {
    case ENOENT:
        {
            int pos = path.find_last_of('/');
            if (pos == std::string::npos)
#if defined(_WIN32)
                pos = path.find_last_of('\\');
            if (pos == std::string::npos)
#endif
                return false;
            if (!makePath( path.substr(0, pos) ))
                return false;
        }

#if defined(_WIN32)
        return 0 == _mkdir(path.c_str());
#else
        return 0 == mkdir(path.c_str(), mode);
#endif

    case EEXIST:
        return true;

    default:
        return false;
    }
}
*/

std::string getModulePath()
{
    char buffer[MAX_PATH] = {};

#ifdef _WIN32
    if(!GetModuleFileNameA(NULL, (LPSTR)buffer, sizeof(buffer)))
    {
        throw std::string("GetModuleFileName() failed!");
    }

#else
    ssize_t len(0);
    if((len = readlink("/proc/self/exe",buffer, sizeof(buffer) - 1)) == -1)
    {
        throw std::string("Error: readlink() failed!");
    }

#endif

	std::string str(buffer);

	auto separatorPosition = str.find_last_of(getDirectorySeparator());

	return (separatorPosition != std::string::npos) && (separatorPosition > 0)
		? str.substr(0, separatorPosition + 1)
		: str;
}

bool setCurrentDirectory(const std::string& directoryName)
{

}