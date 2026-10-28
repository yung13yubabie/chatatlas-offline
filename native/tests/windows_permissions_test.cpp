// Windows-only owned synthetic fixtures. Never modifies an existing user's ACL.
#include "core.hpp"
#include "windows_files.hpp"
#include "file.hpp"
#include <iostream>
using namespace atlas;
static void expect(bool value,const char*message){if(!value)throw Error(message);}
template<class F>static void rejected(F fn){bool caught=false;try{fn();}catch(const Error&){caught=true;}expect(caught,"Expected permission/reparse rejection");}
static std::wstring dacl(const fs::path&p){
 PSECURITY_DESCRIPTOR sd=nullptr;PACL acl=nullptr;
 if(GetNamedSecurityInfoW(const_cast<wchar_t*>(p.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&sd)!=ERROR_SUCCESS)throw Error("Get DACL failed");
 LPWSTR text=nullptr;if(!ConvertSecurityDescriptorToStringSecurityDescriptorW(sd,SDDL_REVISION_1,DACL_SECURITY_INFORMATION,&text,nullptr)){LocalFree(sd);throw Error("DACL serialization failed");}
 std::wstring result=text;LocalFree(text);LocalFree(sd);return result;
}
static void fixtureDirectory(const fs::path&p,const std::wstring&sddl){
 PSECURITY_DESCRIPTOR sd=nullptr;expect(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr),"Fixture SDDL failed");
 SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};bool made=CreateDirectoryW(p.c_str(),&sa);LocalFree(sd);expect(made,"Fixture directory creation failed");
}
int main(){
 auto root=fs::temp_directory_path()/path8("chatatlas-acl-test-"+std::to_string(GetCurrentProcessId()));
 if(fs::exists(root))return 2;
 try{
  privateDir(root);requirePrivateDirectory(root);
  PrivateSecurity owner;
  for(const auto flags:{L"",L"OI",L"CI",L"OICINP",L"OICIIO"}){
   auto fixture=root/(L"flags-"+std::wstring(flags));
   fixtureDirectory(fixture,L"D:P(A;;FA;;;"+owner.sid+L")(A;"+flags+L";FA;;;"+owner.sid+L")");
   rejected([&]{requirePrivateDirectory(fixture);});
  }
  auto callback=root/L"callback";
  fixtureDirectory(callback,L"D:P(A;OICI;FA;;;"+owner.sid+L")(XA;OICI;FA;;;WD;(@User.Title == \"PM\"))");
  rejected([&]{requirePrivateDirectory(callback);});
  for(const auto name:{"CON","bad?name","bad|name","nul.txt","dir/COM1.dat","Lpt9","COM¹.txt","LPT²","COM³","PRN","AUX.ext","CONIN$","CONOUT$","trailing.","space ","dir./ok","../escape","dir/../escape"})rejected([&]{within(root,name);});
  rejected([&]{within(root,std::string("valid\0hidden",12));});
  expect(within(root,"COM10.txt").filename()==L"COM10.txt","Non-device filename rejected");
  {Lock lock(root);requirePrivateDirectory(root/L".vault-lock");rejected([&]{Lock duplicate(root);});}
  auto longDir=privateDir(root/L"long synthetic directory for alias");
  DWORD shortSize=GetShortPathNameW(longDir.c_str(),nullptr,0);
  if(shortSize){std::wstring shortName(shortSize,L'\0');DWORD used=GetShortPathNameW(longDir.c_str(),shortName.data(),shortSize);expect(used&&used<shortSize,"Short-path lookup failed");shortName.resize(used);if(shortName!=longDir.wstring()){requirePrivateDirectory(fs::path(shortName));{ExclusiveFile out(fs::path(shortName)/L"alias.txt");out.write("synthetic");out.sync();}expect(readText(fs::path(shortName)/L"alias.txt")=="synthetic","Short-path create failed");std::cout<<"8.3 parent alias passed\n";}else std::cout<<"SKIP 8.3 alias: volume does not assign short names\n";}
  auto longFile=longDir/L"long synthetic filename for alias.txt";
  {ExclusiveFile out(longFile);out.write("synthetic");out.sync();}
  DWORD shortFileSize=GetShortPathNameW(longFile.c_str(),nullptr,0);
  if(shortFileSize){std::wstring alias(shortFileSize,L'\0');DWORD used=GetShortPathNameW(longFile.c_str(),alias.data(),shortFileSize);expect(used&&used<shortFileSize,"Short file alias failed");alias.resize(used);if(alias!=longFile.wstring()){requirePrivateDirectory(fs::path(alias));expect(readText(fs::path(alias))=="synthetic","Final short-name read failed");std::cout<<"8.3 final alias passed\n";}else std::cout<<"SKIP final 8.3 alias: volume does not assign short names\n";}
  // A private DOS-device mapping is removed by exact target even on exceptions.
  // Never replace an existing drive mapping; no elevation or persistent settings.
  struct Mapping{std::wstring drive,target;~Mapping(){if(!drive.empty())DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|DDD_EXACT_MATCH_ON_REMOVE|DDD_NO_BROADCAST_SYSTEM,drive.c_str(),target.c_str());}} mapping;
  for(wchar_t letter=L'Z';letter>=L'P';--letter){std::wstring drive{letter,L':'};wchar_t existing[32768];if(QueryDosDeviceW(drive.c_str(),existing,32768)||GetLastError()!=ERROR_FILE_NOT_FOUND)continue;auto target=L"\\??\\"+longDir.wstring();if(DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_NO_BROADCAST_SYSTEM,drive.c_str(),target.c_str())){mapping.drive=drive;mapping.target=target;break;}}
  if(!mapping.drive.empty()){auto alias=fs::path(mapping.drive+L"\\");requirePrivateDirectory(alias);{ExclusiveFile out(alias/L"subst.txt");out.write("synthetic");out.sync();}expect(readText(alias/L"subst.txt")=="synthetic","Subst-style alias read failed");std::cout<<"Subst-style alias passed\n";}else std::cout<<"SKIP subst-style alias: mapping unavailable\n";
  auto before=dacl(root);privateDir(root);expect(dacl(root)==before,"Existing parent DACL changed");
  {ExclusiveFile out(root/L"test.txt");out.write("synthetic");out.sync();}
  requirePrivateDirectory(root);requirePrivateDirectory(root/L"test.txt");
  {VerifiedInput in(root/L"test.txt");expect(in.size()==9,"Validated handle size differs");char bytes[16]{};in.read(bytes,sizeof bytes);expect(in.gcount()==9&&!in.bad()&&std::string(bytes,9)=="synthetic","Validated read differs");
   HANDLE writer=CreateFileW((root/L"test.txt").c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(writer!=INVALID_HANDLE_VALUE)CloseHandle(writer);expect(writer==INVALID_HANDLE_VALUE,"Read handle allowed concurrent writer");
   expect(!DeleteFileW((root/L"test.txt").c_str()),"Read handle allowed deletion");
  }
  rejected([&]{VerifiedInput directory(root);});
  rejected([&]{readText(root/L"test.txt",8);});
  bool cancelled=false;try{digest(root/L"test.txt",[]{throw Cancelled();});}catch(const Cancelled&){cancelled=true;}expect(cancelled,"Read cancellation was swallowed");
  // Cancellation must close the read handle so subsequent writes can proceed.
  {HANDLE writer=CreateFileW((root/L"test.txt").c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);expect(writer!=INVALID_HANDLE_VALUE,"Cancelled read leaked a handle");CloseHandle(writer);}
  auto failedCreated=root/L"failed-constructor.tmp";
  HANDLE created=CreateFileW(failedCreated.c_str(),GENERIC_WRITE|DELETE,0,&owner.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
  expect(created!=INVALID_HANDLE_VALUE,"Owned cleanup fixture creation failed");
  bool mismatch=false;try{verifyFileHandle(created,root/L"not-the-created-file");}catch(const Error&){mismatch=true;}
  SetLastError(ERROR_BAD_ARGUMENTS);errno=EIO;bool discarded=discardCreatedFile(created);bool preserved=GetLastError()==ERROR_BAD_ARGUMENTS&&errno==EIO;CloseHandle(created);
  expect(mismatch&&discarded&&preserved&&!fs::exists(failedCreated),"Owned-handle failure cleanup failed");
  rejected([&]{ExclusiveFile duplicate(root/L"test.txt");});
  expect(readText(root/L"test.txt")=="synthetic","Cleanup altered existing unrelated file");
  auto cannotDiscard=root/L"no-delete-handle.tmp";
  HANDLE limited=CreateFileW(cannotDiscard.c_str(),GENERIC_WRITE,0,&owner.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);expect(limited!=INVALID_HANDLE_VALUE,"Limited cleanup fixture failed");
  bool refused=!discardCreatedFile(limited);CloseHandle(limited);expect(refused&&fs::exists(cannotDiscard),"Cleanup failure did not preserve owned artifact");fs::remove(cannotDiscard);
  auto archive=root/L"archive";{Store store(archive);} {Store reopened(archive);}
  expect(dacl(root)==before,"Store changed existing parent DACL");
  PSECURITY_DESCRIPTOR sd=nullptr;
  expect(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;WD)",SDDL_REVISION_1,&sd,nullptr),"Fixture SDDL failed");
  SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};auto shared=root/L"shared";
  bool made=CreateDirectoryW(shared.c_str(),&sa);expect(made,"Shared fixture creation failed");
  auto badFile=archive/L"broad-child.txt";
  HANDLE h=CreateFileW(badFile.c_str(),GENERIC_WRITE,0,&sa,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);LocalFree(sd);
  expect(h!=INVALID_HANDLE_VALUE,"Broad child fixture failed");CloseHandle(h);
  rejected([&]{requirePrivateDirectory(shared);});
  rejected([&]{Store shouldReject(archive);});
  fs::remove(badFile);{Store reopened(archive);}
  // mklink /J needs no symlink privilege on typical NTFS. Only owned paths.
  auto target=privateDir(root/L"target"),junction=root/L"junction";
  {ExclusiveFile out(target/L"any.txt");out.write("synthetic");out.sync();}
  auto command=L"cmd.exe /d /c mklink /J \""+junction.wstring()+L"\" \""+target.wstring()+L"\" >nul 2>&1";
  if(_wsystem(command.c_str())==0){
   rejected([&]{requirePrivateDirectory(junction);});
   rejected([&]{VerifiedInput linked(junction/L"any.txt");});
   rejected([&]{Store shouldReject(junction);});
   fs::remove(junction);std::cout<<"Junction rejection passed\n";
  }else std::cout<<"SKIP junction fixture: mklink /J unsupported or denied\n";
  expect(dacl(root)==before,"Parent DACL changed during tests");
  if(!mapping.drive.empty()){expect(DefineDosDeviceW(DDD_RAW_TARGET_PATH|DDD_REMOVE_DEFINITION|DDD_EXACT_MATCH_ON_REMOVE|DDD_NO_BROADCAST_SYSTEM,mapping.drive.c_str(),mapping.target.c_str()),"Synthetic drive cleanup failed");mapping.drive.clear();}
  fs::remove_all(root);std::cout<<"Windows ACL creation, reopen, parent preservation, broad-child rejection passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove_all(root,ec);return 1;}
}
