// Pure storage-name contract: inert strings only, not attachment execution or antivirus.
#include "blob_path.hpp"
#include <iostream>
#include <vector>
#include <stdexcept>
using namespace atlas;
int main(){try{
 size_t checks=0;auto expect=[&](bool value){++checks;if(!value)throw std::runtime_error("blob path contract mismatch");};
 const std::string hash="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",prefix="assets/01/"+hash;
 for(const auto* ext:{"png","jpg","gif","webp","wav","ogg","mp3","mp4","bin"})expect(canonicalBlobPath(prefix+"."+ext,hash));
 for(const auto& path:std::vector<std::string>{"assets/photo.png","assets/inert.html","assets/inert.exe",prefix+".html",prefix+".exe",prefix+".PNG",prefix+".jpeg",prefix+".bin/child",prefix+".bin ",prefix+".bin.",prefix+".bin:stream","assets/ff/"+hash+".bin","assets/01/"+std::string(64,'a')+".bin","other/01/"+hash+".bin","../"+prefix+".bin","assets/01/../01/"+hash+".bin","assets\\01\\"+hash+".bin","/"+prefix+".bin","https://example.invalid/"+prefix+".bin",prefix+".bin?x",prefix+".bin#x",""})expect(!canonicalBlobPath(path,hash));
 expect(!canonicalBlobPath(prefix+".bin",hash.substr(1)));expect(!canonicalBlobPath(prefix+".bin",hash+"0"));auto upper=hash;upper[10]='A';expect(!canonicalBlobPath("assets/01/"+upper+".bin",upper));expect(!canonicalBlobPath(prefix+".bin",std::string(64,'g')));
 std::cout<<checks<<" pure blob storage-path checks passed (no file contents executed).\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
