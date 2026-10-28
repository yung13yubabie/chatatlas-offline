// Artificial fixtures only. No existing user's directory permissions are changed.
#include "core.hpp"
#include <iostream>
#include <cwctype>
#ifdef _WIN32
#include "windows_files.hpp"
#endif
using namespace atlas;
static void expect(bool ok,const char*message){if(!ok)throw Error(message);}
int main(int argc,char**argv){
 if(argc!=2)return 2;
 auto root=fs::path(argv[1])/"path-identity-fixture";
 if(fs::exists(root))return 2;
 try{
  privateDir(root);auto base=privateDir(root/"Collection Long Name"),nested=privateDir(base/"Assets Mixed Case");auto sibling=privateDir(root/"Collection Long Name Other");
  expect(relativeToDirectory(base,base)==fs::path("."),"Same directory identity failed");
  expect(relativeToDirectory(nested/"new.zip",base)==fs::path("Assets Mixed Case/new.zip"),"Child file identity failed");
  expect(relativeToDirectory(nested/"missing"/"deeper"/"new.zip",base)==fs::path("Assets Mixed Case/missing/deeper/new.zip"),"Missing suffix failed");
  expect(!relativeToDirectory(sibling/"new.zip",base),"Sibling prefix accepted as child");
  expect(!relativeToDirectory(root/"outside.zip",base),"Parent accepted as child");
  expect(!relativeToDirectory(base.root_path(),base),"Volume root accepted as child");
#ifdef _WIN32
  expect(!finalDirectoryParent(LR"(\\?\C:\)"),"Extended drive root walked above volume");
  expect(!finalDirectoryParent(LR"(\\?\UNC\server\share)"),"UNC share root walked above share");
  expect(!finalDirectoryParent(LR"(\\?\UNC\server\share\)"),"Trailing UNC root walked above share");
  auto driveChild=finalDirectoryParent(LR"(\\?\C:\child)");expect(driveChild&&driveChild->first==fs::path(LR"(\\?\C:\)")&&driveChild->second==L"child","Extended drive parent incorrect");
  auto uncChild=finalDirectoryParent(LR"(\\?\UNC\server\share\child)");expect(uncChild&&uncChild->first==fs::path(LR"(\\?\UNC\server\share\)")&&uncChild->second==L"child","Extended UNC parent incorrect");
  bool otherVolumeTested=false;DWORD drives=GetLogicalDrives();
  for(wchar_t letter=L'A';letter<=L'Z';++letter){if(!(drives&(1u<<(letter-L'A'))))continue;std::wstring drive{letter,L':',L'\\'};auto kind=GetDriveTypeW(drive.c_str());if(kind!=DRIVE_FIXED&&kind!=DRIVE_REMOVABLE)continue;
   DWORD attr=GetFileAttributesW(drive.c_str());if(attr==INVALID_FILE_ATTRIBUTES||(attr&FILE_ATTRIBUTE_REPARSE_POINT))continue;
   std::error_code compareError;if(fs::equivalent(fs::path(drive),base.root_path(),compareError)||compareError)continue;
   expect(!relativeToDirectory(fs::path(drive),base),"Other drive root accepted as child");otherVolumeTested=true;break;
  }
  std::cout<<(otherVolumeTested?"Other drive outside containment passed\n":"SKIP other drive: unavailable\n");
  auto folded=nested.wstring();for(auto&c:folded)c=std::towlower(c);
  expect(relativeToDirectory(fs::path(folded)/L"new.zip",base)==fs::path("Assets Mixed Case/new.zip"),"Windows case alias failed");
  DWORD size=GetShortPathNameW(nested.c_str(),nullptr,0);
  if(size){std::wstring alias(size,L'\0');DWORD used=GetShortPathNameW(nested.c_str(),alias.data(),size);expect(used&&used<size,"Short path query failed");alias.resize(used);if(alias!=nested.wstring()){expect(relativeToDirectory(fs::path(alias)/L"new.zip",base)==fs::path("Assets Mixed Case/new.zip"),"Short alias containment failed");std::cout<<"8.3 containment passed\n";}else std::cout<<"SKIP 8.3: no short name\n";}
  auto junction=root/L"linked-collection";
  auto command=L"cmd.exe /d /c mklink /J \""+junction.wstring()+L"\" \""+base.wstring()+L"\" >nul 2>&1";
  if(_wsystem(command.c_str())==0){bool rejected=false;try{relativeToDirectory(junction/L"new.zip",base);}catch(const Error&){rejected=true;}expect(rejected,"Junction containment accepted");fs::remove(junction);std::cout<<"Junction containment rejected\n";}else std::cout<<"SKIP junction: fixture unavailable\n";
  struct Mapping{std::wstring drive,target;~Mapping(){if(!drive.empty())DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|DDD_EXACT_MATCH_ON_REMOVE|DDD_NO_BROADCAST_SYSTEM,drive.c_str(),target.c_str());}} mapping;
  for(wchar_t letter=L'Z';letter>=L'P';--letter){std::wstring drive{letter,L':'};wchar_t existing[32768];if(QueryDosDeviceW(drive.c_str(),existing,32768)||GetLastError()!=ERROR_FILE_NOT_FOUND)continue;auto target=L"\\??\\"+nested.wstring();if(DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_NO_BROADCAST_SYSTEM,drive.c_str(),target.c_str())){mapping.drive=drive;mapping.target=target;break;}}
  if(!mapping.drive.empty()){expect(relativeToDirectory(fs::path(mapping.drive+L"\\new.zip"),base)==fs::path("Assets Mixed Case/new.zip"),"Subst containment failed");expect(relativeToDirectory(nested,fs::path(mapping.drive+L"\\"))==fs::path("."),"Subst root identity failed");std::cout<<"Subst containment passed\n";expect(DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|DDD_EXACT_MATCH_ON_REMOVE|DDD_NO_BROADCAST_SYSTEM,mapping.drive.c_str(),mapping.target.c_str()),"Mapping cleanup failed");mapping.drive.clear();}else std::cout<<"SKIP subst: unavailable\n";
#else
  auto lower=privateDir(root/"collection long name");expect(!relativeToDirectory(lower/"new.zip",base),"Distinct case-sensitive directory conflated");
  auto link=root/"linked";fs::create_directory_symlink(base,link);bool rejected=false;try{relativeToDirectory(link/"new.zip",base);}catch(const Error&){rejected=true;}expect(rejected,"Link was accepted");fs::remove(link);
#endif
  fs::remove_all(root);std::cout<<"Physical directory containment and portable relative paths passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove_all(root,ec);return 1;}
}
