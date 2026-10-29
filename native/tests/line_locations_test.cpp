// Owned synthetic directory fixtures only; never inspect a real LINE installation.
#include "line_locations.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
using namespace atlas;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw Error(message);}
void file(const fs::path& path,size_t count=8){fs::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);out<<std::string(count,'x');if(!out)throw Error("fixture write failed");}
bool contains(const LineDatabaseLocations& found,const fs::path& path){return std::any_of(found.candidates.begin(),found.candidates.end(),[&](const auto& c){return c.path==path;});}
}
int main(int argc,char** argv){
 try{
  require(argc==2,"owned fixture directory required");const auto root=absoluteNormalizedPath(path8(argv[1]));
  require(fs::is_directory(root),"fixture directory exists");
  auto absent=root/"missing";
  auto empty=findLineDatabaseLocations(absent);
  require(empty.candidates.empty(),"missing LINE installation is not an account");
  require(!fs::exists(absent),"discovery must not create a directory");
  require(findLineDatabaseLocations({}).candidates.empty(),"empty base is not searched");
  require(findLineDatabaseLocations("relative-base").searchedDirectories.empty(),"relative base is not searched");
  auto base=root/"local-data",db=base/"LINE"/"Data"/"db";
  fs::create_directories(base/"LINE"/"Data");
  auto before=findLineDatabaseLocations(base);
  require(before.defaultDirectory==base/"LINE"/"Data","manual browsing starts in nearest existing LINE directory");
  require(before.searchedDirectories==std::vector<fs::path>{db},"only known LINE db location is searched");
  file(db/"large-old.edb",4096);file(db/"small-new.edb",8);file(db/"UPPER.EDB");
  const auto modified=fs::file_time_type::clock::now();
  fs::last_write_time(db/"large-old.edb",modified-std::chrono::hours(4));
  fs::last_write_time(db/"small-new.edb",modified);
  fs::last_write_time(db/"UPPER.EDB",modified-std::chrono::hours(2));
  for(const auto* name:{"album_one.edb","ALBUM_TWO.EDB","keep_one.edb","chatStats_one.edb","archive.sqlite3","not-line.db","main.edb-wal"})file(db/name);
  file(db/"empty.edb",0);fs::create_directories(db/"directory.edb");
  file(db/path8("人工帳號")/"account.edb");
  file(db/"first"/"second"/"nested.edb");
  file(db/"first"/"second"/"third"/"too-deep.edb");
  file(base/"LINE"/"other.edb");file(base/"unrelated"/"not-searched.edb");
  auto found=findLineDatabaseLocations(base);
  require(found.defaultDirectory==db,"manual browse starts in db when present");
  require(found.candidates.size()==5,"only nonempty main edb candidates within bounded subtree are listed");
  require(found.limited,"deeper subtree is reported as outside bounded scan");
  require(contains(found,db/path8("人工帳號")/"account.edb"),"Unicode account directory is supported");
  require(contains(found,db/"first"/"second"/"nested.edb"),"two account levels are supported");
  require(!contains(found,db/"first"/"second"/"third"/"too-deep.edb"),"deeper data is not traversed");
  for(const auto& candidate:found.candidates){
   require(candidate.path.is_absolute(),"candidate location is explicit and absolute");
   require(candidate.hasModified,"candidate modification metadata is available");
   require(candidate.kind=="LINE 對話資料庫候選","candidate is not falsely identified as an account");
  }
  auto small=std::find_if(found.candidates.begin(),found.candidates.end(),[&](const auto& c){return c.path==db/"small-new.edb";});
  auto large=std::find_if(found.candidates.begin(),found.candidates.end(),[&](const auto& c){return c.path==db/"large-old.edb";});
  require(small<large&&small->size==8&&large->size==4096,"candidate ordering uses time, never largest-file account selection");
  require(fs::file_size(db/"large-old.edb")==4096&&fs::last_write_time(db/"large-old.edb")==modified-std::chrono::hours(4),"discovery leaves source bytes and metadata untouched");
  bool stopped=false;try{findLineDatabaseLocations(base,[]{throw Cancelled();});}catch(const Cancelled&){stopped=true;}
  require(stopped,"search cancellation is propagated");

  // A link into another directory must not expand the candidate search.
  auto outside=root/"outside";file(outside/"outside.edb");
#ifdef _WIN32
  auto linked=db/L"linked-directory";
  auto command=L"cmd.exe /d /c mklink /J \""+linked.wstring()+L"\" \""+outside.wstring()+L"\" >nul 2>&1";
  require(_wsystem(command.c_str())==0,"create owned junction fixture");
  require(findLineDatabaseLocations(base).candidates.size()==found.candidates.size(),"junction target must not be traversed");
  fs::remove(linked);
  auto linkedBase=root/L"linked-base",linkedRoot=linkedBase/L"LINE"/L"Data"/L"db";
  fs::create_directories(linkedRoot.parent_path());
  command=L"cmd.exe /d /c mklink /J \""+linkedRoot.wstring()+L"\" \""+outside.wstring()+L"\" >nul 2>&1";
  require(_wsystem(command.c_str())==0,"create owned root-junction fixture");
  require(findLineDatabaseLocations(linkedBase).candidates.empty(),"a junction at the known db root must not be followed");fs::remove(linkedRoot);
  require(findLineDatabaseLocations(fs::path(LR"(\\server\share\local-data)")).searchedDirectories.empty(),"UNC environment must not trigger any network search");
#else
  fs::create_directory_symlink(outside,db/"linked-directory");
  fs::create_symlink(outside/"outside.edb",db/"linked-file.edb");
  require(findLineDatabaseLocations(base).candidates.size()==found.candidates.size(),"symlink files and directories are not followed");
  fs::remove(db/"linked-directory");fs::remove(db/"linked-file.edb");
  auto linkedBase=root/"linked-base",linkedRoot=linkedBase/"LINE"/"Data"/"db";
  fs::create_directories(linkedRoot.parent_path());fs::create_directory_symlink(outside,linkedRoot);
  require(findLineDatabaseLocations(linkedBase).candidates.empty(),"a symlink at the known db root must not be followed");fs::remove(linkedRoot);
#endif
  auto capBase=root/"many-accounts",capDb=capBase/"LINE"/"Data"/"db";
  for(unsigned index=0;index<70;++index){auto path=capDb/("account-"+std::to_string(index)+".edb");file(path);fs::last_write_time(path,modified+std::chrono::seconds(index));}
  auto capped=findLineDatabaseLocations(capBase);
  require(capped.limited&&capped.candidates.size()==64,"candidate count is bounded and incomplete scan is disclosed");
  require(capped.candidates.front().path==capDb/"account-69.edb","newest candidate survives capped result sorting");
  require(capped.candidates.back().path==capDb/"account-6.edb","cap retains most recently modified candidates without auto-selection");
  std::cout<<checks<<" LINE database location discovery checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
