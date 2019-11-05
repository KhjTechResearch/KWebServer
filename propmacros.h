/*
 *  propmacros.h
 *  Macros for setting or getting TJS property or object
 *
 *  Copyright (C) 2018-2019 khjxiaogu
 *
 *  Author: khjxiaogu
 *  Web: http://www.khjxiaog.com
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *
*/
#pragma once
#include <windows.h>
#include "tp_stub.h"
#ifndef MACROS
#define MACROS
#define GETSTR(a) tTJSVariant(a).GetString()
#define CONSTRUCTOR Factory(&Class::Factory)
#define NATIVECLASS(a) NCB_REGISTER_CLASS(a)
#define FUNCTION(a) RawCallback(#a, &Class::##a, 0)
template<class T>
 T *alloc(int count=1) {
	return (T*)malloc(sizeof(T)*count);
}
void pprintf(const wchar_t* name, iTJSDispatch2* data, iTJSDispatch2* inst) {
	tTJSVariant i(data);
	inst->PropSet(
		TJS_MEMBERENSURE,
		name,
		NULL,
		&i,
		inst
	);
	data->Release();
}
template<class T>
void pprintf(const wchar_t * name, T data, iTJSDispatch2 * inst) {
	tTJSVariant i(data);
	inst->PropSet(
		TJS_MEMBERENSURE,
		name,
		NULL,
		&i,
		inst
	);
}

template<class T>
void pprintf(int name, T data, iTJSDispatch2 * inst) {
	tTJSVariant i(data);
	inst->PropSetByNum(
		TJS_MEMBERENSURE,
		(tjs_int)name,
		&i,
		inst
	);
}
tTJSVariant getProp(iTJSDispatch2* dic, tjs_char* key) {
	tTJSVariant r;
	dic->PropGet(TJS_MEMBERENSURE, key, NULL, &r, dic);
	return r;
}
tTJSVariant getProp(iTJSDispatch2* dic, tjs_int key) {
	tTJSVariant r;
	dic->PropGetByNum(TJS_MEMBERENSURE, key, &r, dic);
	return r;
}
#endif
