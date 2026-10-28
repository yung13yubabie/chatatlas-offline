#pragma once
#include "core.hpp"
#include <cstdio>
#include <fcntl.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include "windows_files.hpp"
namespace atlas {
class ExclusiveFile{FILE*file=nullptr;public:explicit ExclusiveFile(const fs::path&p){noLinks(p);int fd=-1;
#ifdef _WIN32
 const auto expected=fs::canonical(p.parent_path())/p.filename(); PrivateSecurity security; HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,&security.attributes,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h!=INVALID_HANDLE_VALUE){try{verifyFileHandle(h,expected);}catch(...){CloseHandle(h);throw;}fd=_open_osfhandle(reinterpret_cast<intptr_t>(h),_O_BINARY|_O_WRONLY);if(fd<0)CloseHandle(h);}
#else
 fd=::open(p.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600);
#endif
 if(fd<0)throw Error("同名檔案已存在或無法保存。");file=fdopen(fd,"wb");if(!file){
#ifdef _WIN32
 _close(fd);
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
