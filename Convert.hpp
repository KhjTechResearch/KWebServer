#pragma once
#include "server_https.hpp"

// Added for the json-example
//#define BOOST_SPIRIT_THREADSAFE
//#include <boost/property_tree/json_parser.hpp>
//#include <boost/property_tree/ptree.hpp>

// Added for the default_resource example
//#include "crypto.hpp"
//#include <algorithm>
//#include <boost/filesystem.hpp>
#include <fstream>
#include <vector>
#include <ObjIdl.h>
#include "propmacros.h"
#include "ncbind.hpp"
#include <string>
using NativeTJSFunction = std::function<tjs_error(tTJSVariant* r, tjs_int n, tTJSVariant**p)>;
wchar_t* AnsiToUnicode(const char* szStr) {
	int nLen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szStr, -1, NULL, 0);
	if (nLen == 0)
	{
		return NULL;
	}
	wchar_t* pResult = new wchar_t[nLen];
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
	char* pResult = new char[nLen];
	WideCharToMultiByte(CP_ACP, 0, szStr, -1, pResult, nLen, NULL, NULL);
	return pResult;
}
char* WCSToUTF8(const wchar_t* szStr)
{
	int nLen = WideCharToMultiByte(CP_UTF8, 0, szStr, -1, NULL, 0, NULL, NULL);
	if (nLen == 0)
	{
		return NULL;
	}
	char* pResult = new char[nLen];
	WideCharToMultiByte(CP_UTF8, 0, szStr, -1, pResult, nLen, NULL, NULL);
	return pResult;
}
wchar_t* UTF82WCS(const char* szU8)
{
	//预转换，得到所需空间的大小;
	int wcsLen = ::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), NULL, 0);

	//分配空间要给'\0'留个空间，MultiByteToWideChar不会给'\0'空间
	wchar_t* wszString = new wchar_t[wcsLen + 1];

	//转换
	::MultiByteToWideChar(CP_UTF8, NULL, szU8, strlen(szU8), wszString, wcsLen);

	//最后加上'\0'
	wszString[wcsLen] = '\0';

	return wszString;
}
template<typename T>
class PropertyCaller : public tTJSDispatch
{
public:
	typedef typename std::function<T()> getterT;
	typedef typename std::function<void(T)> setterT;
	getterT getter;
	setterT setter;

	PropertyCaller(getterT getter, setterT setter)
		: getter(getter),setter(setter) {
	}
	PropertyCaller(getterT getter,int)
		: getter(getter), setter(NULL) {
	}
	PropertyCaller(int,setterT setter)
		: getter(NULL), setter(setter) {
	}
	tjs_error TJS_INTF_METHOD PropGet(
		tjs_uint32 flag,
		const tjs_char* membername,
		tjs_uint32* hint,
		tTJSVariant* result,
		iTJSDispatch2* objthis
	)
	{
		if (membername)
			return TJS_E_MEMBERNOTFOUND;
		if (getter)
			*result = tTJSVariant(getter());
		else
			return TJS_E_ACCESSDENYED;
		return TJS_S_OK;
	}
	tjs_error TJS_INTF_METHOD PropSet(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			const tTJSVariant* param,
			iTJSDispatch2* objthis
		)
	{
		if (membername)
			return TJS_E_MEMBERNOTFOUND;
		
		if (setter)
			setter(T(*param));
		else
			return TJS_E_ACCESSDENYED;
		return TJS_S_OK;
	}

};
template <>
tjs_error TJS_INTF_METHOD PropertyCaller<tTJSVariantOctet*> ::PropSet(
	tjs_uint32 flag,
	const tjs_char* membername,
	tjs_uint32* hint,
	const tTJSVariant* param,
	iTJSDispatch2* objthis
)
{
	if (membername)
		return TJS_E_MEMBERNOTFOUND;

	if (setter)
		setter(param->AsOctetNoAddRef());
	else
		return TJS_E_ACCESSDENYED;
	return TJS_S_OK;
}
template <>
tjs_error TJS_INTF_METHOD PropertyCaller<tTJSVariantOctet*> ::PropGet(
	tjs_uint32 flag,
	const tjs_char* membername,
	tjs_uint32* hint,
	tTJSVariant* result,
	iTJSDispatch2* objthis
)
{
	if (membername)
		return TJS_E_MEMBERNOTFOUND;
	if (getter) {
		auto get = getter();
		*result = tTJSVariant(get);
		get->Release();
	}
	else
		return TJS_E_ACCESSDENYED;
	return TJS_S_OK;
}
template <>
tjs_error TJS_INTF_METHOD PropertyCaller<iTJSDispatch2*> ::PropSet(
	tjs_uint32 flag,
	const tjs_char* membername,
	tjs_uint32* hint,
	const tTJSVariant* param,
	iTJSDispatch2* objthis
)
{
	if (membername)
		return TJS_E_MEMBERNOTFOUND;

	if (setter)
		setter(param->AsObjectNoAddRef());
	else
		return TJS_E_ACCESSDENYED;
	return TJS_S_OK;
}
template <>
tjs_error TJS_INTF_METHOD PropertyCaller<iTJSDispatch2*> ::PropGet(
	tjs_uint32 flag,
	const tjs_char* membername,
	tjs_uint32* hint,
	tTJSVariant* result,
	iTJSDispatch2* objthis
)
{
	if (membername)
		return TJS_E_MEMBERNOTFOUND;
	if (getter) {
		auto get = getter();
		*result = tTJSVariant(get);
		get->Release();
	}
	else
		return TJS_E_ACCESSDENYED;
	return TJS_S_OK;
}

class DictIterateCaller : public tTJSDispatch
{
public:
	std::function<void(tTJSVariant *,tTJSVariant*)>func;

	DictIterateCaller(std::function<void(tTJSVariant*, tTJSVariant*)> func)
		: func(func) {
	}

	virtual tjs_error TJS_INTF_METHOD FuncCall( // function invocation
		tjs_uint32 flag,			// calling flag
		const tjs_char* membername,// member name ( NULL for a default member )
		tjs_uint32* hint,			// hint for the member name (in/out)
		tTJSVariant* result,		// result
		tjs_int numparams,			// number of parameters
		tTJSVariant** param,		// parameters
		iTJSDispatch2* objthis		// object as "this"
	) {
		if (numparams > 1) {
			if ((int)*param[1] != TJS_HIDDENMEMBER) {
				func(param[0], param[2]);
			}
		}
		if (result)
			*result = true;
		return TJS_S_OK;
	}
};
class FunctionCaller : public tTJSDispatch
{
public:
	std::function<tjs_error(tTJSVariant*, tjs_int, tTJSVariant**)> func;

	FunctionCaller(NativeTJSFunction func)
		: func(func) {
	}

	virtual tjs_error TJS_INTF_METHOD FuncCall( // function invocation
		tjs_uint32 flag,			// calling flag
		const tjs_char* membername,// member name ( NULL for a default member )
		tjs_uint32* hint,			// hint for the member name (in/out)
		tTJSVariant* result,		// result
		tjs_int numparams,			// number of parameters
		tTJSVariant** param,		// parameters
		iTJSDispatch2* objthis		// object as "this"
	) {
		if (membername)return TJS_E_MEMBERNOTFOUND;
		return func(result, numparams, param);
			
	}
};

class NativeObject : public tTJSDispatch
{
public:
	std::map<std::wstring,std::shared_ptr<iTJSDispatch2>> functions;

	NativeObject(){
	}
	~NativeObject() {
		for (std::map<std::wstring, std::shared_ptr<iTJSDispatch2>>::iterator it = functions.begin(); it != functions.end(); it++) {
			it->second->Release();
		}
	}
	virtual tjs_error TJS_INTF_METHOD FuncCall( // function invocation
		tjs_uint32 flag,			// calling flag
		const tjs_char* membername,// member name ( NULL for a default member )
		tjs_uint32* hint,			// hint for the member name (in/out)
		tTJSVariant* result,		// result1
		tjs_int numparams,			// number of parameters
		tTJSVariant** param,		// parameters
		iTJSDispatch2* objthis		// object as "this"
	) {
		if (!membername)return TJS_E_MEMBERNOTFOUND;
		if (functions.find(membername) != functions.end())
			return functions.at(membername)->FuncCall(flag, NULL, hint, result, numparams, param, objthis);
		else
			return TJS_E_MEMBERNOTFOUND;
		
	}
	tjs_error TJS_INTF_METHOD PropGet(
		tjs_uint32 flag,
		const tjs_char* membername,
		tjs_uint32* hint,
		tTJSVariant* result,
		iTJSDispatch2* objthis
	)
	{
		if (!membername) {
			*result = tTJSVariant(this);
			return TJS_S_OK;
		}
		else
			if (functions.find(membername) != functions.end()) {
				*result = tTJSVariant(&(functions.at(membername)));
				return TJS_S_OK;
			}
			else
				return TJS_E_MEMBERNOTFOUND;
	}
	virtual tjs_error TJS_INTF_METHOD PropSet(
		tjs_uint32 flag,
		const tjs_char* membername,
		tjs_uint32* hint,
		const tTJSVariant* param,
		iTJSDispatch2* objthis
	)
	{
		if (!membername) {
			return TJS_E_MEMBERNOTFOUND;
		}
		else if (param->Type() == tvtObject) {
			if (functions.find(membername) != functions.end()) {
				functions.at(membername)->Release();
			}
			functions.insert(std::pair<std::wstring, std::shared_ptr<iTJSDispatch2>>(std::wstring(membername), std::shared_ptr<iTJSDispatch2>(param->AsObject())));
		}
		else
			if (functions.find(membername) != functions.end()) {
				functions.at(membername)->PropSet(flag, membername, NULL, param, objthis);
				return TJS_S_OK;
			}
			else
				return TJS_E_MEMBERNOTFOUND;
	}
	void putFunc(const std::wstring str, NativeTJSFunction func) {
		functions.insert(std::pair<std::wstring, std::shared_ptr<iTJSDispatch2>>(str, std::shared_ptr<iTJSDispatch2>(new FunctionCaller(func))));
	}
	template<typename  T> void putProp(const std::wstring str,typename PropertyCaller<T>::getterT getter=NULL, typename PropertyCaller<T>::setterT setter=NULL) {
		functions.insert(std::pair<std::wstring, std::shared_ptr<iTJSDispatch2>>(str, std::shared_ptr<iTJSDispatch2>(new PropertyCaller<T>(getter,setter))));
	}
	tjs_error TJS_INTF_METHOD
		EnumMembers(tjs_uint32 flag, tTJSVariantClosure* callback, iTJSDispatch2* objthis)
	{
		tTJSVariant* par = new tTJSVariant[3];
		par[1] = NULL;
		for (std::map<std::wstring, std::shared_ptr<iTJSDispatch2>>::iterator it = functions.begin(); it != functions.end(); it++) {
			par[0] = ttstr(it->first.c_str());
			par[2] = tTJSVariant(&(*(it->second)));
			callback->FuncCall(NULL, NULL, NULL, &tTJSVariant(), 3,&par, objthis);
		}
		return TJS_S_OK;
	}
	tjs_error TJS_INTF_METHOD
		Invalidate(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		if (membername)
			return  TJS_E_MEMBERNOTFOUND;
		else {
			delete this;
			return TJS_S_OK;
		}
	}
	tjs_error TJS_INTF_METHOD
		DeleteMember(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		if (!membername) {
			return TJS_E_MEMBERNOTFOUND;
		}
		if (functions.find(membername) != functions.end()) {
			functions.at(membername)->Release();
			functions.erase(membername);
			return TJS_S_OK;
		}
	}
	tjs_error TJS_INTF_METHOD
		IsValid(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		if (!membername) {
			return TJS_S_TRUE;
		}
		if (functions.find(membername) != functions.end()) {
			
			return functions.at(membername)->IsValid(flag, NULL, hint, objthis);
		}
		return TJS_E_MEMBERNOTFOUND;
	}
};
std::string TStrToCStr(const ttstr& str) {
	char* cs = WCSToUTF8(str.c_str());
	std::string cstr;
	cstr.assign(cs);
	delete[] cs;
	return cstr;
}
ttstr CStrToTStr(const std::string& str) {
	//TVPAddLog((const tjs_nchar*)str.c_str());
	tTJSString res((const tjs_nchar*)str.c_str());
	return res;
}
void IterateObject(iTJSDispatch2* obj,std::function<void(tTJSVariant*, tTJSVariant*)>func) {
	DictIterateCaller* caller = new DictIterateCaller(func);
	tTJSVariantClosure tvc(caller);
	obj->EnumMembers(NULL,&tvc,NULL);
	caller->Release();
}
SimpleWeb::CaseInsensitiveMultimap TDictToCMap(iTJSDispatch2*obj) {
	SimpleWeb::CaseInsensitiveMultimap map;
	IterateObject(obj,[&map](tTJSVariant* key, tTJSVariant* val) {
		map.insert(std::pair<std::string, std::string>(TStrToCStr(key->AsStringNoAddRef()), TStrToCStr(val->AsStringNoAddRef())));
		});
	return map;
}
iTJSDispatch2* CMapToTDict(SimpleWeb::CaseInsensitiveMultimap map) {
	iTJSDispatch2* obj=TJSCreateDictionaryObject();
	for (SimpleWeb::CaseInsensitiveMultimap::iterator it = map.begin(); it != map.end(); it++) {
		pprintf(CStrToTStr((*it).first).c_str(), CStrToTStr((*it).second),obj);
	}
	return obj;
}