// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#include <stdio.h>

#if defined(_WIN32)

#include <tchar.h>

#else

#define errno_t char
errno_t fopen_s(FILE **f, const char *name, const char *mode);
errno_t _wfopen_s(FILE **f, const wchar_t *name, const wchar_t *mode);

#if defined(UNICODE)
  #define _TCHAR wchar_t
  #define _T(x)	L ## x
  #define _tmain wmain
#else		
  #define _TCHAR char
  #define _T(x)	x
  #define _tmain main
#endif

#define TCHAR _TCHAR;

#endif

//std::string toString(const std::wstring & wideString);
//std::wstring toWideString(const std::string & string);