#pragma once
#include "core.hpp"

namespace atlas {
// Statically linked wxSQLite3 AES128 reader, separate from the archive Db.
// Production connections are always read-only. The write flag is for owned fixtures.
class LineCipher {
 void* handle=nullptr;bool fixture=false;
 public:
 explicit LineCipher(const fs::path&,const std::string& passphrase,Control* control=nullptr,bool createForTest=false);
 ~LineCipher();
 LineCipher(const LineCipher&)=delete;LineCipher& operator=(const LineCipher&)=delete;
 J rows(const std::string&,const J& args=J::array(),Tick={});
 // Checks source access and supported encrypted-file shape before a memory scan.
 static void preflight(const fs::path&,Control* control=nullptr);
 static std::string version();
};
}
