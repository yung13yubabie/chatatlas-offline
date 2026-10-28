#pragma once
#include "core.hpp"
namespace atlas {
struct ImportResult { fs::path root,retained; std::string warning; };
class ImportFailure : public Error {
 public: fs::path retained;bool cancelled;
 ImportFailure(std::string message,fs::path path,bool stopped=false):Error(std::move(message)),retained(std::move(path)),cancelled(stopped){}
};
// Reads an extracted, closed backup. Never changes source ACLs or imports its SQL/HTML code.
ImportResult importBackup(const fs::path& source,const fs::path& newDestination,Tick={});
}
