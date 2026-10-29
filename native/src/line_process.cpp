#include "line_process.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <unordered_map>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#endif

namespace atlas {
namespace {
void eraseBytes(void* p,size_t size) noexcept {
#ifdef _WIN32
 if(size)SecureZeroMemory(p,size);
#else
 volatile unsigned char* bytes=static_cast<volatile unsigned char*>(p);
 while(size--)*bytes++=0;
#endif
}
struct Secret32 {
 mutable std::array<unsigned char,32> bytes{};
 explicit Secret32(const unsigned char* p){std::copy_n(p,32,bytes.begin());}
 Secret32(const Secret32& other):bytes(other.bytes){}
 ~Secret32(){eraseBytes(bytes.data(),bytes.size());}
 bool operator==(const Secret32& other)const noexcept{return bytes==other.bytes;}
};
struct SecretHash {
 size_t operator()(const Secret32& key)const noexcept {
  size_t result=sizeof(size_t)==8?size_t(1469598103934665603ULL):size_t(2166136261U);
  for(auto byte:key.bytes){result^=byte;result*=sizeof(size_t)==8?size_t(1099511628211ULL):size_t(16777619U);}
  return result;
 }
};
struct SecretBuffer {
 std::vector<unsigned char> bytes;
 explicit SecretBuffer(size_t size):bytes(size){}
 ~SecretBuffer(){eraseBytes(bytes.data(),bytes.size());}
};
struct SecretString {
 std::string& text;
 ~SecretString(){eraseLineKey(text);}
};
struct EraseScope {
 void* bytes;size_t size;
 ~EraseScope(){eraseBytes(bytes,size);}
};
bool hex(unsigned char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F');}
bool alphaNumeric(unsigned char c){return(c>='0'&&c<='9')||(c>='a'&&c<='z')||(c>='A'&&c<='Z');}
struct SecretText {
 mutable std::string value;
 explicit SecretText(const std::string& text):value(text){}
 SecretText(const SecretText& other):value(other.value){}
 ~SecretText(){eraseLineKey(value);}
 bool operator==(const SecretText& other)const noexcept{return value==other.value;}
};
struct TextHash {size_t operator()(const SecretText& value)const noexcept{return std::hash<std::string>{}(value.value);}};
using SecretCounts=std::unordered_map<SecretText,uint32_t,TextHash>;
void recordText(SecretCounts& values,const std::string& text){
 SecretText value(text);auto found=values.find(value);
 if(found!=values.end()){if(found->second<UINT32_MAX)++found->second;return;}
 if(values.size()>=64)throw CapabilityError("LINE 工作階段資訊超過讀取上限，請保留目前使用的 LINE 後重試。");
 values.emplace(value,1);
}
const std::string& preferredText(const SecretCounts& values){
 static const std::string empty;
 const SecretCounts::value_type* best=nullptr;
 for(auto& item:values)if(!best||item.second>best->second||(item.second==best->second&&item.first.value<best->first.value))best=&item;
 return best?best->first.value:empty;
}
bool equalAscii(std::string_view left,std::string_view right){
 if(left.size()!=right.size())return false;
 for(size_t at=0;at<left.size();++at){auto a=left[at],b=right[at];if(a>='A'&&a<='Z')a+=32;if(b>='A'&&b<='Z')b+=32;if(a!=b)return false;}
 return true;
}
bool safeCredential(const std::string& value,bool application=false){
 return !value.empty()&&value.size()<=4096&&std::all_of(value.begin(),value.end(),[&](unsigned char c){return(application&&c==9)||(c>=(application?32:33)&&c<=126);});
}
bool quotedValue(std::string_view bytes,size_t& at,std::string& out){
 if(at>=bytes.size()||bytes[at++]!='"')return false;
 while(at<bytes.size()&&out.size()<=4096){
  unsigned char c=bytes[at++];if(c=='"')return true;if(c<32||c>126)return false;
  if(c!='\\'){out+=char(c);continue;}if(at>=bytes.size())return false;c=bytes[at++];
  if(c=='"'||c=='\\'||c=='/')out+=char(c);
  else if(c=='t')out+='\t';
  else if(c=='r')out+='\r';
  else if(c=='n')out+='\n';
  else if(c=='b')out+='\b';
  else if(c=='f')out+='\f';
  else if(c=='u'){
   if(at+4>bytes.size())return false;unsigned value=0;
   for(int digit=0;digit<4;++digit){auto hexDigit=static_cast<unsigned char>(bytes[at++]);if(!hex(hexDigit))return false;value=value*16+(hexDigit<='9'?hexDigit-'0':(hexDigit|32)-'a'+10);}
   if(value>127)return false;out+=char(value);
  }else return false;
 }
 return false;
}
bool jsonField(std::string_view bytes,size_t at,std::string& label,std::string& value){
 if(at&&bytes[at-1]=='\\')return false;
 if(!quotedValue(bytes,at,label)||label.size()>64)return false;
 while(at<bytes.size()&&(bytes[at]==' '||bytes[at]=='\t'||bytes[at]=='\r'||bytes[at]=='\n'))++at;
 if(at>=bytes.size()||bytes[at++]!=':')return false;
 while(at<bytes.size()&&(bytes[at]==' '||bytes[at]=='\t'||bytes[at]=='\r'||bytes[at]=='\n'))++at;
 return quotedValue(bytes,at,value);
}
bool headerField(std::string_view bytes,size_t at,std::string& label,std::string& value){
 if(at&&bytes[at-1]!='\n'&&bytes[at-1]!='\r'&&bytes[at-1]!='\0')return false;
 size_t start=at;
 while(at<bytes.size()&&at-start<64&&((bytes[at]>='A'&&bytes[at]<='Z')||(bytes[at]>='a'&&bytes[at]<='z')||bytes[at]=='-'))++at;
 if(at==start||at>=bytes.size()||bytes[at++]!=':')return false;
 label.assign(bytes.substr(start,at-start-1));while(at<bytes.size()&&(bytes[at]==' '||bytes[at]=='\t'))++at;
 start=at;while(at<bytes.size()&&at-start<=4096&&bytes[at]!='\r'&&bytes[at]!='\n'&&bytes[at]!='\0')++at;
 if(at==bytes.size()||at-start>4096)return false;
 value.assign(bytes.substr(start,at-start));while(!value.empty()&&(value.back()==' '||value.back()=='\t'))value.pop_back();return true;
}
bool obsHost(std::string_view value){return equalAscii(value,"obs.line-apps.com")||equalAscii(value,"obs.line-scdn.net")||equalAscii(value,"obs.line-apps.com:443")||equalAscii(value,"obs.line-scdn.net:443");}
bool obsUrl(std::string_view value){if(!value.starts_with("https://"))return false;auto end=value.find_first_of("/?#",8);return obsHost(value.substr(8,end==std::string_view::npos?end:end-8));}
bool observedObsContext(std::string_view bytes,size_t at){
 size_t begin=at>8192?at-8192:0,end=std::min(bytes.size(),at+8192);
 const auto zeroBefore=bytes.rfind('\0',at);if(zeroBefore!=std::string_view::npos&&zeroBefore>=begin)begin=zeroBefore+1;
 const auto blankBefore=bytes.rfind("\r\n\r\n",at);if(blankBefore!=std::string_view::npos&&blankBefore>=begin)begin=blankBefore+4;
 const auto zeroAfter=bytes.find('\0',at);if(zeroAfter!=std::string_view::npos)end=std::min(end,zeroAfter);
 const auto blankAfter=bytes.find("\r\n\r\n",at);if(blankAfter!=std::string_view::npos)end=std::min(end,blankAfter);
 if(bytes[at]=='"'){
  const auto objectBefore=bytes.rfind('{',at),objectAfter=bytes.find('}',at);
  if(objectBefore!=std::string_view::npos&&objectBefore>=begin)begin=objectBefore;
  if(objectAfter!=std::string_view::npos)end=std::min(end,objectAfter+1);
 }
 auto context=bytes.substr(begin,end-begin);
 for(size_t offset=0;offset<context.size();++offset){
  if(context[offset]!='H'&&context[offset]!='h'&&context[offset]!='"')continue;
  std::string label,value;SecretString clearValue{value};
  const bool json=context[offset]=='"';
  if(!(json?jsonField(context,offset,label,value):headerField(context,offset,label,value)))continue;
  if(equalAscii(label,"host")&&obsHost(value))return true;
  if(json&&(equalAscii(label,"url")||equalAscii(label,"requestUrl"))&&obsUrl(value))return true;
 }
 return false;
}
#ifdef _WIN32
struct SessionBinding {
 HANDLE process=nullptr;
 ~SessionBinding(){if(process)CloseHandle(process);}
};
#endif
}
void eraseLineKey(std::string& key)noexcept{eraseBytes(key.data(),key.size());key.clear();}

struct LineSession::Impl {
 SecretText access,obs,application;
#ifdef _WIN32
 std::shared_ptr<SessionBinding> binding;
#endif
 Impl(const std::string& access,const std::string& obs,const std::string& application):access(access),obs(obs),application(application){}
};
LineSession::LineSession(const std::string& access,const std::string& obs,const std::string& application):impl(std::make_unique<Impl>(access,obs,application)){}
LineSession::~LineSession()=default;
const std::string& LineSession::accessToken()const noexcept{return impl->access.value;}
const std::string& LineSession::obsAccessToken()const noexcept{return impl->obs.value;}
const std::string& LineSession::application()const noexcept{return impl->application.value;}
bool LineSession::current()const noexcept{
#ifdef _WIN32
 DWORD exit=0;return impl->binding&&impl->binding->process&&GetExitCodeProcess(impl->binding->process,&exit)&&exit==STILL_ACTIVE;
#else
 return false;
#endif
}
struct LineSessionCandidates::Impl {
 static constexpr size_t contextBytes=16384;
 std::array<unsigned char,contextBytes*2+2> tail{};size_t tailSize=0,skip=0;
 SecretCounts access,obs,application;
 ~Impl(){eraseBytes(tail.data(),tail.size());}
 void scan(std::string_view bytes,size_t start,size_t end){
  for(size_t at=start;at<end;++at){
   const auto first=bytes[at];if(first!='"'&&first!='X'&&first!='x')continue;
   std::string label,value;SecretString clearValue{value};const bool json=first=='"';
   if(!(json?jsonField(bytes,at,label,value):headerField(bytes,at,label,value)))continue;
   if(equalAscii(label,"X-Line-Application")){if(safeCredential(value,true))recordText(application,value);continue;}
   if(equalAscii(label,"X-Line-Access")){
    if(safeCredential(value)){if(observedObsContext(bytes,at))recordText(obs,value);else recordText(access,value);}continue;
   }
   if(json&&(label=="authToken"||label=="accessToken")&&safeCredential(value))recordText(access,value);
  }
 }
};
LineSessionCandidates::LineSessionCandidates():impl(std::make_unique<Impl>()){}
LineSessionCandidates::~LineSessionCandidates()=default;
void LineSessionCandidates::append(const unsigned char* bytes,size_t size,bool final){
 if((!bytes&&size)||size>1024*1024)throw Error("LINE 工作階段讀取區塊不正確。");
 SecretBuffer joined(impl->tailSize+size);std::copy_n(impl->tail.data(),impl->tailSize,joined.bytes.data());if(size)std::copy_n(bytes,size,joined.bytes.data()+impl->tailSize);
 const auto end=final?joined.bytes.size():(joined.bytes.size()>Impl::contextBytes?joined.bytes.size()-Impl::contextBytes:0);
 auto raw=std::string_view(reinterpret_cast<const char*>(joined.bytes.data()),joined.bytes.size());impl->scan(raw,std::min(impl->skip,end),end);
 for(size_t parity=0;parity<2;++parity){
  std::string decoded;SecretString cleanup{decoded};decoded.reserve(joined.bytes.size()/2);
  for(size_t at=parity;at+1<joined.bytes.size();at+=2)decoded+=joined.bytes[at+1]==0&&joined.bytes[at]<=127?char(joined.bytes[at]):'\0';
  auto start=impl->skip>parity?(impl->skip-parity+1)/2:0;auto stop=end>parity?(end-parity+1)/2:0;
  impl->scan(decoded,std::min(start,decoded.size()),std::min(stop,decoded.size()));
 }
 eraseBytes(impl->tail.data(),impl->tail.size());if(final){impl->tailSize=impl->skip=0;return;}
 const auto begin=end>Impl::contextBytes?end-Impl::contextBytes:0;impl->tailSize=joined.bytes.size()-begin;impl->skip=end-begin;
 std::copy(joined.bytes.begin()+begin,joined.bytes.end(),impl->tail.begin());
}
std::shared_ptr<LineSession> LineSessionCandidates::preferred()const{
 const auto& access=preferredText(impl->access);const auto& obs=preferredText(impl->obs);const auto& application=preferredText(impl->application);
 if(access.empty()&&obs.empty())return {};
 return std::shared_ptr<LineSession>(new LineSession(access,obs,application));
}

struct LineKeyCandidates::Impl {
 size_t maximum;std::unordered_map<Secret32,uint32_t,SecretHash> counts;
 std::array<unsigned char,68> tail{};size_t tailSize=0,skip=0;
 explicit Impl(size_t maximum):maximum(maximum){if(!maximum||maximum>65536)throw Error("LINE 金鑰掃描上限不正確。");}
 ~Impl(){eraseBytes(tail.data(),tail.size());}
 void record(const unsigned char* bytes){
  Secret32 candidate(bytes);auto found=counts.find(candidate);
  if(found!=counts.end()){if(found->second<std::numeric_limits<uint32_t>::max())++found->second;return;}
  if(counts.size()>=maximum)throw CapabilityError("LINE 本機解密資訊超過掃描上限，請關閉其他 LINE 視窗後再試。");
  counts.emplace(candidate,1);
 }
 void scan(const unsigned char* bytes,size_t size,size_t start,size_t end){
  for(size_t at=start;at<end;++at){
   if(!hex(bytes[at]))continue;
   if(at+32<=size&&(at==0||!alphaNumeric(bytes[at-1]))&&(at+32==size||!alphaNumeric(bytes[at+32]))){
    bool valid=true;for(size_t n=0;n<32;++n)if(!hex(bytes[at+n])){valid=false;break;}
    if(valid)record(bytes+at);
   }
   if(at+64<=size&&(at<2||bytes[at-1]!=0||!alphaNumeric(bytes[at-2]))&&
      (at+64==size||at+65==size||bytes[at+65]!=0||!alphaNumeric(bytes[at+64]))){
    std::array<unsigned char,32> value{};EraseScope cleanup{value.data(),value.size()};bool valid=true;
    for(size_t n=0;n<32;++n){value[n]=bytes[at+n*2];if(!hex(value[n])||bytes[at+n*2+1]!=0){valid=false;break;}}
    if(valid)record(value.data());
   }
  }
 }
};
LineKeyCandidates::LineKeyCandidates(size_t maximum):impl(std::make_unique<Impl>(maximum)){}
LineKeyCandidates::~LineKeyCandidates()=default;
size_t LineKeyCandidates::size()const{return impl->counts.size();}
void LineKeyCandidates::append(const unsigned char* bytes,size_t size,bool final){
 if((!bytes&&size)||size>1024*1024)throw Error("LINE 記憶體讀取區塊不正確。");
 if(size>std::numeric_limits<size_t>::max()-impl->tailSize)throw Error("LINE 記憶體讀取區塊不正確。");
 SecretBuffer joined(impl->tailSize+size);
 std::copy_n(impl->tail.data(),impl->tailSize,joined.bytes.data());
 if(size)std::copy_n(bytes,size,joined.bytes.data()+impl->tailSize);
 const size_t end=final?joined.bytes.size():(joined.bytes.size()>66?joined.bytes.size()-66:0);
 impl->scan(joined.bytes.data(),joined.bytes.size(),std::min(impl->skip,end),end);
 eraseBytes(impl->tail.data(),impl->tail.size());
 if(final){impl->tailSize=impl->skip=0;return;}
 const size_t begin=end>2?end-2:0;impl->tailSize=joined.bytes.size()-begin;impl->skip=end-begin;
 std::copy(joined.bytes.begin()+begin,joined.bytes.end(),impl->tail.begin());
}
std::string LineKeyCandidates::findMatch(Control& control,const std::function<bool(const std::string&)>& probe,Tick tick){
 if(!probe)throw Error("LINE 資料庫核對方式不正確。");
 using Pair=decltype(impl->counts)::value_type;
 std::vector<const Pair*> ranked;ranked.reserve(impl->counts.size());
 for(auto& entry:impl->counts)ranked.push_back(&entry);
 std::sort(ranked.begin(),ranked.end(),[](const Pair* left,const Pair* right){return left->second!=right->second?left->second>right->second:left->first.bytes<right->first.bytes;});
 for(auto* entry:ranked){
  control.check();if(tick)tick();
  std::string candidate(reinterpret_cast<const char*>(entry->first.bytes.data()),entry->first.bytes.size());SecretString cleanup{candidate};
  if(probe(candidate)){control.check();if(tick)tick();return std::string(candidate);}
 }
 return {};
}

#ifdef _WIN32
namespace {
struct Handle {
 HANDLE value=nullptr;
 explicit Handle(HANDLE value=nullptr):value(value){}
 ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
 Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
 explicit operator bool()const{return value&&value!=INVALID_HANDLE_VALUE;}
};
std::vector<unsigned char> tokenUser(HANDLE process){
 HANDLE raw=nullptr;if(!OpenProcessToken(process,TOKEN_QUERY,&raw))return {};
 Handle token(raw);DWORD size=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&size);
 if(!size||size>65536)return {};
 std::vector<unsigned char> bytes(size);
 if(!GetTokenInformation(token.value,TokenUser,bytes.data(),size,&size))return {};
 return bytes;
}
bool sameUser(HANDLE process,const std::vector<unsigned char>& user){
 auto other=tokenUser(process);if(other.empty()||user.empty())return false;
 return EqualSid(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid,
                 reinterpret_cast<const TOKEN_USER*>(other.data())->User.Sid)!=FALSE;
}
bool trustedPublisher(const fs::path& path,HANDLE file){
 WINTRUST_FILE_INFO info{};info.cbStruct=sizeof(info);info.pcwszFilePath=path.c_str();info.hFile=file;
 WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;
 data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&info;data.dwStateAction=WTD_STATEACTION_VERIFY;
 data.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL|WTD_REVOCATION_CHECK_NONE;
 GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;
 LONG status=WinVerifyTrust(nullptr,&policy,&data);
 struct Close {WINTRUST_DATA& data;GUID& policy;~Close(){data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&data);}}close{data,policy};
 if(status!=ERROR_SUCCESS)return false;
 auto provider=WTHelperProvDataFromStateData(data.hWVTStateData);
 auto signer=provider?WTHelperGetProvSignerFromChain(provider,0,FALSE,0):nullptr;
 if(!signer||!signer->csCertChain||!signer->pasCertChain||!signer->pasCertChain[0].pCert)return false;
 auto cert=signer->pasCertChain[0].pCert;
 DWORD length=CertGetNameStringW(cert,CERT_NAME_ATTR_TYPE,0,const_cast<char*>(szOID_ORGANIZATION_NAME),nullptr,0);
 if(length<2||length>256)return false;
 std::vector<wchar_t> organization(length);
 if(CertGetNameStringW(cert,CERT_NAME_ATTR_TYPE,0,const_cast<char*>(szOID_ORGANIZATION_NAME),organization.data(),length)!=length)return false;
 for(const wchar_t* expected:{L"LINE Corporation",L"LY Corporation",L"NAVER Corporation"})if(_wcsicmp(organization.data(),expected)==0)return true;
 return false;
}
bool readable(const MEMORY_BASIC_INFORMATION& info){
 if(info.State!=MEM_COMMIT||info.Type!=MEM_PRIVATE||!info.RegionSize||info.Protect&(PAGE_NOACCESS|PAGE_GUARD))return false;
 switch(info.Protect&0xff){case PAGE_READONLY:case PAGE_READWRITE:case PAGE_WRITECOPY:case PAGE_EXECUTE_READ:case PAGE_EXECUTE_READWRITE:case PAGE_EXECUTE_WRITECOPY:return true;default:return false;}
}
}
#endif

void visitLineDatabaseKeys(Control& control,const LineDatabaseKeyVisitor& visitor,std::function<void(std::string)> progress,bool observeSession){
 control.check();if(!visitor)throw Error("LINE 資料庫核對方式不正確。");
#ifdef _WIN32
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
 const auto budget=[&]{control.check();if(std::chrono::steady_clock::now()>deadline)throw CapabilityError("LINE 本機解密資訊讀取已達時間上限，請保留一個已登入的 LINE 視窗後重試。");};
 auto user=tokenUser(GetCurrentProcess());if(user.empty())throw CapabilityError("無法確認目前 Windows 帳號，未讀取 LINE。");
 DWORD sessionId=0;if(!ProcessIdToSessionId(GetCurrentProcessId(),&sessionId))throw CapabilityError("無法確認目前 Windows 工作階段，未讀取 LINE。");
 Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));if(!snapshot)throw CapabilityError("無法查看目前的 LINE 程序。");
 PROCESSENTRY32W entry{};entry.dwSize=sizeof(entry);bool anyLine=false,anyTrusted=false,anyReadable=false;
 uint64_t inspected=0;size_t verifiedProcesses=0;
 if(progress)progress("正在核對 LINE 程序與本機解密資訊…");
 if(Process32FirstW(snapshot.value,&entry))do{
  budget();if(_wcsicmp(entry.szExeFile,L"LINE.exe")!=0)continue;
  DWORD otherSession=0;if(!ProcessIdToSessionId(entry.th32ProcessID,&otherSession)||otherSession!=sessionId)continue;
  anyLine=true;Handle process(OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,entry.th32ProcessID));
  if(!process||!sameUser(process.value,user))continue;
  std::vector<wchar_t> image(32768);DWORD length=static_cast<DWORD>(image.size());
  if(!QueryFullProcessImageNameW(process.value,0,image.data(),&length)||!length||length>=image.size())continue;
  fs::path path(std::wstring(image.data(),length));if(_wcsicmp(path.filename().c_str(),L"LINE.exe")!=0)continue;
  Handle executable(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
  if(!executable||!trustedPublisher(path,executable.value))continue;
  anyTrusted=true;if(++verifiedProcesses>8)throw CapabilityError("同時開啟的 LINE 程序過多，請保留要讀取的 LINE 後重試。");
  LineKeyCandidates candidates;LineSessionCandidates contexts;bool processReadable=false;
  if(progress)progress("正在讀取 LINE 本機解密資訊；金鑰只會留在記憶體中…");
  SYSTEM_INFO system{};GetNativeSystemInfo(&system);uintptr_t address=reinterpret_cast<uintptr_t>(system.lpMinimumApplicationAddress),maximum=reinterpret_cast<uintptr_t>(system.lpMaximumApplicationAddress);
  SecretBuffer buffer(1024*1024);
  while(address<maximum){
   budget();DWORD exit=0;if(!GetExitCodeProcess(process.value,&exit)||exit!=STILL_ACTIVE)break;
   MEMORY_BASIC_INFORMATION info{};if(VirtualQueryEx(process.value,reinterpret_cast<LPCVOID>(address),&info,sizeof(info))!=sizeof(info))break;
   const uintptr_t base=reinterpret_cast<uintptr_t>(info.BaseAddress);if(info.RegionSize>std::numeric_limits<uintptr_t>::max()-base)break;
   const uintptr_t next=base+info.RegionSize;if(next<=address)break;
   if(readable(info)){
    for(uintptr_t offset=base;offset<next;){
     budget();const size_t requested=static_cast<size_t>(std::min<uintptr_t>(buffer.bytes.size(),next-offset));
     if(inspected>2ULL*1024*1024*1024-requested)throw CapabilityError("LINE 私有記憶體超過本機掃描上限，請關閉其他 LINE 視窗後重試。");
     inspected+=requested;SIZE_T received=0;
     ReadProcessMemory(process.value,reinterpret_cast<LPCVOID>(offset),buffer.bytes.data(),requested,&received);
     if(received>requested)throw CapabilityError("LINE 記憶體讀取回應不相容，已停止。");
     if(received){anyReadable=processReadable=true;const bool final=received<requested||offset+requested==next;candidates.append(buffer.bytes.data(),received,final);if(observeSession)contexts.append(buffer.bytes.data(),received,final);}
     else{candidates.append(nullptr,0,true);if(observeSession)contexts.append(nullptr,0,true);}
     eraseBytes(buffer.bytes.data(),requested);offset+=requested;
    }
    candidates.append(nullptr,0,true);
    if(observeSession)contexts.append(nullptr,0,true);
   }
   address=next;
  }
  budget();DWORD exit=0;
  if(!processReadable||!GetExitCodeProcess(process.value,&exit)||exit!=STILL_ACTIVE)continue;
  if(progress)progress("正在以選取的 LINE 資料庫核對本機解密資訊…");
  std::shared_ptr<LineSession> observed=observeSession?contexts.preferred():nullptr;
  if(observed){
   observed->impl->binding=std::make_shared<SessionBinding>();
   observed->impl->binding->process=process.value;process.value=nullptr;
  }
  const bool finished=visitor(candidates,std::move(observed),budget);
  budget();if(finished)return;
 }while(Process32NextW(snapshot.value,&entry));
 if(!anyLine)throw CapabilityError("找不到目前工作階段的 LINE.exe。請先開啟並登入 LINE，再選擇本機資料庫。");
 if(!anyTrusted)throw CapabilityError("無法以目前帳號唯讀開啟並核對 LINE 的簽章發行者，未讀取金鑰；不會自動提升權限。");
 if(!anyReadable)throw CapabilityError("目前無法讀取 LINE 的本機解密資訊；請確認 LINE 已登入且屬於目前 Windows 帳號。");
 return;
#else
 (void)progress;(void)observeSession;throw CapabilityError("原生 LINE 金鑰讀取僅支援 Windows，請在已登入 LINE 的 Windows 電腦操作。");
#endif
}

std::string findLineDatabaseKey(const fs::path& database,Control& control,const std::function<bool(const std::string&)>& probe,std::function<void(std::string)> progress,std::shared_ptr<LineSession>* session){
 if(session)session->reset();
 control.check();if(!probe)throw Error("LINE 資料庫核對方式不正確。");
 noLinks(database);if(!fs::is_regular_file(database))throw CapabilityError("請先選擇可讀取的 LINE 本機資料庫。");
 std::string result;SecretString cleanup{result};std::shared_ptr<LineSession> matchedSession;
 visitLineDatabaseKeys(control,[&](LineKeyCandidates& candidates,std::shared_ptr<LineSession> observed,Tick budget){
  auto match=candidates.findMatch(control,probe,std::move(budget));SecretString clearMatch{match};
  if(match.empty())return false;
  result=std::move(match);matchedSession=std::move(observed);return true;
 },std::move(progress),session!=nullptr);
 if(result.empty())throw CapabilityError("本機解密資訊無法開啟選取的 LINE 資料庫，請確認帳號、資料庫與 LINE 版本相容。");
 if(session)*session=std::move(matchedSession);
 return std::string(result);
}
}
