#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
namespace atlas {
// An explicit token SID, not the OWNER RIGHTS placeholder, identifies this user.
class PrivateSecurity {
 PSECURITY_DESCRIPTOR descriptor=nullptr;
 public: SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES),nullptr,FALSE};
 std::wstring sid;
 PrivateSecurity(){
  HANDLE token=nullptr; if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))throw Error("無法確認目前使用者權限。");
  DWORD size=0;GetTokenInformation(token,TokenUser,nullptr,0,&size);std::vector<unsigned char> data(size);
  if(!GetTokenInformation(token,TokenUser,data.data(),size,&size)){CloseHandle(token);throw Error("無法確認目前使用者權限。");}CloseHandle(token);
  LPWSTR value=nullptr;if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid,&value))throw Error("無法確認目前使用者權限。");sid=value;LocalFree(value);
  auto sddl=L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;"+sid+L")";
  if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&descriptor,nullptr))throw Error("無法建立私人保存權限。");attributes.lpSecurityDescriptor=descriptor;
 }
 ~PrivateSecurity(){if(descriptor)LocalFree(descriptor);}
 PrivateSecurity(const PrivateSecurity&)=delete;
};
inline std::wstring finalHandlePath(HANDLE h){
 DWORD n=GetFinalPathNameByHandleW(h,nullptr,0,FILE_NAME_NORMALIZED);if(!n)throw Error("無法驗證保存位置。");
 std::wstring actual(n,L'\0');DWORD used=GetFinalPathNameByHandleW(h,actual.data(),n,FILE_NAME_NORMALIZED);if(!used||used>=n)throw Error("無法驗證保存位置。");actual.resize(used);return actual;
}
// Snapshot the resolved parent BEFORE opening/creating the target. This handles
// subst drives and short directory aliases without re-resolving after the open.
inline fs::path expectedFilePath(const fs::path&p){
 auto full=fs::absolute(p).lexically_normal();noLinks(full);
 auto parent=full.parent_path();
 HANDLE h=CreateFileW(parent.c_str(),0,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)throw Error("無法驗證保存位置。");
 std::wstring resolved;
 try{BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))throw Error("保存位置包含連結。");resolved=finalHandlePath(h);}catch(...){CloseHandle(h);throw;}CloseHandle(h);
 // An existing final-component 8.3 alias may expand too. New filenames stay exact.
 auto filename=full.filename();DWORD n=GetLongPathNameW(full.c_str(),nullptr,0);
 if(n){std::wstring longName(n,L'\0');DWORD used=GetLongPathNameW(full.c_str(),longName.data(),n);if(!used||used>=n)throw Error("無法驗證保存位置。");longName.resize(used);filename=fs::path(longName).filename();}
 return filename.empty()?fs::path(resolved):fs::path(resolved)/filename;
}
inline void verifyFileHandle(HANDLE h,const fs::path&p){
 BY_HANDLE_FILE_INFORMATION info{};
 if(!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))throw Error("保存位置包含連結或無法驗證。");
 // Resolve the intended path and compare it to the object actually opened.
 auto expected=p.wstring();
 auto actual=finalHandlePath(h);
 auto normalize=[](std::wstring s){if(s.rfind(L"\\\\?\\UNC\\",0)==0)s=L"\\\\"+s.substr(8);else if(s.rfind(L"\\\\?\\",0)==0)s=s.substr(4);return s;};
 expected=normalize(expected);actual=normalize(actual);
 if(CompareStringOrdinal(expected.c_str(),-1,actual.c_str(),-1,TRUE)!=CSTR_EQUAL)throw Error("保存位置在操作途中改變。");
}
inline void requirePrivateDirectory(const fs::path&p){
 const auto expected=expectedFilePath(p);
 HANDLE h=CreateFileW(p.c_str(),READ_CONTROL,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
 if(h==INVALID_HANDLE_VALUE)throw Error("無法驗證收藏資料夾權限。");
 PSECURITY_DESCRIPTOR sd=nullptr;PACL acl=nullptr;
 try{verifyFileHandle(h,expected);PrivateSecurity security;
  if(GetSecurityInfo(h,SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&acl,nullptr,&sd)!=ERROR_SUCCESS||!acl)throw Error("收藏資料夾沒有私人存取權限，請選擇新的私人資料夾。");
  bool inheritableOwner=false;BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(h,&info))throw Error("無法驗證收藏權限。");
  for(DWORD i=0;i<acl->AceCount;i++){void*entry=nullptr;if(!GetAce(acl,i,&entry))throw Error("無法驗證收藏權限。");auto head=static_cast<ACE_HEADER*>(entry);
   if(head->AceType==ACCESS_DENIED_ACE_TYPE)continue;
   if(head->AceType!=ACCESS_ALLOWED_ACE_TYPE)throw Error("收藏資料夾權限不受支援，請選擇新的私人資料夾。");
   auto ace=static_cast<ACCESS_ALLOWED_ACE*>(entry);LPWSTR value=nullptr;if(!ConvertSidToStringSidW(&ace->SidStart,&value))throw Error("無法驗證收藏權限。");std::wstring sid=value;LocalFree(value);
   if(sid==security.sid && (ace->Mask&FILE_ALL_ACCESS)==FILE_ALL_ACCESS &&
      (head->AceFlags&(OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE))==(OBJECT_INHERIT_ACE|CONTAINER_INHERIT_ACE) &&
      !(head->AceFlags&(INHERIT_ONLY_ACE|NO_PROPAGATE_INHERIT_ACE)))inheritableOwner=true;
   if(ace->Mask && sid!=security.sid && sid!=L"S-1-5-18" && sid!=L"S-1-5-32-544")throw Error("此收藏資料夾允許其他帳號存取，請選擇新的私人資料夾。");
  }
  if((info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)&&!inheritableOwner)throw Error("收藏資料夾缺少可繼承的私人權限，請選擇新的私人資料夾。");
 }catch(...){if(sd)LocalFree(sd);CloseHandle(h);throw;}
 if(sd)LocalFree(sd);CloseHandle(h);
}
}
#endif
