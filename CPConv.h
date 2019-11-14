#pragma once
//from char to wchar
//deprecated
wchar_t* AnsiToUnicode(const char* szStr);
/*
from wchar(TJS_W) to char
*/
char* UnicodeToAnsi(const wchar_t* szStr);
//from wchar to UTF8
char* WCSToUTF8(const wchar_t* szStr);
//from UTF8 to wchar
wchar_t* UTF82WCS(const char* szU8);