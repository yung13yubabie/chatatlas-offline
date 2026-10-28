#pragma once
#include "core.hpp"
#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <limits>
#include <ios>
#include <sys/stat.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include "windows_files.hpp"
namespace atlas {
// Reads use the SAME validated handle throughout. Final reparse points are
// rejected; Windows writers/deleters cannot share this reader. Ancestor races
// and path-based consumers such as SQLite are not a blanket security guarantee.
class VerifiedInput {
 FILE* file=nullptr;std::streamsize count=0;uint64_t length=0;
 public:
 explicit VerifiedInput(const fs::path&p){
  noLinks(p);int fd=-1;
#ifdef _WIN32
  const auto expected=expectedFilePath(p);
  HANDLE h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
  if(h==INVALID_HANDLE_VALUE)throw Error("檔案無法讀取或仍在使用中。");
  try{verifyFileHandle(h,expected);BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
   if(GetFileType(h)!=FILE_TYPE_DISK||!GetFileInformationByHandle(h,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||!GetFileSizeEx(h,&size)||size.QuadPart<0)throw Error("附件不是一般檔案。");
   length=static_cast<uint64_t>(size.QuadPart);
  }catch(...){CloseHandle(h);throw;}
  fd=_open_osfhandle(reinterpret_cast<intptr_t>(h),_O_BINARY|_O_RDONLY);if(fd<0)CloseHandle(h);
#else
  fd=::open(p.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd>=0){struct stat info{};if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_size<0){::close(fd);throw Error("附件不是一般檔案。");}length=static_cast<uint64_t>(info.st_size);}
#endif
  if(fd<0)throw Error("檔案無法讀取。");file=fdopen(fd,"rb");if(!file){
#ifdef _WIN32
   _close(fd);
#else
   ::close(fd);
#endif
   throw Error("檔案無法讀取。");}
 }
 ~VerifiedInput(){if(file)std::fclose(file);}
 VerifiedInput(const VerifiedInput&)=delete;VerifiedInput&operator=(const VerifiedInput&)=delete;
 uint64_t size()const{return length;}
 explicit operator bool()const{return file&&!std::feof(file)&&!std::ferror(file);}
 VerifiedInput&read(char*data,size_t n){if(n>static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))throw Error("讀取範圍過大。");count=static_cast<std::streamsize>(std::fread(data,1,n,file));return *this;}
 std::streamsize gcount()const{return count;}
 bool bad()const{return std::ferror(file)!=0;}
};
#ifdef _WIN32
// Only call for a handle returned by this operation's successful CREATE_NEW.
// Never fall back to deleting by pathname: it may no longer name our object.
// Best effort: if the filesystem refuses disposition, retain the original error
// and leave the empty artifact rather than risk deleting an unrelated file.
inline bool discardCreatedFile(HANDLE h) noexcept {
 const DWORD error=GetLastError();const int savedErrno=errno;
 FILE_DISPOSITION_INFO disposition{TRUE};
 const bool removed=SetFileInformationByHandle(h,FileDispositionInfo,&disposition,sizeof(disposition))!=FALSE;
 SetLastError(error);errno=savedErrno;return removed;
}
#endif
class ExclusiveFile{FILE*file=nullptr;public:explicit ExclusiveFile(const fs::path&p){noLinks(p);int fd=-1;
#ifdef _WIN32
 const auto expected=expectedFilePath(p); PrivateSecurity security; HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE|DELETE,0,&security.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h!=INVALID_HANDLE_VALUE){try{verifyFileHandle(h,expected);}catch(...){discardCreatedFile(h);CloseHandle(h);throw;}fd=_open_osfhandle(reinterpret_cast<intptr_t>(h),_O_BINARY|_O_WRONLY);if(fd<0){discardCreatedFile(h);CloseHandle(h);}}
#else
 fd=::open(p.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600);
#endif
 if(fd<0)throw Error("同名檔案已存在或無法保存。");file=fdopen(fd,"wb");if(!file){
#ifdef _WIN32
 discardCreatedFile(reinterpret_cast<HANDLE>(_get_osfhandle(fd)));_close(fd);
#else
 ::close(fd);
#endif
 throw Error("檔案無法保存。");}}
 ~ExclusiveFile(){if(file)std::fclose(file);}void write(const void*p,size_t n){if(n&&std::fwrite(p,1,n,file)!=n)throw Error("可用空間不足，保存未完成。");}void write(const std::string&s){write(s.data(),s.size());}void sync(){if(std::fflush(file))throw Error("檔案保存未完成。");
#ifdef _WIN32
 if(!FlushFileBuffers(reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)))))throw Error("檔案保存未完成。");
#else
 if(::fsync(fileno(file)))throw Error("檔案保存未完成。");
#endif
 }
};
inline void publishNew(const fs::path&from,const fs::path&to){noLinks(to);
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH))throw Error("目的地已有檔案或無法完成保存，請改用其他名稱。");
#else
 if(::link(from.c_str(),to.c_str()))throw Error("目的地已有檔案或無法完成保存，請改用其他名稱。");::unlink(from.c_str());
#endif
}
}
