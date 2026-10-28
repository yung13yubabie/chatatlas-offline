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
int main(){
 auto root=fs::temp_directory_path()/path8("chatatlas-acl-test-"+std::to_string(GetCurrentProcessId()));
 if(fs::exists(root))return 2;
 try{
  privateDir(root);requirePrivateDirectory(root);
  auto before=dacl(root);privateDir(root);expect(dacl(root)==before,"Existing parent DACL changed");
  {ExclusiveFile out(root/L"test.txt");out.write("synthetic");out.sync();}
  requirePrivateDirectory(root);requirePrivateDirectory(root/L"test.txt");
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
  auto command=L"cmd.exe /d /c mklink /J \""+junction.wstring()+L"\" \""+target.wstring()+L"\" >nul 2>&1";
  if(_wsystem(command.c_str())==0){
   rejected([&]{requirePrivateDirectory(junction);});
   rejected([&]{Store shouldReject(junction);});
   fs::remove(junction);std::cout<<"Junction rejection passed\n";
  }else std::cout<<"SKIP junction fixture: mklink /J unsupported or denied\n";
  expect(dacl(root)==before,"Parent DACL changed during tests");
  fs::remove_all(root);std::cout<<"Windows ACL creation, reopen, parent preservation, broad-child rejection passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';std::error_code ec;fs::remove_all(root,ec);return 1;}
}
