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

 // Bind content-addressed asset member names to the bytes being archived.
 // These are inert synthetic bytes; suffix validation is not malware detection.
 const auto asset=fixture/"inert-asset";{std::ofstream f(asset);f<<"old harmless";}
 const auto oldAsset=digest(asset);const auto assetPrefix="assets/"+oldAsset.hash.substr(0,2)+"/"+oldAsset.hash;
 {Zip zip(fixture/"asset-suffixes.zip");for(const auto*ext:{"png","jpg","gif","webp","wav","ogg","mp3","mp4","bin"})zip.add(asset,assetPrefix+"."+ext,{});zip.close();}
 auto assetZip=readText(fixture/"asset-suffixes.zip");expect(little(assetZip,assetZip.size()-14,2)==9,"all nine canonical asset suffixes remain supported");
 for(const auto*ext:{"png","jpg","gif","webp","wav","ogg","mp3","mp4","bin"})expect(assetZip.find(assetPrefix+"."+ext)!=std::string::npos,"canonical asset member retained");
 auto rejectAsset=[&](const std::string&name,const std::string&label){const auto path=fixture/(label+".zip");bool rejected=false;{Zip zip(path);try{zip.add(asset,name,{});}catch(const IntegrityError&){rejected=true;}zip.close();}expect(rejected,"bad asset identity explicitly rejected");auto rejectedZip=readText(path);expect(rejectedZip.size()==98&&little(rejectedZip,0,4)==0x06064b50&&little(rejectedZip,24,8)==0,"rejection writes no member header or directory entry");};
 rejectAsset("assets/00/"+std::string(64,'0')+".bin","wrong-hash");
 rejectAsset("assets/"+(oldAsset.hash.substr(0,2)=="ff"?std::string("00"):std::string("ff"))+"/"+oldAsset.hash+".bin","wrong-prefix");
 rejectAsset("assets/noncanonical.bin","noncanonical-name");rejectAsset(assetPrefix+".html","unapproved-suffix");
 // Simulate the serializer boundary after a previously successful preflight.
 // No Store is opened, and no Store location guard is bypassed.
 {std::ofstream f(asset,std::ios::trunc);f<<"new harmless";}
 expect(digest(asset).hash!=oldAsset.hash,"fixture changed after old digest was recorded");
 rejectAsset(assetPrefix+".bin","changed-after-preflight");
 {Zip zip(fixture/"ordinary-members.zip");zip.add(asset,"archive.sqlite3",{});zip.add(asset,"reader/generation.js",{});zip.add(asset,"downloads/chunk.js",{});zip.close();}
 auto ordinary=readText(fixture/"ordinary-members.zip");expect(little(ordinary,ordinary.size()-14,2)==3&&ordinary.find("new harmless")!=std::string::npos,"nonasset members retain existing behavior");

 const auto cancelled=fixture/"cancelled.zip",partial=fixture/"cancelled.partial";int centralTicks=0;bool cancelledCaught=false;
 try{{Zip zip(partial);zip.add(source,"a",{});zip.add(source,"b",{});zip.close([&]{if(++centralTicks==2)throw Cancelled();});}publishNew(partial,cancelled);}catch(const Cancelled&){cancelledCaught=true;fs::remove(partial);}
 expect(cancelledCaught&&centralTicks==2&&!fs::exists(cancelled)&&!fs::exists(partial),"central directory cancellation prevents publication");
 const auto finished=fixture/"finished.zip",pending=fixture/"finished.partial",retained=fixture/"retained-snapshot";
 {Zip zip(pending);zip.add(source,"a",{});zip.close();}publishNew(pending,finished);
 fs::create_directory(retained);{std::ofstream f(retained/"synthetic-blocker");f<<"x";}
 expect(cleanupPublishedSnapshot(retained)==retained&&fs::exists(finished),"published ZIP stays completed with retained cleanup warning");
 expect(cleanupPublishedSnapshot(fixture/"already-absent").empty(),"already removed snapshot needs no warning");
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
