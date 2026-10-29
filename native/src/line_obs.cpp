#include "line_obs.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <winhttp.h>
#endif

// Protocol source: evex-dev/linejs, commit
// 4fc512ab8c19a2490ca73bb71777beb775ca3ae3, packages/linejs/base/request/
// legy.ts, auth_token.ts and service/talk/mod.ts. Only the read-only
// acquireEncryptedAccessToken(featureType=2) RPC is implemented here.
namespace atlas {
namespace {
constexpr size_t responseLimit=256*1024;
constexpr std::string_view method="acquireEncryptedAccessToken";
constexpr std::array<unsigned char,16> iv={78,9,72,62,56,245,255,114,128,18,123,158,251,92,45,51};
void wipe(void* value,size_t size)noexcept{
#ifdef _WIN32
 if(size)SecureZeroMemory(value,size);
#else
 volatile unsigned char* p=static_cast<volatile unsigned char*>(value);while(size--)*p++=0;
#endif
}
struct SecretString {std::string& value;~SecretString(){eraseLineKey(value);}};
struct SecretWide {std::wstring& value;~SecretWide(){wipe(value.data(),value.size()*sizeof(wchar_t));}};
struct SecretBytes {std::vector<unsigned char> value;explicit SecretBytes(size_t n=0):value(n){}~SecretBytes(){wipe(value.data(),value.size());}};
[[noreturn]] void protocolError(){throw DownloadUnavailable("LINE 附件授權回應格式不相容，未保存任何憑證。");}
void bounded(std::string_view value,size_t limit=responseLimit){if(value.size()>limit)protocolError();}
bool safeToken(std::string_view value){return !value.empty()&&value.size()<=8192&&std::all_of(value.begin(),value.end(),[](unsigned char c){return c>=33&&c<=126;});}
bool safeApplication(std::string_view value){return !value.empty()&&value.size()<=1024&&std::all_of(value.begin(),value.end(),[](unsigned char c){return c=='\t'||(c>=32&&c<=126);});}
uint32_t rotate(uint32_t v,unsigned count){return(v<<count)|(v>>(32-count));}
uint32_t little(std::string_view bytes,size_t offset){return uint32_t(static_cast<unsigned char>(bytes[offset]))|uint32_t(static_cast<unsigned char>(bytes[offset+1]))<<8|uint32_t(static_cast<unsigned char>(bytes[offset+2]))<<16|uint32_t(static_cast<unsigned char>(bytes[offset+3]))<<24;}
void u16(std::string& bytes,size_t value){if(value>65535)protocolError();bytes.push_back(char(value>>8));bytes.push_back(char(value));}
void field(std::string& bytes,std::string_view key,std::string_view value){u16(bytes,key.size());bytes.append(key);u16(bytes,value.size());bytes.append(value);}
class Reader {
 std::string_view data;size_t at=0,budget=0;
 public:
 explicit Reader(std::string_view value):data(value){bounded(value);}
 size_t remaining()const{return data.size()-at;}
 unsigned byte(){if(at>=data.size())protocolError();return static_cast<unsigned char>(data[at++]);}
 uint64_t varint(){uint64_t value=0;for(unsigned shift=0;shift<64;shift+=7){auto b=byte();if(shift==63&&(b&0xfe))protocolError();value|=uint64_t(b&127)<<shift;if(!(b&128))return value;}protocolError();}
 std::string bytes(size_t length){if(length>remaining())protocolError();auto result=std::string(data.substr(at,length));at+=length;return result;}
 void skipBytes(size_t length){if(length>remaining())protocolError();at+=length;}
 std::string binary(){auto length=varint();if(length>8192)protocolError();return bytes(static_cast<size_t>(length));}
 void skip(unsigned type,unsigned depth=0){
  if(depth>24||++budget>4096)protocolError();
  switch(type){
   case 1:case 2:return;
   case 3:byte();return;
   case 4:case 5:case 6:varint();return;
   case 7:skipBytes(8);return;
   case 8:{auto size=varint();if(size>remaining())protocolError();skipBytes(static_cast<size_t>(size));return;}
   case 9:case 10:{auto head=byte();auto size=head>>4;if(size==15)size=static_cast<unsigned>(varint());if(size>4096)protocolError();for(unsigned i=0;i<size;++i)element(head&15,depth+1);return;}
   case 11:{auto size=varint();if(size>4096)protocolError();if(!size)return;auto types=byte();for(size_t i=0;i<size;++i){element(types>>4,depth+1);element(types&15,depth+1);}return;}
   case 12:{unsigned count=0;while(true){auto head=byte();if(!head)return;if(++count>4096)protocolError();if(!(head>>4))varint();skip(head&15,depth+1);}}
   default:protocolError();
  }
 }
 void element(unsigned type,unsigned depth){if(type==1||type==2){auto value=byte();if(value!=1&&value!=2)protocolError();}else skip(type,depth);}
};
std::string normalizeObsToken(std::string value){
 SecretString cleanup{value};
 const auto split=value.find(char(0x1e));
 auto selected=std::string_view(value);
 if(split!=std::string::npos){auto second=value.find(char(0x1e),split+1);selected=selected.substr(split+1,second==std::string::npos?second:second-split-1);}
 if(!safeToken(selected))protocolError();
 return std::string(selected);
}
#ifdef _WIN32
void cryptoStatus(NTSTATUS status){if(status<0)throw DownloadUnavailable("Windows 無法完成 LINE 附件授權的原生加密。");}
struct Algorithm {BCRYPT_ALG_HANDLE handle=nullptr;~Algorithm(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}};
struct CryptoKey {BCRYPT_KEY_HANDLE handle=nullptr;~CryptoKey(){if(handle)BCryptDestroyKey(handle);}};
std::string aes(std::string_view key,std::string_view input,bool encrypt){
 if(key.size()!=16||input.size()>responseLimit)protocolError();
 Algorithm algorithm;cryptoStatus(BCryptOpenAlgorithmProvider(&algorithm.handle,BCRYPT_AES_ALGORITHM,nullptr,0));
 cryptoStatus(BCryptSetProperty(algorithm.handle,BCRYPT_CHAINING_MODE,reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),sizeof(BCRYPT_CHAIN_MODE_CBC),0));
 DWORD objectSize=0,returned=0;cryptoStatus(BCryptGetProperty(algorithm.handle,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof objectSize,&returned,0));
 if(!objectSize||objectSize>65536)protocolError();SecretBytes object(objectSize),secret(key.size());std::memcpy(secret.value.data(),key.data(),key.size());
 CryptoKey handle;cryptoStatus(BCryptGenerateSymmetricKey(algorithm.handle,&handle.handle,object.value.data(),objectSize,secret.value.data(),static_cast<ULONG>(secret.value.size()),0));
 std::array<unsigned char,16> nonce=iv;ULONG required=0;
 auto operation=encrypt?BCryptEncrypt:BCryptDecrypt;
 cryptoStatus(operation(handle.handle,reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),static_cast<ULONG>(input.size()),nullptr,nonce.data(),static_cast<ULONG>(nonce.size()),nullptr,0,&required,BCRYPT_BLOCK_PADDING));
 if(required>responseLimit+16)protocolError();SecretBytes output(required);nonce=iv;
 cryptoStatus(operation(handle.handle,reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),static_cast<ULONG>(input.size()),nullptr,nonce.data(),static_cast<ULONG>(nonce.size()),output.value.data(),required,&returned,BCRYPT_BLOCK_PADDING));
 return std::string(reinterpret_cast<const char*>(output.value.data()),returned);
}
std::string base64Bytes(const void* bytes,size_t length){return base64(std::string_view(static_cast<const char*>(bytes),length));}
std::string rsaKey(std::string_view key){
 // Public LINE transport key, not an account credential. It is pinned to the
 // primary protocol source above; no remote key discovery is performed.
 static constexpr char encoded[]="MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAsMC6HAYeMq4R59e2yRw6W1OWT2t9aepiAp4fbSCXzRj7A29BOAFAvKlzAub4oxN13Nt8dbcB+ICAufyDnN5Nd3+vXgDxEXZ/sx2/wuFbC3B3evSNKR4hKcs80suRs8aL6EeWi+bAU2oYIc78BbqhNzx0WCzZSJbMBFw1VlsU/HQ/XdiUufopl5QSa0S246XXmwJmmXRO0v7bNvrxaNV0cbviGkOvTlBt1+RerIFHMTw3SwLDnCOolTz3CuE5V2OrPZCmC0nlmPRzwUfxoxxs/6qFdpZNoORH/s5mQenSyqPkmH8TBOlHJWPH3eN1k6aZIlK5S54mcUb/oNRRq9wD1wIDAQAB";
 DWORD length=0;if(!CryptStringToBinaryA(encoded,0,CRYPT_STRING_BASE64,nullptr,&length,nullptr,nullptr)||length>4096)protocolError();std::vector<unsigned char> der(length);
 if(!CryptStringToBinaryA(encoded,0,CRYPT_STRING_BASE64,der.data(),&length,nullptr,nullptr))protocolError();
 CERT_PUBLIC_KEY_INFO* info=nullptr;DWORD infoSize=0;
 if(!CryptDecodeObjectEx(X509_ASN_ENCODING,X509_PUBLIC_KEY_INFO,der.data(),length,CRYPT_DECODE_ALLOC_FLAG,nullptr,&info,&infoSize))protocolError();
 struct Free{void* value;~Free(){LocalFree(value);}}free{info};CryptoKey imported;
 if(!CryptImportPublicKeyInfoEx2(X509_ASN_ENCODING,info,0,nullptr,&imported.handle))protocolError();
 BCRYPT_OAEP_PADDING_INFO padding{BCRYPT_SHA1_ALGORITHM,nullptr,0};ULONG required=0;
 cryptoStatus(BCryptEncrypt(imported.handle,reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),static_cast<ULONG>(key.size()),&padding,nullptr,0,nullptr,0,&required,BCRYPT_PAD_OAEP));
 if(required!=256)protocolError();std::vector<unsigned char> output(required);
 cryptoStatus(BCryptEncrypt(imported.handle,reinterpret_cast<PUCHAR>(const_cast<char*>(key.data())),static_cast<ULONG>(key.size()),&padding,nullptr,0,output.data(),required,&required,BCRYPT_PAD_OAEP));
 return "0008"+base64Bytes(output.data(),required);
}
std::wstring wide(std::string_view value){
 if(value.size()>16384)protocolError();int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);if(count<=0)protocolError();std::wstring result(count,0);if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),result.data(),count)!=count)protocolError();return result;
}
struct Internet {HINTERNET handle=nullptr;explicit Internet(HINTERNET value):handle(value){if(!handle)throw DownloadUnavailable("LINE 附件授權連線無法啟動。");}~Internet(){WinHttpCloseHandle(handle);}operator HINTERNET()const{return handle;}};
std::string post(bool encrypted,std::string_view application,std::string_view token,std::string_view body,std::string_view key,Control& control){
 control.check();if(!safeApplication(application)||!safeToken(token))throw DownloadUnavailable("LINE 工作階段缺少有效的原生附件授權資訊。");
 auto first=application.find('\t'),second=first==std::string_view::npos?first:application.find('\t',first+1);
 if(first==std::string_view::npos||second==std::string_view::npos||second-first>50)protocolError();
 auto version=application.substr(first+1,second-first-1);if(version.empty()||!std::all_of(version.begin(),version.end(),[](unsigned char c){return(c>='0'&&c<='9')||c=='.';}))protocolError();
 auto agent=wide("Line/"+std::string(version));
 Internet session(WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
 if(!WinHttpSetTimeouts(session,5000,5000,5000,1000))protocolError();
 Internet connection(WinHttpConnect(session,encrypted?L"gf.line.naver.jp":L"legy.line-apps.com",INTERNET_DEFAULT_HTTPS_PORT,0));
 Internet request(WinHttpOpenRequest(connection,L"POST",encrypted?L"/enc":L"/S4",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));
 DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
 if(!WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof policy)||!WinHttpSetOption(request,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof disabled))protocolError();
 std::string headers="content-type: application/x-thrift\r\naccept: application/x-thrift\r\naccept-encoding: identity\r\nx-lpv: 1\r\nx-lal: ja_JP\r\nx-lhm: POST\r\nx-line-application: ";SecretString eraseHeaders{headers};headers.append(application);headers+="\r\n";
 if(encrypted){headers+="x-le: 7\r\nx-lap: 5\r\nx-lcs: ";auto wrapped=rsaKey(key);SecretString eraseWrapped{wrapped};headers+=wrapped;headers+="\r\n";}
 else{headers+="x-line-access: ";headers.append(token);headers+="\r\n";}
 auto nativeHeaders=wide(headers);SecretWide eraseNative{nativeHeaders};
 if(body.size()>responseLimit)protocolError();
 if(!WinHttpSendRequest(request,nativeHeaders.c_str(),static_cast<DWORD>(nativeHeaders.size()),const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0)||!WinHttpReceiveResponse(request,nullptr)){control.check();throw DownloadUnavailable("LINE 附件授權暫時無法取得，未嘗試其他帳號或登入方式。");}
 control.check();DWORD status=0,size=sizeof status;
 if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)throw DownloadUnavailable("LINE 附件授權遭拒或已失效，請在 LINE 開啟附件後重新連接。");
 std::string result;SecretString eraseResult{result};std::array<char,16384> chunk{};
 struct WipeChunk{std::array<char,16384>& value;~WipeChunk(){wipe(value.data(),value.size());}}wipeChunk{chunk};
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
 while(true){control.check();if(std::chrono::steady_clock::now()>deadline)throw DownloadUnavailable("LINE 附件授權回應逾時。");DWORD count=0;if(!WinHttpReadData(request,chunk.data(),static_cast<DWORD>(chunk.size()),&count)){control.check();throw DownloadUnavailable("LINE 附件授權回應未完成。");}if(!count)break;if(count>responseLimit-result.size())protocolError();result.append(chunk.data(),count);}
 control.check();return std::string(result);
}
bool encryptedAccess(std::string_view token){
 if(token.find(':')!=std::string_view::npos)return true;
 auto first=token.find('.');return first!=std::string_view::npos&&token.find('.',first+1)!=std::string_view::npos;
}
std::string resolvedToken(std::string_view token){
 // A MID-prefixed JWT is observed in some LINE credential contexts. Its JWT
 // payload is the actual HTTP token, as specified by linejs resolveLineAccessToken.
 auto colon=token.find(':');if(colon!=std::string_view::npos){auto tail=token.substr(colon+1);auto dot=tail.find('.');if(dot!=std::string_view::npos&&tail.find('.',dot+1)!=std::string_view::npos&&tail.starts_with("eyJ"))return std::string(tail);}
 // Existing primary access tokens are transmitted unchanged. Bare long-term
 // auth keys are deliberately not converted into fresh credentials here.
 return std::string(token);
}
#endif
}

std::string lineObsTokenRequest(){
 std::string result;result.push_back(char(0x82));result.push_back(char(0x21));result.push_back(0);result.push_back(char(method.size()));result.append(method);result.append("\x25\x04\x00",3);return result;
}
std::string parseLineObsTokenReply(std::string_view bytes){
 Reader input(bytes);if(input.byte()!=0x82||input.byte()!=0x41||input.varint()!=0)protocolError();
 if(input.binary()!=method)protocolError();int64_t previous=0;std::string result;SecretString erase{result};bool found=false;
 while(true){auto header=input.byte();if(!header)break;unsigned type=header&15,delta=header>>4;int64_t id;
  if(delta)id=previous+delta;else{auto encoded=input.varint();if(encoded>65535)protocolError();id=int64_t(encoded>>1)^-int64_t(encoded&1);}
  previous=id;if(id==1)throw DownloadUnavailable("LINE 拒絕附件授權要求，未改用其他帳號或憑證。");
  if(id==0){if(found||type!=8)protocolError();result=input.binary();found=true;}else input.skip(type);
 }
 if(input.remaining()||!found)protocolError();return normalizeObsToken(result);
}
uint32_t lineLegyHash(std::string_view bytes,uint32_t seed){
 constexpr uint32_t p1=0x9e3779b1,p2=0x85ebca77,p3=0xc2b2ae3d,p4=0x27d4eb2f,p5=0x165667b1;
 auto round=[](uint32_t accumulator,uint32_t value){return rotate(accumulator+value*p2,13)*p1;};size_t at=0;uint32_t hash;
 if(bytes.size()>=16){uint32_t a=seed+p1+p2,b=seed+p2,c=seed,d=seed-p1;do{a=round(a,little(bytes,at));at+=4;b=round(b,little(bytes,at));at+=4;c=round(c,little(bytes,at));at+=4;d=round(d,little(bytes,at));at+=4;}while(at<=bytes.size()-16);hash=rotate(a,1)+rotate(b,7)+rotate(c,12)+rotate(d,18);}else hash=seed+p5;
 hash+=static_cast<uint32_t>(bytes.size());while(at+4<=bytes.size()){hash=rotate(hash+little(bytes,at)*p3,17)*p4;at+=4;}while(at<bytes.size())hash=rotate(hash+static_cast<unsigned char>(bytes[at++])*p5,11)*p1;
 hash^=hash>>15;hash*=p2;hash^=hash>>13;hash*=p3;hash^=hash>>16;return hash;
}
std::array<unsigned char,4> lineLegyMac(std::string_view key,std::string_view ciphertext){
 if(key.size()!=16)protocolError();bounded(ciphertext);
 std::string inner(16,0),outer(16,0);SecretString eraseInner{inner},eraseOuter{outer};for(size_t i=0;i<16;++i){inner[i]=char(static_cast<unsigned char>(key[i])^0x36);outer[i]=char(static_cast<unsigned char>(key[i])^0x5c);}inner.append(ciphertext);auto first=lineLegyHash(inner);for(unsigned shift:{24u,16u,8u,0u})outer.push_back(char(first>>shift));auto last=lineLegyHash(outer);return {static_cast<unsigned char>(last>>24),static_cast<unsigned char>(last>>16),static_cast<unsigned char>(last>>8),static_cast<unsigned char>(last)};
}
std::string lineLegyPlaintext(std::string_view token,std::string_view body){
 if(!safeToken(token))protocolError();bounded(body);std::string headers;SecretString erase{headers};u16(headers,2);field(headers,"x-lpqs","/S4");field(headers,"x-lt",token);std::string result(1,char(7));u16(result,headers.size());result+=headers;result.append(body);return result;
}
LineLegyReply::~LineLegyReply(){for(auto& entry:headers)eraseLineKey(entry.second);eraseLineKey(body);}
LineLegyReply parseLineLegyReply(std::string_view bytes){
 bounded(bytes);if(bytes.size()<5||static_cast<unsigned char>(bytes[0])!=7)protocolError();size_t at=1;
 auto number=[&]{if(at+2>bytes.size())protocolError();auto value=static_cast<unsigned char>(bytes[at])*256u+static_cast<unsigned char>(bytes[at+1]);at+=2;return value;};
 auto length=number();size_t end=at+length;if(end>bytes.size()||length<2)protocolError();auto count=number();if(count>32)protocolError();LineLegyReply reply;
 auto read=[&](size_t size){if(at>end||size>end-at)protocolError();auto value=std::string(bytes.substr(at,size));at+=size;return value;};
 auto fieldLength=[&]{if(at>end||end-at<2)protocolError();return number();};
 for(unsigned i=0;i<count;++i){auto key=read(fieldLength()),value=read(fieldLength());if(key.empty()||!reply.headers.emplace(std::move(key),std::move(value)).second)protocolError();}
 if(at!=end)protocolError();auto status=reply.headers.find("x-lc");if(status!=reply.headers.end()&&status->second!="200")throw DownloadUnavailable("LINE 附件授權遭拒，未改用其他帳號。");reply.body=std::string(bytes.substr(end));return reply;
}
std::string sealLineLegy(std::string_view key,std::string_view plaintext){
#ifdef _WIN32
 auto result=aes(key,plaintext,true);auto mac=lineLegyMac(key,result);result.append(reinterpret_cast<const char*>(mac.data()),mac.size());return result;
#else
 (void)key;(void)plaintext;throw CapabilityError("原生 LINE 附件授權加密只支援 Windows。");
#endif
}
std::string openLineLegy(std::string_view key,std::string_view ciphertext){
#ifdef _WIN32
 if(ciphertext.size()<20||ciphertext.size()%16!=4)protocolError();auto body=ciphertext.substr(0,ciphertext.size()-4);auto mac=lineLegyMac(key,body);unsigned difference=0;for(size_t i=0;i<4;++i)difference|=mac[i]^static_cast<unsigned char>(ciphertext[body.size()+i]);if(difference)throw IntegrityError("LINE 附件授權回應完整性檢查失敗。");return aes(key,body,false);
#else
 (void)key;(void)ciphertext;throw CapabilityError("原生 LINE 附件授權加密只支援 Windows。");
#endif
}
std::string wrapLineLegyKey(std::string_view key){
#ifdef _WIN32
 if(key.size()!=16)protocolError();return rsaKey(key);
#else
 (void)key;throw CapabilityError("原生 LINE 附件授權加密只支援 Windows。");
#endif
}
std::string acquireLineObsToken(const std::shared_ptr<LineSession>& session,Control& control){
 control.check();if(!session||!session->current())throw DownloadUnavailable("未取得與所選資料庫相符的 LINE 附件工作階段，請先在 LINE 開啟附件再重新連接。");
 if(!session->obsAccessToken().empty()){if(!safeToken(session->obsAccessToken()))protocolError();return std::string(session->obsAccessToken());}
 if(!safeToken(session->accessToken())||!safeApplication(session->application()))throw DownloadUnavailable("LINE 工作階段缺少可辨識的附件授權資訊。");
#ifdef _WIN32
 auto token=resolvedToken(session->accessToken());SecretString eraseToken{token};auto rpc=lineObsTokenRequest();bool encrypted=encryptedAccess(token);std::string response;SecretString eraseResponse{response};
 if(encrypted){SecretBytes key(16);cryptoStatus(BCryptGenRandom(nullptr,key.value.data(),static_cast<ULONG>(key.value.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG));std::string_view secret(reinterpret_cast<const char*>(key.value.data()),key.value.size());auto plaintext=lineLegyPlaintext(token,rpc);SecretString erasePlaintext{plaintext};auto request=sealLineLegy(secret,plaintext);response=post(true,session->application(),token,request,secret,control);auto clear=openLineLegy(secret,response);SecretString eraseClear{clear};auto reply=parseLineLegyReply(clear);SecretString eraseReply{reply.body};for(auto& entry:reply.headers)eraseLineKey(entry.second);control.check();if(!session->current())throw DownloadUnavailable("LINE 工作階段已結束，未繼續取得附件授權。");return parseLineObsTokenReply(reply.body);}
 response=post(false,session->application(),token,rpc,{},control);control.check();if(!session->current())throw DownloadUnavailable("LINE 工作階段已結束，未繼續取得附件授權。");return parseLineObsTokenReply(response);
#else
 throw CapabilityError("原生 LINE 附件工作階段僅支援 Windows。");
#endif
}
}
