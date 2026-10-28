// Test production ZIP writer directly; link all core sources EXCEPT zip.cpp.
// Synthetic fixtures never open a Store or bypass its location checks.
#include "../src/zip.cpp"
#include <iostream>
#include <fstream>
#include <limits>
using namespace atlas;
static int passed=0;
static void expect(bool ok,const char*what){if(!ok)throw std::runtime_error(what);++passed;}
template<class F>static void rejects(F f,const char*what){bool failed=false;try{f();}catch(const std::exception&){failed=true;}expect(failed,what);}
static uint64_t little(const std::string&s,size_t p,size_t n){if(p>s.size()||n>s.size()-p)throw std::runtime_error("truncated test record");uint64_t v=0;for(size_t i=0;i<n;++i)v|=uint64_t(static_cast<unsigned char>(s[p+i]))<<(8*i);return v;}
static void arithmetic(){
 using namespace zip_bounds;constexpr auto maximum=std::numeric_limits<uint64_t>::max();
 expect(add(0xffffffffULL,1)==0x100000000ULL,"ZIP64 crosses 4GiB");
 expect(add(maximum,0)==maximum,"uint64 maximum exact");rejects([&]{add(maximum,1);},"counter overflow");
 expect(memberEnd(0xffffffffULL,1,0xffffffffULL)==0x200000031ULL,"member size and offset above 4GiB");
 expect(memberEnd(0,65535,0)==65585,"65535-byte name");rejects([]{memberEnd(0,65536,0);},"65536-byte name");rejects([]{memberEnd(0,0,0);},"empty name");
 rejects([&]{memberEnd(maximum-50,1,0);},"local header overflow");rejects([&]{memberEnd(0,1,maximum);},"payload overflow");
 expect(directoryEntry(65535)==65609,"directory name bound");rejects([]{directoryEntry(65536);},"directory name overflow");
 expect(directoryEnd(maximum-98,0)==maximum,"end records reserve 98 bytes");rejects([&]{directoryEnd(maximum-98,1);},"central and end record overflow");
 expect(legacyCount(65534)==65534&&legacyCount(65535)==65535&&legacyCount(65536)==65535,"legacy count sentinel");
 expect(legacySize(0xfffffffe)==0xfffffffe&&legacySize(0xffffffff)==0xffffffff&&legacySize(0x100000000ULL)==0xffffffff,"legacy size sentinel");
 expect(copySize(0xffffffffULL,1,0x100000000ULL)==0x100000000ULL,"copy crosses 4GiB");rejects([]{copySize(3,1,3);},"growth before copy");rejects([&]{copySize(maximum,1,maximum);},"copy overflow");
}
int main(){fs::path fixture;try{
 arithmetic();fixture=fs::temp_directory_path()/("chatatlas-zip-boundary-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));if(!fs::create_directory(fixture))throw std::runtime_error("fixture exists");
 const auto source=fixture/"source";{std::ofstream f(source,std::ios::binary);f<<"abc";}
 {Zip zip(fixture/"small.zip");zip.add(source,"a",{});zip.close();}auto bytes=readText(fixture/"small.zip");
 expect(little(bytes,0,4)==0x04034b50&&little(bytes,4,2)==45,"ZIP64 local version");
 expect(little(bytes,18,4)==0xffffffff&&little(bytes,22,4)==0xffffffff,"local size sentinels");
 expect(little(bytes,31,2)==1&&little(bytes,33,2)==16&&little(bytes,35,8)==3&&little(bytes,43,8)==3,"local ZIP64 extra");expect(bytes.substr(51,3)=="abc","stored bytes");
 expect(little(bytes,54,4)==0x02014b50&&little(bytes,96,4)==0xffffffff,"central offset sentinel");
 expect(little(bytes,101,2)==1&&little(bytes,103,2)==24&&little(bytes,105,8)==3&&little(bytes,121,8)==0,"central ZIP64 extra");
 expect(little(bytes,129,4)==0x06064b50&&little(bytes,153,8)==1&&little(bytes,169,8)==75&&little(bytes,177,8)==54,"ZIP64 EOCD values");
 expect(little(bytes,185,4)==0x07064b50&&little(bytes,193,8)==129&&little(bytes,205,4)==0x06054b50&&bytes.size()==227,"locator and legacy EOCD");
 {Zip zip(fixture/"names.zip");rejects([&]{zip.add(source,std::string(65536,'n'),{});},"writer long name rejection");zip.add(source,std::string(65535,'n'),{});zip.close();}expect(little(readText(fixture/"names.zip"),26,2)==65535,"writer maximum name");
 int ticks=0;
#ifdef _WIN32
 // The validated reader intentionally denies write-sharing. Do not relax it
 // merely to provoke growth: verify the attempted append fails instead.
 bool appendBlocked=false;
 {Zip zip(fixture/"growth.partial");zip.add(source,"a",[&]{if(++ticks==2){std::ofstream f(source,std::ios::app|std::ios::binary);appendBlocked=!f;if(f)throw std::runtime_error("writer acquired a shared read source");}});zip.close();}
 expect(ticks==2&&appendBlocked,"Windows reader blocks concurrent append");
 expect(readText(source)=="abc"&&fs::file_size(fixture/"growth.partial")==227,"blocked append preserves source and ZIP");
#else
 {Zip zip(fixture/"growth.partial");rejects([&]{zip.add(source,"a",[&]{if(++ticks==2){std::ofstream f(source,std::ios::app|std::ios::binary);f<<"d";}});},"writer source growth");}
 expect(ticks==2&&fs::file_size(fixture/"growth.partial")==51,"grown payload not written");
#endif
 {std::ofstream f(source,std::ios::trunc);}
 {Zip zip(fixture/"many.zip");for(size_t i=0;i<65536;++i)zip.add(source,std::to_string(i),{});zip.close();}bytes=readText(fixture/"many.zip");auto end=bytes.size()-98;
 expect(little(bytes,end,4)==0x06064b50&&little(bytes,end+24,8)==65536&&little(bytes,end+32,8)==65536,"ZIP64 count exceeds 65535");expect(little(bytes,bytes.size()-14,2)==65535&&little(bytes,bytes.size()-12,2)==65535,"legacy count saturates");
 fs::remove_all(fixture);std::cout<<"ZIP boundary checks passed: "<<passed<<"\n";return 0;
 }catch(const std::exception&e){if(!fixture.empty()){std::error_code ec;fs::remove_all(fixture,ec);}std::cerr<<e.what()<<"\n";return 1;}}
