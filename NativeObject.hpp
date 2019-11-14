/*
 *  NativeObject.hpp
 *
 *  Define native objects for use.
 *  Copyright (C) 2018-2019 khjxiaogu
 *
 *  Author: khjxiaogu
 *  Web: http://www.khjxiaogu.com
*/
#pragma once
//Native Object to provide bridge between TJS Object call and Native Object
class NativeObject : public tTJSDispatch
{
public:
	//map to store interal functions
	std::map<std::wstring, iTJSDispatch2*> functions;
	//lock to prevent multi-threading errors
	std::mutex concurrent;
	//Constructors are defined by children.
	NativeObject() {
	}
	//free all functions within this object
	virtual ~NativeObject() {
		std::lock_guard<std::mutex> lock(concurrent);
		for (std::map<std::wstring, iTJSDispatch2*>::iterator it = functions.begin(); it != functions.end(); it++) {
			try {
				it->second->Release();
			}
			catch (...)
			{
			}
		}
		//TVPAddLog("native obj released");
	}
	//for instanceof operation,reload this to set the class name
	virtual const wchar_t* getClass() {
		return L"NativeClass";
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
		std::lock_guard<std::mutex> lock(concurrent);
		if (!membername)return TJS_E_MEMBERNOTFOUND;
		if (functions.find(membername) != functions.end())
			return functions.at(membername)->FuncCall(flag, NULL, hint, result, numparams, param, objthis);
		else
			return TJS_E_MEMBERNOTFOUND;
	}
	virtual tjs_error TJS_INTF_METHOD PropGet(
		tjs_uint32 flag,
		const tjs_char* membername,
		tjs_uint32* hint,
		tTJSVariant* result,
		iTJSDispatch2* objthis
	)
	{
		std::lock_guard<std::mutex> lock(concurrent);
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
		std::lock_guard<std::mutex> lock(concurrent);
		if (!membername) {
			return TJS_E_NOTIMPL;
		}
		else if (param->Type() == tvtObject) {
			if (functions.find(membername) != functions.end()) {
				functions.at(membername)->Release();
			}
			functions.insert(std::pair<std::wstring, iTJSDispatch2*>(std::wstring(membername), (param->AsObject())));
		}
		else
			if (functions.find(membername) != functions.end()) {
				functions.at(membername)->PropSet(flag, membername, NULL, param, objthis);
				return TJS_S_OK;
			}
			else
				return TJS_E_MEMBERNOTFOUND;

		return TJS_E_MEMBERNOTFOUND;

	}
	//macro to put lambda function into this object
	void putFunc(const std::wstring str, NativeTJSFunction func) {
		std::lock_guard<std::mutex> lock(concurrent);
		functions.insert(std::pair<std::wstring, iTJSDispatch2*>(str, (new FunctionCaller(func))));
	}
	//macro to put lambda properto into this object.
	template<typename  T> void putProp(const std::wstring str,
		typename PropertyCaller<T>::getterT getter = NULL,
		typename PropertyCaller<T>::setterT setter = NULL) {
		std::lock_guard<std::mutex> lock(concurrent);
		functions.insert(std::pair<std::wstring, iTJSDispatch2*>(str, (new PropertyCaller<T>(getter, setter))));
	}
	tjs_error TJS_INTF_METHOD
		EnumMembers(tjs_uint32 flag, tTJSVariantClosure* callback, iTJSDispatch2* objthis)
	{
		std::lock_guard<std::mutex> lock(concurrent);
		tTJSVariant* par[3];
		par[0] = new tTJSVariant();
		par[1] = new tTJSVariant(NULL);
		par[2] = new tTJSVariant();
		for (std::map<std::wstring, iTJSDispatch2*>::iterator it = functions.begin(); it != functions.end(); it++) {
			*par[0] = ttstr(it->first.c_str());
			*par[2] = tTJSVariant(&(*(it->second)));
			callback->FuncCall(NULL, NULL, NULL, &tTJSVariant(), 3, par, objthis);
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
		std::lock_guard<std::mutex> lock(concurrent);
		if (!membername) {
			return TJS_E_MEMBERNOTFOUND;
		}
		if (functions.find(membername) != functions.end()) {
			functions.at(membername)->Release();
			functions.erase(membername);
			return TJS_S_OK;
		}
		return TJS_E_MEMBERNOTFOUND;
	}
	tjs_error TJS_INTF_METHOD
		IsValid(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		std::lock_guard<std::mutex> lock(concurrent);
		if (!membername) {
			return TJS_S_TRUE;
		}
		if (functions.find(membername) != functions.end()) {

			return TJSIsObjectValid(functions.at(membername)->IsValid(flag, NULL, hint, objthis));
		}
		return TJS_E_MEMBERNOTFOUND;
	}
	tjs_error TJS_INTF_METHOD
		IsInstanceOf(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			const tjs_char* classname,
			iTJSDispatch2* objthis
		)
	{
		if (!membername) {
			if (wcscmp(classname, L"Class") || wcscmp(classname, getClass()))
				return TJS_S_TRUE;
		}
		else
			return functions.at(membername)->IsInstanceOf(flag, NULL, hint, classname, objthis);
		return TJS_S_FALSE;
	}
};