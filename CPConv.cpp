/*
 *  CPConv.cpp
 *
 *  Implements the Codepage Convertions in win32
 *  Copyright (C) 2018-2019 khjxiaogu
 *
 *  Author: khjxiaogu
 *  Web: http://www.khjxiaogu.com
*/
#include "CPConv.h"
#include <windows.h>
//from char to wchar
//deprecated
wchar_t* AnsiToUnicode(const char* szStr) {
	int nLen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szStr, -1, NULL, 0);
	if (nLen == 0)
	{
		return NULL;
	}
	wchar_t* pResult = new wchar_t[nLen + 1];
	MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szStr, -1, pResult, nLen);
	return pResult;
}

/*
from wchar(TJS_W) to char
*/
char* UnicodeToAnsi(const wchar_t* szStr)
{
	int nLen = WideCharToMultiByte(CP_ACP, 0, szStr, -1, NULL, 0, NULL, NULL);
	if (nLen == 0)
	{
		return NULL;
	}
	char* pResult = new char[nLen + 1];
	WideCharToMultiByte(CP_ACP, 0, szStr, -1, pResult, nLen, NULL, NULL);
	return pResult;
}

//from wchar to UTF8
char* WCSToUTF8(const wchar_t* szStr)
{
	int nLen = WideCharToMultiByte(CP_UTF8, 0, szStr, -1, NULL, 0, NULL, NULL);
	if (nLen == 0)
	{
		return NULL;
	}
	char* pResult = new char[nLen + 1];
	WideCharToMultiByte(CP_UTF8, 0, szStr, -1, pResult, nLen, NULL, NULL);
	return pResult;
}

//from UTF8 to wchar
wchar_t* UTF82WCS(const char* szU8)
{
	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), NULL, 0);
	wchar_t* wszString = new wchar_t[wcsLen + 1];

	//转换
	::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), wszString, wcsLen);

	//最后加上'\0'
	wszString[wcsLen] = '\0';

	return wszString;
}
