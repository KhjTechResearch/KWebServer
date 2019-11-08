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
#include "Convert.hpp"
#include <math.h>
#include "IStreamStdWrapper.h"
using namespace std;
// 
using HttpsServer = SimpleWeb::Server<SimpleWeb::HTTPS>;
using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
//macros for definding setter and getters
#define HTTPSERVCONFIGI(X) void set##X(int val) {\
	server->config.##X = val;\
	}\
int get##X() {\
  return server->config.##X; \
  }
#define HTTPSERVCONFIGS(X)     \
  void set##X(ttstr val) {       \
    server->config.##X = TStrToCStr(val);  \
  }                            \
 ttstr get##X() {               \
    return ttstr(server->config.##X.c_str()); \
  }

static mutex globalParseMutex;//parsing mutex,avoid errors.
static mutex RefCountMutex;//counting reference mutex,avoid threading problems
static int aliveConnCount;//counts alive conections
extern tjs_int TVPPluginGlobalRefCount;
//define message
#define WM_HTTP_REQUEST WM_USER|0x3533
//define message
#define WM_HTTPS_REQUEST WM_USER|0x3534
//count connection macros
#define REGISTALIVECONN RefCountMutex.lock();aliveConnCount++;RefCountMutex.unlock()
#define UNREGISTALIVECONN RefCountMutex.lock();aliveConnCount--;RefCountMutex.unlock()

//define the call lambda
#define RequestEnvelop(X,N,T) RequestEnvelopDiffer(X,X,N,T)

//function to parse requests
template <class Y>
iTJSDispatch2* getRequestParam(shared_ptr<typename SimpleWeb::Server<Y>::Request> request) {
	iTJSDispatch2* r = TJSCreateDictionaryObject();
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
//event call lambda macro
#define RequestEnvelopDiffer(X,Y,N,T) [=](shared_ptr <SimpleWeb::Server<##T>::Response> res, shared_ptr <SimpleWeb::Server<##T>::Request> req)\
 {\
globalParseMutex.lock();\
/*::PostMessage(msghwnd, WM_##T_REQUEST,0, (LPARAM)rr);*/\
ttstr mn(N);\
tTJSVariant *arg=new tTJSVariant[2];\
auto itq= getRequestParam<T>(req);\
auto its =new  KResponse<T>(res);\
arg[0] =itq;\
arg[1] = its;\
/*Y->FuncCall(NULL,NULL,NULL,NULL,2,&arg,X);*/\
TVPPostEvent(X, Y,mn,rand(),NULL/*TVP_EPT_EXCLUSIVE*/, 2,arg);\
its->Release();\
itq->Release();\
delete[] arg;\
globalParseMutex.unlock();\
 }
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
				tTJSVariant* tv = new tTJSVariant[2];
				if (ec) {
					tv[0] = ec.value();
					tv[1] = CStrToTStr(ec.message());
				}
				static ttstr evn(L"onSent");
				TVPPostEvent(this, this, evn, NULL, NULL, 2, tv);
				delete[] tv;
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
			ifs->open(stor.c_str(), ios::binary, _SH_DENYNO);//open file
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


template<class T>
class KServer {
	typedef SimpleWeb::Server<T> ST;
	ST* server;
	KServer(string crt, string key);
	KServer();
	iTJSDispatch2* objthis;
public:
	//added for multithreading
#ifdef USE_WM
	static ATOM WindowClass;
	HWND createMessageWindow() {
		HINSTANCE hinst = ::GetModuleHandle(NULL);
		if (!WindowClass) {
			//init a message receiver
			WNDCLASSEXW wcex = {
				/*size*/sizeof(WNDCLASSEX), /*style*/0, /*proc*/WndProc, /*extra*/0L,0L, /*hinst*/hinst,
				/*icon*/NULL, /*cursor*/NULL, /*brush*/NULL, /*menu*/NULL,
				/*class*/L"KHttpServer Msg wnd class", /*smicon*/NULL };
			WindowClass = ::RegisterClassExW(&wcex);
			if (!WindowClass)
				TVPThrowExceptionMessage(TJS_W("register window class failed."));
		}
		HWND hwnd = ::CreateWindowExW(0, (LPCWSTR)MAKELONG(WindowClass, 0), TJS_W("KHttpServer Msg"),
			0, 0, 0, 1, 1, HWND_MESSAGE, NULL, hinst, NULL);
		if (!hwnd) TVPThrowExceptionMessage(TJS_W("create message window failed."));
		::SetWindowLong(hwnd, GWL_USERDATA, (LONG)this);
		return hwnd;
	}
	static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
		if (msg == WM_HTTP_REQUEST) {
			KServer* self = (KServer*)(::GetWindowLong(hwnd, GWL_USERDATA));
			PwRequestResponse* rr = (PwRequestResponse*)lp;
			if (self && rr) self->onRequest(rr);
			if (rr) rr->done();
			return 0;
		}
		return DefWindowProc(hwnd, msg, wp, lp);
	}
#endif
	static tjs_error TJS_INTF_METHOD Factory(KServer** inst, tjs_int n, tTJSVariant** p, iTJSDispatch2* objthis);
	HTTPSERVCONFIGI(port);
	HTTPSERVCONFIGI(thread_pool_size);
	HTTPSERVCONFIGI(timeout_request);
	HTTPSERVCONFIGI(timeout_content);
	HTTPSERVCONFIGI(reuse_address);
	HTTPSERVCONFIGI(fast_open);
	HTTPSERVCONFIGS(address);
	void setDefaults() {
		server->config.address = "0.0.0.0";
		server->config.thread_pool_size = 2;
		//server->config.timeout_content = 5000;
		server->default_callback = RequestEnvelop(objthis, L"onRequest", T);
		/*server->on_error = [this](shared_ptr<ST::Request> request,const SimpleWeb::error_code& ec) {
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
		iTJSDispatch2* objcxt = TJSCreateDictionaryObject();
		objcxt->PropSet(TJS_MEMBERENSURE, L"onEvent", NULL, p[2], objcxt);
		if (p[1]->Type() == tvtString)
			self->server->resource[TStrToCStr(p[1]->AsStringNoAddRef())][TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, "onEvent", T);
		else
			self->server->default_resource[TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt, "onEvent", T);
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD setDefaultCallBack(tTJSVariant* r, tjs_int n, tTJSVariant** p, KServer* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		if (n < 2) return TJS_E_BADPARAMCOUNT;
		iTJSDispatch2* objcxt = TJSCreateDictionaryObject();
		objcxt->PropSet(TJS_MEMBERENSURE,L"onEvent",NULL, p[1], objcxt);
		self->server->default_resource[TStrToCStr(p[0]->AsStringNoAddRef())] = RequestEnvelop(objcxt,"onEvent", T);
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

#ifdef HAVE_MAIN_MET
int main() {
  // HTTPS-server at port 8080 using 1 thread
  // Unless you do more heavy non-threaded processing in the resources,
  // 1 thread is usually faster than several threads
  HttpsServer server("server.crt", "server.key");
  server.config.port = 433;
  server.config.address = "0.0.0.0";
  server.config.thread_pool_size = 2;

  // Add resources using path-regex and method-string, and an anonymous function
  // POST-example for the path /string, responds the posted string
  server.resource["^/string$"]["POST"] = [](shared_ptr<HttpsServer::Response> response, shared_ptr<HttpsServer::Request> request){
    // Retrieve string:
    auto content = request->content.string();
    // request->content.string() is a convenience function for:
    // stringstream ss;
    // ss << request->content.rdbuf();
    // auto content=ss.str();

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;
	
    // Alternatively, use one of the convenience functions, for instance:
    // response->write(content);
  };

  // POST-example for the path /json, responds firstName+" "+lastName from the posted json
  // Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
  // Example posted json:
  // {
  //   "firstName": "John",
  //   "lastName": "Smith",
  //   "age": 25
  // }
  server.resource["^/json$"]["POST"] = [](shared_ptr<HttpsServer::Response> response, shared_ptr<HttpsServer::Request> request) {
    try {
      ptree pt;
      read_json(request->content, pt);

      auto name = pt.get<string>("firstName") + " " + pt.get<string>("lastName");

      *response << "HTTP/1.1 200 OK\r\n"
                << "Content-Length: " << name.length() << "\r\n\r\n"
                << name;
    }
    catch(const exception &e) {
      *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n"
                << e.what();
    }


    // Alternatively, using a convenience function:
    // try {
    //     ptree pt;
    //     read_json(request->content, pt);

    //     auto name=pt.get<string>("firstName")+" "+pt.get<string>("lastName");
    //     response->write(name);
    // }
    // catch(const exception &e) {
    //     response->write(SimpleWeb::StatusCode::client_error_bad_request, e.what());
    // }
  };

  // GET-example for the path /info
  // Responds with request-information
  server.resource["^/info$"]["GET"] = [](shared_ptr<HttpsServer::Response> response, shared_ptr<HttpsServer::Request> request) {
    stringstream stream;
    stream << "<h1>Request from " << request->remote_endpoint().address().to_string() << ":" << request->remote_endpoint().port() << "</h1>";

    stream << request->method << " " << request->path << " HTTP/" << request->http_version;

    stream << "<h2>Query Fields</h2>";
    auto query_fields = request->parse_query_string();
    for(auto &field : query_fields)
      stream << field.first << ": " << field.second << "<br>";

    stream << "<h2>Header Fields</h2>";
    for(auto &field : request->header)
      stream << field.first << ": " << field.second << "<br>";

    response->write(stream);
  };

  // GET-example for the path /match/[number], responds with the matched string in path (number)
  // For instance a request GET /match/123 will receive: 123
  server.resource["^/match/([0-9]+)$"]["GET"] = [](shared_ptr<HttpsServer::Response> response, shared_ptr<HttpsServer::Request> request) {
    response->write(request->path_match[1].str());
  };

  // GET-example simulating heavy work in a separate thread
  server.resource["^/work$"]["GET"] = [](shared_ptr<HttpsServer::Response> response, shared_ptr<HttpsServer::Request> /*request*/) {
    thread work_thread([response] {
      this_thread::sleep_for(chrono::seconds(5));
      response->write("Work done");
    });
    work_thread.detach();
  };

  // Default GET-example. If no other matches, this anonymous function will be called.
  // Will respond with content in the web/-directory, and its subdirectories.
  // Default file: index.html
  // Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
  server.default_resource["GET"] = [](shared_ptr<HttpsServer::Response> response, shared_ptr<HttpsServer::Request> request) {
    try {
      auto web_root_path = boost::filesystem::canonical("web");
      auto path = boost::filesystem::canonical(web_root_path / request->path);
      // Check if path is within web_root_path
      if(distance(web_root_path.begin(), web_root_path.end()) > distance(path.begin(), path.end()) ||
         !equal(web_root_path.begin(), web_root_path.end(), path.begin()))
        throw invalid_argument("path must be within root path");
      if(boost::filesystem::is_directory(path))
        path /= "index.html";

      SimpleWeb::CaseInsensitiveMultimap header;

      // Uncomment the following line to enable Cache-Control
      // header.emplace("Cache-Control", "max-age=86400");

#ifdef HAVE_OPENSSL
//    Uncomment the following lines to enable ETag
//    {
//      ifstream ifs(path.string(), ifstream::in | ios::binary);
//      if(ifs) {
//        auto hash = SimpleWeb::Crypto::to_hex_string(SimpleWeb::Crypto::md5(ifs));
//        header.emplace("ETag", "\"" + hash + "\"");
//        auto it = request->header.find("If-None-Match");
//        if(it != request->header.end()) {
//          if(!it->second.empty() && it->second.compare(1, hash.size(), hash) == 0) {
//            response->write(SimpleWeb::StatusCode::redirection_not_modified, header);
//            return;
//          }
//        }
//      }
//      else
//        throw invalid_argument("could not read file");
//    }
#endif

      auto ifs = make_shared<ifstream>();
      ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);

      if(*ifs) {
        auto length = ifs->tellg();
        ifs->seekg(0, ios::beg);

        header.emplace("Content-Length", to_string(length));
        response->write(header);

        // Trick to define a recursive function within this scope (for example purposes)
        class FileServer {
        public:
          static void read_and_send(const shared_ptr<HttpsServer::Response> &response, const shared_ptr<ifstream> &ifs) {
            // Read and send 128 KB at a time
            static vector<char> buffer(131072); // Safe when server is running on one thread
            streamsize read_length;
            if((read_length = ifs->read(&buffer[0], static_cast<streamsize>(buffer.size())).gcount()) > 0) {
              response->write(&buffer[0], read_length);
              if(read_length == static_cast<streamsize>(buffer.size())) {
                response->send([response, ifs](const SimpleWeb::error_code &ec) {
                  if(!ec)
                    read_and_send(response, ifs);
                  else
                    cerr << "Connection interrupted" << endl;
                });
              }
            }
          }
        };
        FileServer::read_and_send(response, ifs);
      }
      else
        throw invalid_argument("could not read file");
    }
    catch(const exception &e) {
      response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path + ": " + e.what());
    }
  };

  server.on_error = [](shared_ptr<HttpsServer::Request> /*request*/, const SimpleWeb::error_code & /*ec*/) {
    // Handle errors here
    // Note that connection timeouts will also call this handle with ec set to SimpleWeb::errc::operation_canceled
  };

  thread server_thread([&server]() {
    // Start server4
    server.start();
  });

  // Wait for server to start so that the client can connect
  this_thread::sleep_for(chrono::seconds(1));

  // Client examples
  // Second create() parameter set to false: no certificate verification
  HttpsClient client("localhost:8080", false);

  string json_string = "{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";

  // Synchronous request examples
  try {
    cout << "Example GET request to https://localhost:8080/match/123" << endl;
    auto r1 = client.request("GET", "/match/123");
    cout << "Response content: " << r1->content.rdbuf() << endl << endl; // Alternatively, use the convenience function r1->content.string()

    cout << "Example POST request to https://localhost:8080/string" << endl;
    auto r2 = client.request("POST", "/string", json_string);
    cout << "Response content: " << r2->content.rdbuf() << endl << endl;
  }
  catch(const SimpleWeb::system_error &e) {
    cerr << "Client request error: " << e.what() << endl;
  }

  // Asynchronous request example
  cout << "Example POST request to https://localhost:8080/json" << endl;
  client.request("POST", "/json", json_string, [](shared_ptr<HttpsClient::Response> response, const SimpleWeb::error_code &ec) {
    if(!ec)
      cout << "Response content: " << response->content.rdbuf() << endl;
  });
  client.io_service->run();

  server_thread.join();
}

#endif