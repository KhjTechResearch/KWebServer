#include "client_https.hpp"
#include "Convert.hpp"
using namespace std;
//alias define
using HttpsClient = SimpleWeb::Client<SimpleWeb::HTTPS>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;
//Convert T(JS) Dictionary to std C(++) Multimap.
SimpleWeb::CaseInsensitiveMultimap TDictToCMap(iTJSDispatch2* obj) {
	SimpleWeb::CaseInsensitiveMultimap map;
	IterateObject(obj, [&map](tTJSVariant* key, tTJSVariant* val) {
		map.insert(std::pair<std::string, std::string>(TStrToCStr(key->AsStringNoAddRef()), TStrToCStr(val->AsStringNoAddRef())));
		});
	return map;
}
//Get T(JS) Dictionary from std C(++) Multimap.
iTJSDispatch2* CMapToTDict(SimpleWeb::CaseInsensitiveMultimap map) {
	iTJSDispatch2* obj = TJSCreateDictionaryObject();
	for (SimpleWeb::CaseInsensitiveMultimap::iterator it = map.begin(); it != map.end(); it++) {
		PutToObject(CStrToTStr((*it).first).c_str(), CStrToTStr((*it).second), obj);
	}
	return obj;
}
class KClient{
	typedef HttpsClient TC;
	TC * hc;
	iTJSDispatch2* objthis;
	int timeout;
public:
	static tjs_error TJS_INTF_METHOD Factory(KClient** inst, tjs_int n, tTJSVariant** p, iTJSDispatch2* objthis) {
		if (n < 2)
			return TJS_E_BADPARAMCOUNT;
		ttstr crt(*p[0]);
		ttstr key(*p[1]);
		crt = TVPNormalizeStorageName(crt);
		TVPGetLocalName(crt);
		key = TVPNormalizeStorageName(key);
		TVPGetLocalName(key);
		if (inst) {
			KClient* self = *inst = new KClient();
			self->objthis = objthis;
		}
		return TJS_S_OK;
	}
	void settimeout(int t) {
		timeout = t;
	}
	int gettimeout() {
		return timeout;
	}
	static tjs_error TJS_INTF_METHOD open(tTJSVariant* r, tjs_int n, tTJSVariant** p, KClient* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		if (n < 1) return TJS_E_BADPARAMCOUNT;
		self->close();
		self->hc = new TC(TStrToCStr(p[0]->AsStringNoAddRef()));
		return TJS_S_OK;
	}
	static tjs_error TJS_INTF_METHOD sendSync(tTJSVariant* r, tjs_int n, tTJSVariant** p, KClient* self) {
		if (!self) return TJS_E_NATIVECLASSCRASH;
		if (n <= 3) return TJS_E_BADPARAMCOUNT;
		self->hc->config.timeout = self->timeout;
		std::shared_ptr<TC::Response> res;
		if(p[3]->Type()==tvtString)
			res = self->hc->request(TStrToCStr(p[0]->AsStringNoAddRef()), TStrToCStr(p[1]->AsStringNoAddRef()), TStrToCStr(p[3]->AsStringNoAddRef()),TDictToCMap(p[2]->AsObjectNoAddRef()));
		else if(p[3]->Type()==tvtOctet)
			res = self->hc->request(TStrToCStr(p[0]->AsStringNoAddRef()), TStrToCStr(p[1]->AsStringNoAddRef()), TOctToCStr(p[3]->AsOctetNoAddRef()), TDictToCMap(p[2]->AsObjectNoAddRef())); 
		iTJSDispatch2* resp = TJSCreateDictionaryObject();
		PutToObject(L"status", CStrToTStr(res->status_code), resp);
		PutToObject(L"headers", CMapToTDict(res->header), resp);

		PutToObject(L"contentData", new FunctionCaller([=](tTJSVariant* r, tjs_int n, tTJSVariant** p)->tjs_error {*r = CStrToTOct(res->content.string()); return TJS_S_OK; }), resp);
		PutToObject(L"content", new FunctionCaller([=](tTJSVariant* r, tjs_int n, tTJSVariant** p)->tjs_error {*r = CStrToTStr(res->content.string()); return TJS_S_OK; }), resp);
		*r = tTJSVariant(resp, resp);
		return TJS_S_OK;
	}
	void close() {
		if (hc)
			delete hc;
		hc = NULL;
	}
};
#define NATIVEPROP(X) NCB_PROPERTY(X,get##X,set##X)
#define REGISTALL(X) \
NCB_REGISTER_CLASS(X) {\
	CONSTRUCTOR;\
	FUNCTION(open);\
	NATIVEPROP(timeout);\
}
REGISTALL(KClient)