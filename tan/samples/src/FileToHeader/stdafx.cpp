// stdafx.cpp : source file that includes just the standard includes
// CLKernelPreprocessor.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

#if !defined(_WIN32) && defined( __GNUC__ ) 

#include <cassert>
#include <cerrno>
#include <cerrno>
#include <cwchar>
#include <cstdlib>

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

errno_t fopen_s(FILE **f, const char *name, const char *mode)
{
    assert(f);

    errno_t ret = 0;
    *f = fopen(name, mode);
    
    /* Can't be sure about 1-to-1 mapping of errno and MS' errno_t */
    if(!*f)
    {
        ret = errno;
    }

    return ret;
}

/*inline std::string narrow(std::wstring const& text)
{
    std::locale const loc("");
    wchar_t const* from = text.c_str();
    std::size_t const len = text.size();
    std::vector<char> buffer(len + 1);
    std::use_facet<std::ctype<wchar_t> >(loc).narrow(from, from + len, '_', &buffer[0]);
    return std::string(&buffer[0], &buffer[len]);
}

std::string w2s(const std::wstring &var)
{
   static std::locale loc("");
   auto &facet = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(loc);
   return std::wstring_convert<std::remove_reference<decltype(facet)>::type, wchar_t>(&facet).to_bytes(var);
}

std::wstring s2w(const std::string &var)
{
   static std::locale loc("");
   auto &facet = std::use_facet<std::codecvt<wchar_t, char, std::mbstate_t>>(loc);
   return std::wstring_convert<std::remove_reference<decltype(facet)>::type, wchar_t>(&facet).from_bytes(var);
}*/

//unsafe proxy
errno_t _wfopen_s(FILE **f, const wchar_t *name, const wchar_t *mode)
{
    auto length(wcslen(name));
    char charName[length + 1];
    std::wcstombs(charName, name, length);

    length = wcslen(mode);
    char charMode[length + 1];
    std::wcstombs(charMode, mode, length);

    return fopen_s(f, charName, charMode);
}

#endif