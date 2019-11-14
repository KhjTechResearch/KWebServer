/*
 *  httpserver.cpp
 *  A simple hi-performance web server for TVP/krkr 2/z core for win32
 *
 *  Copyright (C) 2018-2019 khjxiaogu
 *
 *  Author: khjxiaogu
 *  Web: http://www.khjxiaog.com
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *
*/

#include "server_https.hpp"
#include "Convert.hpp"
#include <math.h>
#include "IStreamStdWrapper.h"
#define USE_TVP_EVENT
using namespace std;
//alias define
using HttpsServer = SimpleWeb::Server<SimpleWeb::HTTPS>;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
//macro to define property of server config
#define SC_PROPERTY_SETTER(T,X)\
void set##X(T val) {\
	server->config.##X = val;\
}
#define SC_PROPERTY_GETTER(T,X) \
T get##X() {\
  return server->config.##X; \
}
#define SC_PROPERTY_DEF(T,X) \
SC_PROPERTY_SETTER(T,X)\
SC_PROPERTY_GETTER(T,X)




static mutex globalParseMutex;//parsing mutex,avoid errors.
static mutex RefCountMutex;//counting reference mutex,avoid threading problems
static int aliveConnCount;//counts alive conections
extern tjs_int TVPPluginGlobalRefCount;
//define message
#define WM_ON_HTTP_REQUEST (WM_USER|0x3533)

//count connection macros
#define REGISTALIVECONN RefCountMutex.lock();aliveConnCount++;RefCountMutex.unlock()
#define UNREGISTALIVECONN RefCountMutex.lock();aliveConnCount--;RefCountMutex.unlock()

//define the call lambda
#define RequestEnvelop(X,N,T) RequestEnvelopDiffer(X,X,N,T)

//function to parse requests
template <class Y>
iTJSDispatch2* getRequestParam(shared_ptr<typename SimpleWeb::Server<Y>::Request> request) {
	iTJSDispatch2* r = TJSCreateDictionaryObject();
	while(!r)
		r = TJSCreateDictionaryObject();
	pprintf(L"method", CStrToTStr(request->method), r);
	pprintf(L"path", CStrToTStr(request->path), r);
	pprintf(L"query", CMapToTDict(request->parse_query_string()), r);
	pprintf(L"version", CStrToTStr(request->http_version), r);
	pprintf(L"headers", CMapToTDict(request->header), r);
	std::string s;
	s.assign(request->content.string());
	//pprintf(L"method", CStrToTStr(request->method), r);
	boost::asio::ip::tcp::endpoint ep = request->remote_endpoint();
	pprintf(L"client", (CStrToTStr(ep.address().to_string()) + L":" + ttstr(ep.port())), r);//cpmbine ip string
	pprintf(L"contentString", new PropertyCaller<ttstr>([s] {return CStrToTStr(s); }, NULL), r);//Convert only if used.
	pprintf(L"contentData", new PropertyCaller<tTJSVariantOctet*>([s] { return new tTJSVariantOctet((unsigned char*)s.c_str(), s.size()); }, NULL), r);
	return r;
}
class MainEvent {
	tTJSVariant** vars;
	int argc;
	const wchar_t* member;
	tTJSVariantClosure cls;
public:
	MainEvent(tTJSVariantClosure cls,int argc, tTJSVariant** vars,const wchar_t* members) :cls(cls),argc(argc),vars(vars),member(members){
	};
	void Invoke() {
		//tTJSVariant result;
		//tTJSVariantClosure caller(otarg,othis);
			cls.FuncCall(NULL, member, NULL,NULL, argc, vars,NULL);
	}
	~MainEvent() {
		for(int i=0;i<argc;i++)
			delete *(vars+i);
	}
};
ofstream *os;
//event call lambda macro
#ifdef USE_TVP_EVENT
#define RequestEnvelopDiffer(X,Y,N,T) [=](shared_ptr <SimpleWeb::Server<##T>::Response> res, shared_ptr <SimpleWeb::Server<##T>::Request> req)\
 {\
std::lock_guard<mutex> mtx(globalParseMutex);\
ttstr mn(N);\
tTJSVariant *arg=new tTJSVariant[2];\
auto itq= getRequestParam<T>(req);\
auto its =new  KResponse<T>(res);\
arg[0] = tTJSVariant(itq,itq); \
arg[1] = its; \
/*Y->FuncCall(NULL,NULL,NULL,NULL,2,&arg,X);*/\
TVPPostEvent(X, Y,mn,rand(),/*NULL*/TVP_EPT_EXCLUSIVE, 2,arg);\
its->Release();\
itq->Release();\
delete[] arg;\
}
#else  
#define RequestEnvelopDiffer(X,Y,N,T) [=](shared_ptr <SimpleWeb::Server<##T>::Response> res, shared_ptr <SimpleWeb::Server<##T>::Request> req)\
 {\
std::lock_guard<mutex> mtx(globalParseMutex);\
tTJSVariant* arg[2]; \
auto itq = getRequestParam<T>(req); \
auto its = new  KResponse<T>(res);\
itq->AddRef();\
arg[0] =new tTJSVariant(itq,itq); \
arg[1] =new tTJSVariant(its); \
PostMessageW(hwnd, WM_HTTP_REQUEST,NULL,(LPARAM)new MainEvent(X,2,arg,N) );\
its->Release();\
/*itq->Release();*/\
}
#endif

//file sender,default 128k per packet.
template<typename T,int packetsize= 1024*128>
class FileSender {
public:
	shared_ptr<typename T::Response> response;
	shared_ptr < ifstream> ifs;
	const std::function<void(SimpleWeb::error_code)> callback;
	char* buffer;
	FileSender(shared_ptr<typename T::Response> response, shared_ptr <ifstream> ifs, const std::function<void(SimpleWeb::error_code)> callback)
		:response(response),ifs(ifs),callback(callback) {
		//Allocate buffer
		char x;
		ifs->read(&x, 1);//read a bit to know if it is valid
		ifs->seekg(0);
		buffer = new char[packetsize];
		REGISTALIVECONN;
		
	}
	~FileSender() {
		ifs->close();
		callback(SimpleWeb::error_code());
		delete[] buffer;
		UNREGISTALIVECONN;
	}
	void send() {
		streamsize read_length;
		try {
			if ((read_length = ifs->read(buffer, static_cast<streamsize>(packetsize)).gcount()) > 0) {
				response->write(buffer, read_length);//write and send
				response->send([=, this](const SimpleWeb::error_code& ec) {
					if (!ec)
						if (read_length == packetsize) {
							send();
						}
						else
							delete this;//if send complete
					else
						callback(ec);
					});
			}
		}
		catch (...) {
			send();
		}
	}
};
#define RELEASEIF 
template <class Y>
class KResponse : public NativeObject
{
	typedef SimpleWeb::Server<Y> T;
	//boolean unreleased=true;
public:
	std::function<void(tTJSVariant*, tTJSVariant*)>func;
	shared_ptr<typename T::Response> response;
	KResponse(shared_ptr<typename T::Response> res) :response(res) {
		REGISTALIVECONN;
		putFunc(L"write", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {
			if (n < 1)
				response->write();//default write with 200 and empty response
			else if (n < 2)
				response->write((SimpleWeb::StatusCode)p[0]->AsInteger());//only write status code
			else if (n < 3)
				if (p[0]->Type() == tvtVoid)
					response->write(TDictToCMap(p[1]->AsObjectNoAddRef()));//only write header with default 200
				else
					response->write((SimpleWeb::StatusCode)p[0]->AsInteger(), TDictToCMap(p[1]->AsObjectNoAddRef()));//write header and status code
			else if (n == 3)
				if (p[2]->Type() == tvtString)
					response->write((SimpleWeb::StatusCode)p[0]->AsInteger(), TStrToCStr(p[2]->AsStringNoAddRef()), TDictToCMap(p[1]->AsObjectNoAddRef()));//write headers and a string
				else if (p[2]->Type() == tvtOctet)
					response->write((SimpleWeb::StatusCode)p[0]->AsInteger(), string((const char*)p[2]->AsOctetNoAddRef()->GetData(), p[2]->AsOctetNoAddRef()->GetLength()), TDictToCMap(p[1]->AsObjectNoAddRef()));//write headers and an octet
				else
					return TJS_E_INVALIDPARAM;
			else
				return TJS_E_BADPARAMCOUNT;
			RELEASEIF
			return TJS_S_OK;
			});
		putFunc(L"append", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {
			if (n < 1) return TJS_E_BADPARAMCOUNT;
			if (p[0]->Type() == tvtString)
				*response << TStrToCStr(p[0]->AsStringNoAddRef());
			else if (p[2]->Type() == tvtOctet)
				*response << string((const char*)p[2]->AsOctetNoAddRef()->GetData(), p[2]->AsOctetNoAddRef()->GetLength());
			else
				return TJS_E_INVALIDPARAM;
			RELEASEIF
			return TJS_S_OK;
			});
		putFunc(L"appendFile", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {
			if (n < 1) return TJS_E_BADPARAMCOUNT;
			ttstr stor(*p[0]);
			IStream* tvpstr = TVPCreateIStream(stor, TJS_BS_READ);//open TVP IStream
			auto ISW = new UnbufferedOLEStreamBuf(tvpstr);//wrap it
			istream ifs(ISW);//wrap it again
			response->write(ifs);
			RELEASEIF
			return TJS_S_OK;
			});
		putFunc(L"send", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {

			//send with callback
			response->send([this](SimpleWeb::error_code ec) {
#ifdef USE_TVP_EVENT
				std::lock_guard<mutex> mtx(globalParseMutex);
				tTJSVariant* tv = new tTJSVariant[2];
				if (ec) {
					tv[0] = ec.value();
					tv[1] = CStrToTStr(ec.message());
				}
				static ttstr evn(L"onSent");
				TVPPostEvent(this, this, evn, NULL, NULL, 2, tv);
				delete[] tv;
#endif
				});
			RELEASEIF
			return TJS_S_OK;
			});
		putFunc(L"release", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {
			this->Release();
			return TJS_S_OK;
			});
		putFunc(L"sendFile", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {
			if (n < 1) return TJS_E_BADPARAMCOUNT;

			ttstr stor(*p[0]);
			stor = TVPNormalizeStorageName(stor);
			TVPGetLocalName(stor);//get local file name
			//TVPAddLog(stor);
			ifstream* ifs = new ifstream();
			while(!ifs->is_open())
				ifs->open(stor.c_str(), ios::binary,_SH_DENYWR);//open file
			this->AddRef();
			if (!ifs->good())
				return TJS_E_INVALIDPARAM;

			if (n == 2) {
				response->write((SimpleWeb::StatusCode)p[1]->AsInteger());//write if status code provided
			}
			else if (n == 3) {
				auto headers = TDictToCMap(p[2]->AsObjectNoAddRef());
				ifs->seekg(0, ios::end);
				headers.emplace(std::pair<std::string,std::string>("Content-Length", std::to_string(ifs->tellg())));//set to content length
			//	TVPAddLog(std::to_string(ifs->tellg()).c_str());
				ifs->seekg(0,ios::beg);
				response->write((SimpleWeb::StatusCode)p[1]->AsInteger(),headers);//write headers if provided
			}
			//instance filesender with callback and stream
			FileSender<T>* fs = new FileSender<T>(response, shared_ptr <ifstream>(ifs), [&, this](SimpleWeb::error_code ec) {
#ifdef USE_TVP_EVENT
				std::lock_guard<mutex> mtx(globalParseMutex);
				tTJSVariant* tv = new tTJSVariant[2];
				if (ec) {
					tv[0] = ec.value();
					tv[1] = CStrToTStr(ec.message());
				}
				try {
					static ttstr evn(L"onSent");
					TVPPostEvent(this, this, evn, NULL, NULL, 2, tv);
				
					this->Release();

				}
				catch (...) {}
				delete[] tv;
#endif
			//	ifs->close();
				});
			//thread thr([&] {
					fs->send();
			//});

			//thr.detach();
			RELEASEIF
			return TJS_S_OK;
			//fs.callback(SimpleWeb::error_code());
			});
		/*putFunc(L"sendFileEx", [this](tTJSVariant* r, tjs_int n, tTJSVariant** p) {
			if (n < 1) return TJS_E_BADPARAMCOUNT;
			ttstr stor(*p[0]);
			IStream* tvpstr=TVPCreateIStream(stor, TJS_BS_READ);
			auto ISW = new UnbufferedOLEStreamBuf(tvpstr);
			istream* ifs = new istream(ISW);
			if (n == 2) {
				response->write((SimpleWeb::StatusCode)p[1]->AsInteger());
			}
			else if (n == 3) {
				auto headers = TDictToCMap(p[2]->AsObjectNoAddRef());
				ifs->seekg(0, ios::end);
				headers.emplace(std::pair<std::string, std::string>("Content-Length", std::to_string(ifs->tellg())));
				ifs->seekg(0);
				response->write((SimpleWeb::StatusCode)p[1]->AsInteger());
			}
			FileSender<T> fs(response, shared_ptr <istream>(ifs), [&,this](SimpleWeb::error_code ec) {
				tTJSVariant* tv = new tTJSVariant[2];
				if (ec) {
					tv[0] = ec.value();
					tv[1] = CStrToTStr(ec.message());
				}
				static ttstr evn(L"onSent");
				TVPPostEvent(this, this, evn, NULL, NULL, 2, tv);
				delete[] tv;
				});
			fs.send();
			RELEASEIF
				return TJS_S_OK;
			//fs.callback(SimpleWeb::error_code());
			});*/
		putProp<int>(L"autoClose", [this]()->int{
			return response->close_connection_after_response ? 1 : 0;
			},
			[this](int val) {
				response->close_connection_after_response = (val == 1);
			});

	}
	virtual ~KResponse() {
		UNREGISTALIVECONN;
	}

};

static ATOM WindowClass;
template<class T>
class KServer {
	typedef SimpleWeb::Server<T> ST;
	ST* server;
	//Constructors are differ in different protocol
	KServer(string crt, string key);
	KServer();
	iTJSDispatch2* objthis;
	
public:
	//if not using TVPPostEvent,use Windows message(experimental)
#ifndef USE_TVP_EVENT
	
	HWND hwnd;
	HWND createMessageWindow() {
		HINSTANCE hinst = ::GetModuleHandle(NULL);
		if (!WindowClass) {
			//make a window class
			WNDCLASSEXW wcex = {
				/*size*/sizeof(WNDCLASSEX), /*style*/0, /*proc*/WndProc, /*extra*/0L,0L, /*hinst*/hinst,
				/*icon*/NULL, /*cursor*/NULL, /*brush*/NULL, /*menu*/NULL,
				/*class*/L"KHttpServer Msg wnd class", /*smicon*/NULL };
			WindowClass = ::RegisterClassExW(&wcex);
			if (!WindowClass)
				TVPThrowExceptionMessage(TJS_W("register window class failed."));
		}
		//create a window for this instance to receive messahes
		hwnd = ::CreateWindowExW(0, (LPCWSTR)MAKELONG(WindowClass, 0), (ttstr(TJS_W("KHttpServer Msg"))+rand()).c_str(),
			0, 0, 0, 1, 1, HWND_MESSAGE, NULL, hinst, NULL);
		if (!hwnd) TVPThrowExceptionMessage(TJS_W("create message window failed."));
		//bind window with this FOR FUTURE USE.
		::SetWindowLong(hwnd, GWL_USERDATA, (LONG)this);
		return hwnd;
	}
	static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		if (msg == WM_ON_HTTP_REQUEST) {
			//prevent all exceptions
			__try {
				((MainEvent*)lp)->Invoke();
			}
			__except (1) {
			
			}
			__try {
				delete ((MainEvent*)lp);
			}
			__except (1) {}
			return 0;
		}
		return DefWindowProc(hwnd, msg, wp, lp);
	}
#endif
	//this method should be implemented seperately in different protocol
	static tjs_error TJS_INTF_METHOD Factory(KServer** inst, tjs_int n, tTJSVariant** p, iTJSDispatch2* objthis);
	//define server properties needed to set
	SC_PROPERTY_DEF(short,port)
	SC_PROPERTY_DEF(int,thread_pool_size)
	SC_PROPERTY_DEF(int,timeout_request);
	SC_PROPERTY_DEF(int,timeout_content);
	SC_PROPERTY_DEF(int,reuse_address);
	SC_PROPERTY_DEF(int,fast_open);
	void setaddress(ttstr ip) {
		server->config.address = TStrToCStr(ip);
	}
	ttstr getaddress() {
		 return CStrToTStr(server->config.address);
	}

	//initialize instance after being constructed
	//since different protocol uses different ctors
	void setDefaults() {
		server->config.address = "0.0.0.0";
		server->config.thread_pool_size = 2;
		//server->config.timeout_content = 5000;
#ifndef USE_TVP_EVENT
		createMessageWindow();
#endif
		server->default_callback = RequestEnvelop(objthis, L"onRequest", T);
		/*server->on_error = [this](shared_ptr<ST::Request> request,const SimpleWeb::error_code& ec) {
			std::lock_guard<mutex> mtx(globalParseMutex);
			ttstr erc(L"onError");
			tTJSVariant* tv =new tTJSVariant[3];
			tv[0] = getRequestParam<T>(request);
			if (ec) {
				tv[1] = ec.value();
				tv[2] = CStrToTStr(ec.message());
			}
			TVPPostEvent(objthis, objthis, erc, NULL, NULL, 3,tv ); 
			delete[] tv;
		};*/
	}
	static tjs_error TJS_INTF_METHOD setSpecificCallBack(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		if (n < 3) return TJS_E_BADPARAMCOUNT;
#ifdef USE_TVP_EVENT
		iTJSDispatch2* objcxt = TJSCreateDictionaryObject();
		objcxt->PropSet(TJS_MEMBERENSURE, L"onEvent", NULL, p[2], objcxt);
		if (p[1]->Type() == tvtString)
			self->server->resource[TStrToCStr(p[1]->AsStringNoAddRef())][TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, "onEvent", T);
		else
			self->server->default_resource[TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, "onEvent", T);
#else
		HWND hwnd = self->hwnd;
		if (p[1]->Type() == tvtString) {
			auto objcxt = p[2]->AsObjectClosure();
			self->server->resource[TStrToCStr(p[1]->AsStringNoAddRef())][TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, NULL, T);
		}
		else {
			auto objcxt = p[2]->AsObjectClosure();
			self->server->default_resource[TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, NULL, T);
		}
#endif
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD setDefaultCallBack(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		if (n < 2) return TJS_E_BADPARAMCOUNT;
#ifdef USE_TVP_EVENT
		iTJSDispatch2* objcxt = TJSCreateDictionaryObject();
		objcxt->PropSet(TJS_MEMBERENSURE,L"onEvent",NULL, p[1], objcxt);
		self->server->default_resource[TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt,"onEvent", T);
#else
		HWND hwnd = self->hwnd;
		auto objcxt = p[1]->AsObjectClosure();
		self->server->default_resource[TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, NULL, T);
#endif
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD start(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		self->server->start();
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD stop(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		self->server->stop();
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD getAliveConnectionCount(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		*r = tTJSVariant(aliveConnCount);
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD startAsync(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		thread server_thread([self]() {
			// Start server4
			self->server->start();
			});
		server_thread.detach();
		return TJS_S_OK;
	}
};
template<>
KServer<SimpleWeb::HTTPS>::KServer(string crt, string key) {
	try {
		server = new ST(crt, key);
	}
	catch (boost::system::error_code e) {
		TVPThrowExceptionMessage(CStrToTStr(e.message()).c_str());
	}
	catch (boost::system::system_error e) {
		TVPThrowExceptionMessage(CStrToTStr(e.code().message()).c_str());
	}
	catch (...) {
	
	}
	server->config.port = 443;
	setDefaults();
}
template<>
tjs_error TJS_INTF_METHOD KServer<SimpleWeb::HTTPS>::Factory(KServer** inst, tjs_int n, tTJSVariant** p, iTJSDispatch2* objthis) {
	if (n < 2)
		return TJS_E_BADPARAMCOUNT;
	ttstr crt(*p[0]);
	ttstr key(*p[1]);
	crt = TVPNormalizeStorageName(crt);
	TVPGetLocalName(crt);
	key = TVPNormalizeStorageName(key);
	TVPGetLocalName(key);
	if (inst) {
		KServer* self = *inst = new KServer<SimpleWeb::HTTPS>(TStrToCStr(crt), TStrToCStr(key));
		self->objthis = objthis;
	}
	return TJS_S_OK;
}
template<>
KServer<SimpleWeb::HTTP>::KServer() {
	server = new ST();
	server->config.port = 80;
	setDefaults();
}
template<>
tjs_error TJS_INTF_METHOD KServer<SimpleWeb::HTTP>::Factory(KServer** inst, tjs_int n, tTJSVariant** p, iTJSDispatch2* objthis) {
	if (inst) {
		KServer* self = *inst = new KServer<SimpleWeb::HTTP>();
		self->objthis = objthis;
	}
	return TJS_S_OK;
}
#define NATIVEPROP(X) NCB_PROPERTY(X,get##X,set##X)
#define REGISTALL(X) \
NCB_REGISTER_CLASS(X) {\
	CONSTRUCTOR;\
	FUNCTION(setSpecificCallBack);\
	FUNCTION(setDefaultCallBack);\
	FUNCTION(start);\
	FUNCTION(startAsync);\
	FUNCTION(stop);\
	NATIVEPROP(port);\
	FUNCTION(getAliveConnectionCount);\
	NATIVEPROP(thread_pool_size);\
	NATIVEPROP(timeout_request);\
	NATIVEPROP(timeout_content);\
	NATIVEPROP(reuse_address);\
	NATIVEPROP(fast_open);\
	NATIVEPROP(address);\
}
using HTTP = SimpleWeb::HTTP;
using HTTPS = SimpleWeb::HTTPS;
using KHttpsServer = KServer<HTTPS>;
REGISTALL(KHttpsServer)
using KHttpServer = KServer<HTTP>;
REGISTALL(KHttpServer)
void cof() {
	os = new ofstream();
	os->open(L"httpserv.log", ios::out, _SH_DENYWR);
}
NCB_PRE_REGIST_CALLBACK(cof);
void eof() {
	os->close();
	
}
NCB_POST_UNREGIST_CALLBACK(eof);