// Owned local fixtures; invalid UNC spelling must fail before any server access.
// This does not create SMB shares or claim live NAS/SMB identity validation.
#include "core.hpp"
#include "file.hpp"
#include <iostream>
using namespace atlas;
static unsigned checks=0;
static void expect(bool ok,const char* message){++checks;if(!ok)throw Error(message);}
template<class F>static void remoteRejected(F fn){bool caught=false;try{fn();}catch(const Error& e){caught=std::string(e.what()).find("網路共用")!=std::string::npos;}expect(caught,"Remote destination did not fail at the explicit preflight");}
int main(int argc,char** argv){
 if(argc!=2)return 2;const auto root=fs::path(argv[1])/L"private-path-fixture";
 if(fs::exists(root))return 2;
 try{
  for(auto spelling:{LR"(\\chatatlas-invalid.invalid\share\never-created)",LR"(\\?\UNC\chatatlas-invalid.invalid\share\never-created)",LR"(\\?\unc\chatatlas-invalid.invalid\share\never-created)"}){
   fs::path p(spelling);remoteRejected([&]{requireLocalPrivatePath(p);});remoteRejected([&]{privateDir(p);});remoteRejected([&]{ExclusiveFile out(p);});remoteRejected([&]{Lock lock(p);});
  }
  // A missing descendant must be checked without creating it during preflight.
  requireLocalPrivatePath(root/L"not-created"/L"deeper");expect(!fs::exists(root),"Preflight created a folder");
  privateDir(root);requirePrivateDirectory(root);
  requireLocalPrivatePath(root);requireLocalPrivatePath(root/L"new.txt");
  auto extended=root.wstring();if(extended.rfind(L"\\\\?\\",0)!=0)extended=L"\\\\?\\"+extended;
  requireLocalPrivatePath(fs::path(extended));requireLocalPrivatePath(fs::path(extended)/L"new.txt");
  {ExclusiveFile out(root/L"positive.txt");out.write("positive control");out.sync();}
  expect(readText(root/L"positive.txt")=="positive control","Local private write/read regressed");
  {Lock lock(root);}expect(fs::exists(root/L".vault-lock"),"Local lock creation regressed");
  bool unknownTested=false;
  for(wchar_t letter=L'Z';letter>=L'P';--letter){std::wstring drive{letter,L':',L'\\'};if(GetDriveTypeW(drive.c_str())!=DRIVE_NO_ROOT_DIR)continue;bool caught=false;try{requireLocalPrivatePath(fs::path(drive)/L"never-created");}catch(const Error& e){caught=std::string(e.what()).find("磁碟類型")!=std::string::npos;}expect(caught,"Unknown/nonexistent drive accepted");unknownTested=true;break;}
  if(!unknownTested)std::cout<<"SKIP unavailable-drive fixture: all candidate letters allocated\n";
  fs::remove_all(root);std::cout<<"Windows private path preflight: "<<checks<<" passed; live SMB/mapped-remote scenario not exercised\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove_all(root,ec);return 1;}
}
