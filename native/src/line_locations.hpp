#pragma once
#include "core.hpp"

namespace atlas {
// Metadata only. A candidate is not proof of the account or database format;
// opening the user's explicit selection still goes through the cipher/schema checks.
struct LineDatabaseCandidate {
 fs::path path;
 uintmax_t size=0;
 fs::file_time_type modified{};
 bool hasModified=false;
 std::string kind="LINE 對話資料庫候選";
};
struct LineDatabaseLocations {
 fs::path defaultDirectory;
 std::vector<fs::path> searchedDirectories;
 std::vector<LineDatabaseCandidate> candidates;
 bool limited=false;
};
// Searches only LINE/Data/db and at most two account-subdirectory levels.
// Never chooses an account, reads file contents, follows links, or creates files.
LineDatabaseLocations findLineDatabaseLocations(const fs::path& localAppData,Tick={});
LineDatabaseLocations currentLineDatabaseLocations(Tick={});
}
