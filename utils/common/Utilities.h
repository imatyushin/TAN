#pragma once

#ifndef countOf

#if defined(_MSC_VER)
	#define countOf _countof
#else
	#define countOf(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#endif