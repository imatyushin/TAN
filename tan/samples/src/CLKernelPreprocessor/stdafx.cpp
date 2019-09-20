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

#endif