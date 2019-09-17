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

#include <errno.h>

#if !defined(__APPLE__) && !defined(__MACOSX)
  #ifndef errno_t
    #define errno_t char
  #endif
#endif

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