/*
 *  Callers.hpp
 *
 *  Define some native object with lambda members to help simplify the code
 *  Copyright (C) 2018-2019 khjxiaogu
 *
 *  Author: khjxiaogu
 *  Web: http://www.khjxiaogu.com
*/
template<typename T>
class PropertyCaller : public tTJSDispatch
{
public:
	//getter type and setter type macro
	typedef typename std::function<T()> getterT;
	typedef typename std::function<void(T)> setterT;
	getterT getter;
	setterT setter;
	//normal construct
	PropertyCaller(getterT getter, setterT setter)
		: getter(getter), setter(setter) {
	}
	//Read only property
	PropertyCaller(getterT getter, int)
		: getter(getter), setter(NULL) {
	}
	//Write only property
	PropertyCaller(int, setterT setter)
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
	tjs_error TJS_INTF_METHOD
		IsInstanceOf(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			const tjs_char* classname,
			iTJSDispatch2* objthis
		)
	{
		if (membername)
			return TJS_E_MEMBERNOTFOUND;
		else
			if (wcscmp(classname, L"Property"))
				return TJS_S_TRUE;
		return TJS_S_FALSE;
	}
};

//For some special type,define a type-convertion function
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

//Function caller to Iterate Object Members.
class ObjectIterateCaller : public tTJSDispatch
{
public:
	std::function<void(tTJSVariant*, tTJSVariant*)>func;

	ObjectIterateCaller(std::function<void(tTJSVariant*, tTJSVariant*)> func)
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
//define Native function lambda type
using NativeTJSFunction = std::function<tjs_error(tTJSVariant * r, tjs_int n, tTJSVariant * *p)>;
//Function caller to provide a bridge between tjs function and native function
class FunctionCaller : public tTJSDispatch
{
public:
	NativeTJSFunction func;

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
	tjs_error TJS_INTF_METHOD
		IsInstanceOf(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			const tjs_char* classname,
			iTJSDispatch2* objthis
		)
	{
		if (membername)
			return TJS_E_MEMBERNOTFOUND;
		else
			if (wcscmp(classname, L"Function"))
				return TJS_S_TRUE;
		return TJS_S_FALSE;
	}
};