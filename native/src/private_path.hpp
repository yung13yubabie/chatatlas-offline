#pragma once
#include <string>
#include <string_view>
namespace atlas {
// Only DOS drive paths have the local-volume semantics used by our private ACLs.
// Classify before any filesystem call so UNC probes do not contact a server.
enum class PrivatePathKind { drive, remote, unsupported };
inline PrivatePathKind privatePathKind(std::wstring_view raw) {
 std::wstring p(raw);for(auto& c:p)if(c==L'/')c=L'\\';
 if(p.find(L'\0')!=std::wstring::npos)return PrivatePathKind::unsupported;
 if(p.rfind(L"\\\\?\\",0)==0){
  auto prefix=p.substr(4,4);for(auto& c:prefix)if(c>=L'a'&&c<=L'z')c-=L'a'-L'A';
  if(prefix==L"UNC\\")return PrivatePathKind::remote;
  p.erase(0,4);
 }else if(p.rfind(L"\\\\.\\",0)==0||p.rfind(L"\\??\\",0)==0)return PrivatePathKind::unsupported;
 else if(p.rfind(L"\\\\",0)==0)return PrivatePathKind::remote;
 return p.size()>=3&&((p[0]>=L'A'&&p[0]<=L'Z')||(p[0]>=L'a'&&p[0]<=L'z'))&&p[1]==L':'&&p[2]==L'\\' ? PrivatePathKind::drive : PrivatePathKind::unsupported;
}
// Numeric values are the stable Win32 DRIVE_* constants; keep policy testable
// without creating a share or changing a user's network/drive configuration.
inline bool localPrivateDriveType(unsigned type){return type==2||type==3||type==5||type==6;}
}
