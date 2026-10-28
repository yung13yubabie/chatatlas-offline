#include "core.hpp"
#include "file.hpp"
#include "windows_files.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace atlas {
void Control::check()const{while(paused&&!stopped)std::this_thread::sleep_for(std::chrono::milliseconds(60));if(stopped)throw Cancelled();}
void Control::wait(int ms)const{for(int t=0;t<ms;t+=50){check();std::this_thread::sleep_for(std::chrono::milliseconds(std::min(50,ms-t)));}check();}
static const uint32_t constants[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
static uint32_t rotate(uint32_t x,unsigned n){return (x>>n)|(x<<(32-n));}
Sha256::Sha256():h{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19}{}
void Sha256::block(const unsigned char*p){uint32_t w[64];for(int i=0;i<16;i++)w[i]=uint32_t(p[i*4])<<24|uint32_t(p[i*4+1])<<16|uint32_t(p[i*4+2])<<8|p[i*4+3];for(int i=16;i<64;i++){uint32_t a=w[i-15],b=w[i-2];w[i]=w[i-16]+(rotate(a,7)^rotate(a,18)^(a>>3))+w[i-7]+(rotate(b,17)^rotate(b,19)^(b>>10));}auto a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];for(int i=0;i<64;i++){uint32_t t1=v+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[i]+w[i],t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;}
void Sha256::update(const void*data,size_t n){auto p=static_cast<const unsigned char*>(data);length+=n;while(n){size_t take=std::min(n,64-used);std::memcpy(buffer.data()+used,p,take);p+=take;n-=take;used+=take;if(used==64){block(buffer.data());used=0;}}}
std::string Sha256::finish(){const uint64_t bits=length*8;unsigned char end=0x80;update(&end,1);unsigned char zero=0;while(used!=56)update(&zero,1);unsigned char b[8];for(int i=0;i<8;i++)b[7-i]=static_cast<unsigned char>(bits>>(8*i));update(b,8);std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto x:h)s<<std::setw(8)<<x;return s.str();}
std::string sha(std::string_view text){Sha256 h;h.update(text.data(),text.size());return h.finish();}
std::string key(const std::vector<std::string>&parts){std::string value="[";for(size_t i=0;i<parts.size();i++){if(i)value+=", ";value+=J(parts[i]).dump(-1,' ',true);}return sha(value+"]");}
std::string utf8(const fs::path&p){auto s=p.u8string();return std::string(s.begin(),s.end());}fs::path path8(const std::string&s){return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()),s.size()));}
std::string now(){auto t=std::chrono::system_clock::now();time_t tt=std::chrono::system_clock::to_time_t(t);std::tm tm{};
#ifdef _WIN32
 gmtime_s(&tm,&tt);
#else
 gmtime_r(&tt,&tm);
#endif
 char out[64];std::strftime(out,sizeof out,"%Y-%m-%dT%H:%M:%S",&tm);return std::string(out)+".000+00:00";}
void noLinks(const fs::path&given){auto p=fs::absolute(given).lexically_normal();while(!p.empty()){
#ifdef _WIN32
 DWORD attr=GetFileAttributesW(p.c_str());if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_REPARSE_POINT))throw Error("保存位置包含連結，請改用一般私人資料夾。");
#else
 std::error_code ec;if(fs::is_symlink(fs::symlink_status(p,ec)))throw Error("保存位置包含連結，請改用一般私人資料夾。");
#endif
 auto next=p.parent_path();if(next==p)break;p=next;}}
fs::path privateDir(fs::path p){p=fs::absolute(p).lexically_normal();noLinks(p);
 // Only protect a directory created by this call. Existing destination parents
 // may be shared system or user folders; changing their permissions is unsafe.
 #ifdef _WIN32
 if(!fs::exists(p)){privateDir(p.parent_path());PrivateSecurity security;if(!CreateDirectoryW(p.c_str(),&security.attributes)&&GetLastError()!=ERROR_ALREADY_EXISTS)throw Error("無法建立私人資料夾。");}
 noLinks(p);
 #else
 if(fs::create_directories(p)){
  fs::permissions(p,fs::perms::owner_all,fs::perm_options::replace);
 }
 #endif
 return p;}
fs::path within(const fs::path&root,const std::string&name){auto rel=path8(name);if(name.empty()||rel.is_absolute()||rel.has_root_name()||name.find('\\')!=std::string::npos||name.find(':')!=std::string::npos)throw Error("檔案位置不正確。");for(auto p:rel)if(p=="..")throw Error("檔案位置不正確。");auto result=fs::absolute(root/rel).lexically_normal();noLinks(result);return result;}
std::string readText(const fs::path&p,uint64_t max){noLinks(p);if(!fs::is_regular_file(p)||fs::file_size(p)>max)throw Error("檔案無法讀取或過大。");std::ifstream in(p,std::ios::binary);if(!in)throw Error("檔案無法讀取。");std::string s(std::istreambuf_iterator<char>(in),{});if(s.size()>max)throw Error("檔案過大。");return s;}
static std::atomic<unsigned long>seq{0};static fs::path temporary(const fs::path&p){return p.parent_path()/path8(".atlas-"+sha(now()+std::to_string(seq++)+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())).substr(0,24));}
static void replace(const fs::path&from,const fs::path&to){
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw Error("保存未完成，請檢查可用空間。");
#else
 fs::rename(from,to);
#endif
}
void atomicText(const fs::path&p,const std::string&data){noLinks(p);privateDir(p.parent_path());auto temp=temporary(p);bool owned=false;try{{ExclusiveFile out(temp);owned=true;out.write(data);out.sync();}replace(temp,p);}catch(...){std::error_code ec;if(owned)fs::remove(temp,ec);throw;}}

Digest digest(const fs::path&p,Tick tick){noLinks(p);if(!fs::is_regular_file(p))throw Error("附件不是一般檔案。");auto stamp=fs::last_write_time(p);auto expected=fs::file_size(p);std::ifstream in(p,std::ios::binary);if(!in)throw Error("附件無法讀取。");Sha256 hash;Digest r;uint32_t crc=0xffffffff;static const auto table=[](){std::array<uint32_t,256>a{};for(uint32_t i=0;i<256;i++){uint32_t c=i;for(int n=0;n<8;n++)c=(c&1)?0xedb88320^(c>>1):c>>1;a[i]=c;}return a;}();std::vector<char>b(1024*1024);while(in){if(tick)tick();in.read(b.data(),b.size());auto n=in.gcount();hash.update(b.data(),n);r.size+=n;for(std::streamsize i=0;i<n;i++)crc=table[(crc^static_cast<unsigned char>(b[i]))&255]^(crc>>8);}if(in.bad()||expected!=r.size||stamp!=fs::last_write_time(p))throw Error("檔案仍在變動，請稍後再試。");r.hash=hash.finish();r.crc=crc^0xffffffff;return r;}
void copyVerified(const fs::path&from,const fs::path&to,int64_t expected,const std::string&hash,Tick tick){noLinks(from);noLinks(to);privateDir(to.parent_path());auto tmp=temporary(to);bool owned=false;try{std::ifstream in(from,std::ios::binary);if(!in)throw Error("附件無法保存。");{ExclusiveFile out(tmp);owned=true;std::vector<char>b(1024*1024);while(in){if(tick)tick();in.read(b.data(),b.size());out.write(b.data(),in.gcount());}if(in.bad())throw Error("附件無法保存。");out.sync();}auto d=digest(tmp,tick);if((expected>=0&&d.size!=static_cast<uint64_t>(expected))||(!hash.empty()&&d.hash!=hash)||(d.size==0&&expected!=0&&hash.empty()))throw Error("附件內容不符，未列為完整副本。");replace(tmp,to);}catch(...){std::error_code ec;if(owned)fs::remove(tmp,ec);throw;}}

std::string safeName(std::string s){for(auto&c:s)if(static_cast<unsigned char>(c)<32||std::string("<>:\"/\\|?*").find(c)!=std::string::npos)c='_';while(!s.empty()&&(s.back()=='.'||s.back()==' '))s.pop_back();if(s.size()>150){s.resize(150);while(!s.empty()&&(static_cast<unsigned char>(s.back())&0xc0)==0x80)s.pop_back();if(!s.empty()&&(static_cast<unsigned char>(s.back())&0x80))s.pop_back();}if(s.empty())s="attachment";auto upper=s.substr(0,s.find('.'));std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char c){return std::toupper(c);});if(upper=="CON"||upper=="PRN"||upper=="AUX"||upper=="NUL"||(upper.size()==4&&(upper.substr(0,3)=="COM"||upper.substr(0,3)=="LPT")&&upper[3]>='1'&&upper[3]<='9'))s="_"+s;return s;}
std::string base64(std::string_view s){static const char*c="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string out;out.reserve((s.size()+2)/3*4);for(size_t i=0;i<s.size();i+=3){uint32_t v=uint32_t(static_cast<unsigned char>(s[i]))<<16;if(i+1<s.size())v|=uint32_t(static_cast<unsigned char>(s[i+1]))<<8;if(i+2<s.size())v|=static_cast<unsigned char>(s[i+2]);out+=c[(v>>18)&63];out+=c[(v>>12)&63];out+=i+1<s.size()?c[(v>>6)&63]:'=';out+=i+2<s.size()?c[v&63]:'=';}return out;}
std::string safeJson(const J&v){auto s=v.dump(-1,' ',true);std::string result;for(char c:s)if(c=='<')result+="\\u003c";else if(c=='&')result+="\\u0026";else result+=c;return result;}
Lock::Lock(const fs::path&root){auto p=within(root,".vault-lock");
#ifdef _WIN32
 HANDLE h=CreateFileW(p.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);OVERLAPPED o{};if(h==INVALID_HANDLE_VALUE||!LockFileEx(h,LOCKFILE_EXCLUSIVE_LOCK|LOCKFILE_FAIL_IMMEDIATELY,0,1,0,&o)){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);throw Error("這份收藏正在使用中，請稍後再試。");}handle=h;
#else
 fd=::open(p.c_str(),O_RDWR|O_CREAT|O_NOFOLLOW,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)::close(fd);throw Error("這份收藏正在使用中，請稍後再試。");}
#endif
}
Lock::~Lock(){
#ifdef _WIN32
 if(handle)CloseHandle(static_cast<HANDLE>(handle));
#else
 if(fd>=0)::close(fd);
#endif
}
}
