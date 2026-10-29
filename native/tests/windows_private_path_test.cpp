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
 const char* phase="start";auto stage=[&](const char* name){phase=name;std::cout<<"Private path phase: "<<name<<std::endl;};
 try{
  stage("extended DOS normalization and root parents (no filesystem)");
  for(const auto spelling:{LR"(\\?\C:\)",LR"(\\?\C:\one\..\two)",LR"(\\?\C:\one\two\..)",LR"(\\?\c:\mixed\folder)"}){
   auto normalized=absoluteNormalizedPath(fs::path(spelling));
   expect(privatePathKind(normalized.wstring())==PrivatePathKind::drive,"Extended DOS lost absolute drive root");
   expect(normalized.wstring().rfind(L"\\\\?\\",0)==0,"Extended DOS prefix lost");
   expect(absoluteNormalizedPath(normalized)==normalized,"Extended DOS normalization not stable");
  }
  expect(absoluteNormalizedPath(fs::path(LR"(\\?\C:\one\..\two)")).wstring()==LR"(\\?\C:\two)","Extended DOS dot normalization changed drive semantics");
  expect(parentDirectoryPath(fs::path(LR"(\\?\C:\)"))==fs::path(LR"(\\?\C:\)"),"Extended drive root parent escaped volume");
  expect(parentDirectoryPath(fs::path(LR"(\\?\C:\child)"))==fs::path(LR"(\\?\C:\)"),"Extended child parent became drive-relative");
  expect(parentDirectoryPath(fs::path(LR"(\\?\C:\child\)"))==fs::path(LR"(\\?\C:\child)"),"Extended trailing separator parent skipped intended directory");
  stage("extended UNC lexical preservation without server access");
  expect(absoluteNormalizedPath(fs::path(LR"(\\?\unc\server\share\one\..\two)")).wstring()==LR"(\\?\UNC\server\share\two)","Extended UNC changed share/root");
  auto uncRoot=absoluteNormalizedPath(fs::path(LR"(\\?\UNC\server\share)"));
  expect(parentDirectoryPath(uncRoot)==uncRoot,"Extended UNC root parent escaped share");
  bool escaped=false;try{absoluteNormalizedPath(fs::path(LR"(\\?\UNC\server\share\..\other)"));}catch(const Error& e){escaped=std::string(e.what()).find("共用根目錄")!=std::string::npos;}expect(escaped,"Extended UNC normalization crossed share boundary");
  stage("ordinary relative and drive-relative normalization unchanged");
  for(const auto spelling:{L"relative\\..\\folder",L"C:relative",L"C:\\folder\\..\\next"})expect(absoluteNormalizedPath(fs::path(spelling))==fs::absolute(fs::path(spelling)).lexically_normal(),"Ordinary path behavior changed");
  stage("remote private creation rejected before server access");
  for(auto spelling:{LR"(\\chatatlas-invalid.invalid\share\never-created)",LR"(\\?\UNC\chatatlas-invalid.invalid\share\never-created)",LR"(\\?\unc\chatatlas-invalid.invalid\share\never-created)"}){
   fs::path p(spelling);remoteRejected([&]{requireLocalPrivatePath(p);});remoteRejected([&]{privateDir(p);});remoteRejected([&]{ExclusiveFile out(p);});remoteRejected([&]{Lock lock(p);});
  }
  // A missing descendant must be checked without creating it during preflight.
  stage("local missing descendant preflight");
  requireLocalPrivatePath(root/L"not-created"/L"deeper");expect(!fs::exists(root),"Preflight created a folder");
  stage("local directory creation and ACL positive control");
  privateDir(root);requirePrivateDirectory(root);
  stage("local existing directory and missing filename");
  requireLocalPrivatePath(root);requireLocalPrivatePath(root/L"new.txt");
  auto extended=root.wstring();if(extended.rfind(L"\\\\?\\",0)!=0)extended=L"\\\\?\\"+extended;
  stage("extended existing directory and missing filename");
  requireLocalPrivatePath(fs::path(extended));requireLocalPrivatePath(fs::path(extended)/L"new.txt");
  requireLocalPrivatePath(fs::path(extended+L"\\"));requirePrivateDirectory(fs::path(extended+L"\\"));
  stage("extended long-path creation and verified IO");
  auto longPath=fs::path(extended);while(longPath.wstring().size()<320)longPath/=L"owned-long-path-segment";
  privateDir(longPath);requirePrivateDirectory(longPath);
  {ExclusiveFile out(longPath/L"long.txt");out.write("long positive");out.sync();}
  expect(readText(longPath/L"long.txt")=="long positive","Extended long-path verified IO failed");
  expect(within(longPath,"inside.txt")==absoluteNormalizedPath(longPath/L"inside.txt"),"within lost extended path identity");
  expect(relativeToDirectory(longPath/L"long.txt",root).has_value(),"Extended path identity containment failed");
  stage("8.3 alias positive control when available");
  DWORD shortSize=GetShortPathNameW(root.c_str(),nullptr,0);
  if(shortSize){std::wstring shortName(shortSize,L'\0');auto used=GetShortPathNameW(root.c_str(),shortName.data(),shortSize);expect(used&&used<shortSize,"Short alias lookup failed");shortName.resize(used);requireLocalPrivatePath(fs::path(shortName));expect(relativeToDirectory(fs::path(shortName),root)==fs::path("."),"Short alias identity changed");}else std::cout<<"SKIP short-name lookup unavailable\n";
  stage("ordinary local file and lock creation");
  {ExclusiveFile out(root/L"positive.txt");out.write("positive control");out.sync();}
  expect(readText(root/L"positive.txt")=="positive control","Local private write/read regressed");
  {Lock lock(root);}expect(fs::exists(root/L".vault-lock"),"Local lock creation regressed");
  bool unknownTested=false;
  stage("unknown drive fails closed");
  for(wchar_t letter=L'Z';letter>=L'P';--letter){std::wstring drive{letter,L':',L'\\'};if(GetDriveTypeW(drive.c_str())!=DRIVE_NO_ROOT_DIR)continue;bool caught=false;try{requireLocalPrivatePath(fs::path(drive)/L"never-created");}catch(const Error& e){caught=std::string(e.what()).find("磁碟類型")!=std::string::npos;}expect(caught,"Unknown/nonexistent drive accepted");unknownTested=true;break;}
  if(!unknownTested)std::cout<<"SKIP unavailable-drive fixture: all candidate letters allocated\n";
  fs::remove_all(fs::path(extended));std::cout<<"Windows private path preflight: "<<checks<<" passed; live SMB/mapped-remote scenario not exercised\n";
 }catch(const std::exception& e){std::cerr<<"Private path failed in phase ["<<phase<<"] after "<<checks<<" checks: "<<e.what()<<'\n';std::error_code ec;auto cleanup=root.wstring();if(cleanup.rfind(L"\\\\?\\",0)!=0)cleanup=L"\\\\?\\"+cleanup;fs::remove_all(fs::path(cleanup),ec);return 1;}
}
