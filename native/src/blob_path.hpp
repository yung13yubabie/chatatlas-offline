#pragma once
#include <string>
#include <string_view>
namespace atlas {
// Storage identity, not a content-safety verdict. Unknown attachment bytes use .bin.
inline bool canonicalBlobPath(std::string_view path,std::string_view hash){
 if(hash.size()!=64)return false;
 for(unsigned char c:hash)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
 const auto prefix=std::string("assets/")+std::string(hash.substr(0,2))+"/"+std::string(hash);
 for(auto ext:{".png",".jpg",".gif",".webp",".wav",".ogg",".mp3",".mp4",".bin"})if(path==prefix+ext)return true;
 return false;
}
}
