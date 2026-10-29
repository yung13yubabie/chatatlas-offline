#include "line_locations.hpp"
#include <algorithm>
#include <deque>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include "private_path.hpp"
#endif

namespace atlas {
namespace {
constexpr size_t maxEntries=4096,maxDirectories=256,maxCandidates=64;
constexpr unsigned maxDepth=2;
std::string lowerAscii(std::string value){for(auto& c:value)if(c>='A'&&c<='Z')c+=char('a'-'A');return value;}
bool candidateName(const fs::path& path){
 if(lowerAscii(utf8(path.extension()))!=".edb")return false;
 auto name=lowerAscii(utf8(path.filename()));
 // The reference local reader identifies these as separate feature databases.
 for(const auto* prefix:{"album_","keep_","chatstats_"})if(name.rfind(prefix,0)==0)return false;
 return true;
}
bool plainDirectory(const fs::path& path){
 try{noLinks(path);}catch(const Error&){return false;}
 std::error_code ec;return fs::is_directory(fs::symlink_status(path,ec))&&!ec;
}
}

LineDatabaseLocations findLineDatabaseLocations(const fs::path& localAppData,Tick tick){
 LineDatabaseLocations result;
 if(localAppData.empty()||!localAppData.is_absolute())return result;
#ifdef _WIN32
 // Discovery is local-only; a modified environment must not trigger network traversal.
 if(privatePathKind(localAppData.wstring())!=PrivatePathKind::drive)return result;
 auto spelling=localAppData.wstring();for(auto& c:spelling)if(c==L'/')c=L'\\';
 if(spelling.rfind(L"\\\\?\\",0)==0)spelling.erase(0,4);
 if(!localPrivateDriveType(GetDriveTypeW(spelling.substr(0,3).c_str())))return result;
#endif
 const auto base=absoluteNormalizedPath(localAppData),root=base/L"LINE"/L"Data"/L"db";
 result.defaultDirectory=root;
 for(const auto& directory:{root,base/L"LINE"/L"Data",base/L"LINE",base}){
  if(tick)tick();if(plainDirectory(directory)){result.defaultDirectory=directory;break;}
 }
 result.searchedDirectories.push_back(root);
 if(!plainDirectory(root))return result;
 struct Pending{fs::path path;unsigned depth;};
 std::deque<Pending> pending{{root,0}};
 size_t entries=0,directories=0;
 while(!pending.empty()){
  if(tick)tick();auto current=std::move(pending.front());pending.pop_front();
  if(++directories>maxDirectories){result.limited=true;break;}
  if(!plainDirectory(current.path)){result.limited=true;continue;}
  std::error_code ec;
  fs::directory_iterator iterator(current.path,fs::directory_options::none,ec),end;
  if(ec){result.limited=true;continue;}
  while(iterator!=end){
   if(tick)tick();if(++entries>maxEntries){result.limited=true;pending.clear();break;}
   const auto path=iterator->path();
   bool allowed=true;try{noLinks(path);}catch(const Error&){allowed=false;}
   auto status=allowed?fs::symlink_status(path,ec):fs::file_status{};
   if(allowed&&!ec){
    if(fs::is_directory(status)){
     if(current.depth<maxDepth&&pending.size()<maxDirectories)pending.push_back({path,current.depth+1});
     else result.limited=true;
    }else if(fs::is_regular_file(status)&&candidateName(path)){
     auto size=fs::file_size(path,ec);
     if(!ec&&size){
      LineDatabaseCandidate candidate;candidate.path=path;candidate.size=size;
      candidate.modified=fs::last_write_time(path,ec);candidate.hasModified=!ec;
      result.candidates.push_back(std::move(candidate));
     }
    }
   }
   ec.clear();iterator.increment(ec);if(ec){result.limited=true;break;}
  }
 }
 std::sort(result.candidates.begin(),result.candidates.end(),[](const auto& a,const auto& b){
  if(a.hasModified!=b.hasModified)return a.hasModified;
  if(a.hasModified&&a.modified!=b.modified)return a.modified>b.modified;
  return utf8(a.path)<utf8(b.path);
 });
 if(result.candidates.size()>maxCandidates){result.candidates.resize(maxCandidates);result.limited=true;}
 return result;
}

LineDatabaseLocations currentLineDatabaseLocations(Tick tick){
#ifdef _WIN32
 DWORD length=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
 if(!length||length>32768)return {};
 std::wstring value(length,L'\0');DWORD copied=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),length);
 if(!copied||copied>=length)return {};
 value.resize(copied);return findLineDatabaseLocations(fs::path(value),std::move(tick));
#else
 (void)tick;return {};
#endif
}
}
