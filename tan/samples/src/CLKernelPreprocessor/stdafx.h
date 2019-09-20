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