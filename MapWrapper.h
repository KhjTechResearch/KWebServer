#pragma once
#include "tp_stub.h"
#include "server_https.hpp"
#include "convert.hpp"
class MapWrapper :
	public tTJSDispatch
{
public:
	SimpleWeb::CaseInsensitiveMultimap& map;
	MapWrapper(SimpleWeb::CaseInsensitiveMultimap& map):map(map) {};
	tjs_error TJS_INTF_METHOD
		PropGet(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			tTJSVariant* result,
			iTJSDispatch2* objthis
		)
	{
		if(membername){
			auto it=map.find(TStrToCStr(ttstr(membername)));
			if (it == map.end()) {
				if (flag & TJS_MEMBERMUSTEXIST)
					return TJS_E_MEMBERNOTFOUND;
				if (flag & TJS_MEMBERENSURE)
					*result = tTJSVariant();
				return TJS_S_OK;
			}
			*result = CStrToTStr(it->second);
			return TJS_S_OK;
		}
		*result = this;
		return TJS_S_OK;
	}

	tjs_error TJS_INTF_METHOD
		PropGetByNum(
			tjs_uint32 flag,
			tjs_int num,
			tTJSVariant* result,
			iTJSDispatch2* objthis
		) {
		auto it = map.find(TStrToCStr(ttstr(num)));
		if (it == map.end()) {
			return TJS_S_OK;
		}
		*result = CStrToTStr(it->second);
		return TJS_S_OK;
	}



	tjs_error TJS_INTF_METHOD
		GetCount(
			tjs_int* result,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		*result=(int) map.size();
		return TJS_S_OK;
	}


	tjs_error TJS_INTF_METHOD
		PropSetByVS(tjs_uint32 flag, tTJSVariantString* membername,
			const tTJSVariant* param, iTJSDispatch2* objthis)
	{
		return TJS_E_NOTIMPL;
	}

	tjs_error TJS_INTF_METHOD
		EnumMembers(tjs_uint32 flag, tTJSVariantClosure* callback, iTJSDispatch2* objthis)
	{
		return TJS_E_NOTIMPL;
	}

	tjs_error TJS_INTF_METHOD
		Invalidate(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		return membername ? TJS_E_MEMBERNOTFOUND : TJS_E_NOTIMPL;
	}

	tjs_error TJS_INTF_METHOD
		InvalidateByNum(
			tjs_uint32 flag,
			tjs_int num,
			iTJSDispatch2* objthis
		);

	tjs_error TJS_INTF_METHOD
		IsValid(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2* objthis
		)
	{
		return membername ? TJS_E_MEMBERNOTFOUND : TJS_E_NOTIMPL;
	}

	tjs_error TJS_INTF_METHOD
		IsValidByNum(
			tjs_uint32 flag,
			tjs_int num,
			iTJSDispatch2* objthis
		);

	tjs_error TJS_INTF_METHOD
		CreateNew(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			iTJSDispatch2** result,
			tjs_int numparams,
			tTJSVariant** param,
			iTJSDispatch2* objthis
		)
	{
		return membername ? TJS_E_MEMBERNOTFOUND : TJS_E_NOTIMPL;
	}

	tjs_error TJS_INTF_METHOD
		CreateNewByNum(
			tjs_uint32 flag,
			tjs_int num,
			iTJSDispatch2** result,
			tjs_int numparams,
			tTJSVariant** param,
			iTJSDispatch2* objthis
		);

	tjs_error TJS_INTF_METHOD
		Reserved1(
		)
	{
		return TJS_E_NOTIMPL;
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
		return membername ? TJS_E_MEMBERNOTFOUND : TJS_E_NOTIMPL;
	}

	tjs_error TJS_INTF_METHOD
		IsInstanceOfByNum(
			tjs_uint32 flag,
			tjs_int num,
			const tjs_char* classname,
			iTJSDispatch2* objthis
		);

	tjs_error TJS_INTF_METHOD
		Operation(
			tjs_uint32 flag,
			const tjs_char* membername,
			tjs_uint32* hint,
			tTJSVariant* result,
			const tTJSVariant* param,
			iTJSDispatch2* objthis
		);

	tjs_error TJS_INTF_METHOD
		OperationByNum(
			tjs_uint32 flag,
			tjs_int num,
			tTJSVariant* result,
			const tTJSVariant* param,
			iTJSDispatch2* objthis
		);
};

