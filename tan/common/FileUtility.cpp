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

std::string getCurrentDirectory()
{
    char currentDirectory[MAX_PATH] = { 0 };

#ifdef _WIN32
    GetCurrentDirectory(MAX_PATH, currentDirectory);
#else
	getcwd(currentDirectory, MAX_PATH);
#endif

	return currentDirectory;
}

bool setCurrentDirectory(const std::string& directoryName)
{
	return -1 !=
#ifdef _WIN32
		_chdir(directoryName.c_str());
#else
		chdir(directoryName.c_str());
#endif
		;
}

std::string joinPaths(const std::string & left, const std::string & right)
{
	auto separator(getDirectorySeparator());

	return left + (left.rfind(separator) == (left.size() - separator.length()) ? right : separator + right);
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

/*
void getFileVersionAndDate(wchar_t *logMessage, char *version, size_t maxLength))
{
#ifdef _WIN32
	time_t dt = time(NULL);
	tm *lt = localtime(&dt);

	auto moduleFileName = getModuleFileName();

	DWORD dummy = 0;
	DWORD size = GetFileVersionInfoSizeW(filename, &dummy);
	std::wstring buffer(size + 1);

	WCHAR * buffer = new WCHAR[size];
	if (buffer == NULL){ return; }
	WCHAR *ver = NULL, *pStr;
	WCHAR *pSrch = L"FileVersion";
	if (GetFileVersionInfoW(filename, 0, size, (void*)buffer)){
		pStr = buffer;
		while (pStr < buffer + size){
			if (wcsncmp(pStr, pSrch, 10) == 0){
				ver = pStr + wcslen(pStr) + 2;
				break;
			}
			else {
				pStr++;
			}
		}
	}

	wsprintfW(logMessage, L"**** %s v%s on %4d/%02d/%02d %02d:%02d:%02d ****\n", filename, ver,
		2000 + (lt->tm_year % 100), 1 + lt->tm_mon, lt->tm_mday, lt->tm_hour, lt->tm_min, lt->tm_sec);

	int lv = wcslen(ver);
	for (int i = 0; i < lv; i++){
		version[i] = char(ver[i]);
	}
	delete []buffer;
#else
    //todo: implement
	std::swprintf(logMessage, 256, L"Error! Not implemented");
	std::snprintf(version, 256, "Error! Not implemented");
#endif
}
*/

std::string getPath2File(const std::string& fileNameWithPath)
{
	auto separatorPosition = fileNameWithPath.find_last_of(getDirectorySeparator());

	return (separatorPosition != std::string::npos)
		? fileNameWithPath.substr(0, separatorPosition + 1)
		: "";
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
        return fileName.substr(lastDot + 1);
    }

    return "";
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

std::string getModuleFileName()
{
    char buffer[MAX_PATH] = {};

#ifdef _WIN32
    if(!GetModuleFileNameA(NULL, (LPSTR)buffer, sizeof(buffer)))
    {
        throw std::string("GetModuleFileName() failed!");
    }
#else
    if(-1 == readlink("/proc/self/exe", buffer, MAX_PATH))
    {
        throw std::string("Error: readlink() failed!");
    }
#endif

	return buffer;

	//todo: ivm: ivestigate why?
	/*
	std::string str(buffer);

	auto separatorPosition = str.find_last_of(getDirectorySeparator());

	return (separatorPosition != std::string::npos) && (separatorPosition > 0)
		? str.substr(0, separatorPosition + 1)
		: str;
	*/
}