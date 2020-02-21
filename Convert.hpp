
/*
 *  Convert.hpp
 *  Provides Simple Convertion Template between C++ types and TJS types
 *
 *  Copyright (C) 2018-2019 khjxiaogu
 *
 *  Author: khjxiaogu
 *  Web: http://www.khjxiaogu.com
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
*/
#pragma once
#include <fstream>
#include <vector>
#include <ObjIdl.h>
#include "propmacros.h"
#include "ncbind.hpp"
#include <string>
#include "CPConv.h"
#include "Callers.hpp"
#include "NativeObject.hpp"

//Convert T(JS) String to std C(++) UTF-8 String. 
std::string TStrToCStrUTF8(const ttstr& str) {
	char* cs = WCSToUTF8(str.c_str());
	std::string cstr;
	cstr.assign(cs);
	delete[] cs;
	return cstr;
}
//Convert T(JS) String to std C(++) ANSI String. 
std::string TStrToCStr(const ttstr& str) {
	char* cs = UnicodeToAnsi(str.c_str());
	std::string cstr;
	cstr.assign(cs);
	delete[] cs;
	return cstr;
}
//Get T(JS) String from std C(++) String. 
ttstr CStrToTStr(const std::string& str) {
	try {
		tTJSString res((const tjs_nchar*)str.c_str());
		return res;
	}
	catch(...){
		return ttstr();
	}
}
//Convert T(JS) Octet to std C(++) String. 
std::string TOctToCStr(tTJSVariantOctet* oct) {
	return std::string((const char*)oct->GetData(), oct->GetLength());

}
//Get T(JS) Octet from std C(++) String. 
tTJSVariant CStrToTOct(std::string s) {
	return tTJSVariant((unsigned char*)s.c_str(), s.size());
}
//Iterate Object members and call function.
void IterateObject(iTJSDispatch2* obj,std::function<void(tTJSVariant*, tTJSVariant*)>func) {
	ObjectIterateCaller* caller = new ObjectIterateCaller(func);
	tTJSVariantClosure tvc(caller);
	obj->EnumMembers(NULL,&tvc,NULL);
	caller->Release();
}
