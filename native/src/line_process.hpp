#pragma once
#include "core.hpp"
#include <memory>

namespace atlas {
// Wipes a caller-owned passphrase after the encrypted reader has consumed it.
void eraseLineKey(std::string&) noexcept;

// Credentials observed in the same signed LINE process whose key opens the
// selected database. None of these values may enter JSON, settings or logs.
class LineSession final {
 struct Impl;std::unique_ptr<Impl> impl;
 explicit LineSession(const std::string&,const std::string&,const std::string&);
 friend class LineSessionCandidates;
 friend std::string findLineDatabaseKey(const fs::path&,Control&,
  const std::function<bool(const std::string&)>&,std::function<void(std::string)>,std::shared_ptr<LineSession>*);
 public:
 ~LineSession();
 LineSession(const LineSession&)=delete;LineSession& operator=(const LineSession&)=delete;
 const std::string& accessToken() const noexcept;
 const std::string& obsAccessToken() const noexcept;
 const std::string& application() const noexcept;
 bool current() const noexcept;
};

// Bounded recognizer for explicitly labelled HTTP contexts, including UTF-16LE.
// A fixture result has no process binding and current() therefore returns false.
class LineSessionCandidates final {
 struct Impl;std::unique_ptr<Impl> impl;
 public:
 LineSessionCandidates();~LineSessionCandidates();
 LineSessionCandidates(const LineSessionCandidates&)=delete;LineSessionCandidates& operator=(const LineSessionCandidates&)=delete;
 void append(const unsigned char*,size_t,bool endOfRegion=false);
 std::shared_ptr<LineSession> preferred() const;
};

// Streaming ASCII / UTF-16LE recognizer. It retains at most 68 input bytes and
// securely erases candidate storage; no key enumeration or diagnostic API exists.
class LineKeyCandidates final {
 struct Impl;std::unique_ptr<Impl> impl;
 public:
 explicit LineKeyCandidates(size_t maximum=8192);
 ~LineKeyCandidates();
 LineKeyCandidates(const LineKeyCandidates&)=delete;
 LineKeyCandidates& operator=(const LineKeyCandidates&)=delete;
 void append(const unsigned char*,size_t,bool endOfRegion=false);
 size_t size() const;
 std::string findMatch(Control&,const std::function<bool(const std::string&)>&,Tick={});
};

// Read-only, current-user Windows LINE processes with a verified signed publisher.
// The probe must validate the candidate against the explicitly selected database.
// The returned key belongs to the caller and must be erased with eraseLineKey().
std::string findLineDatabaseKey(const fs::path& database,Control&,
 const std::function<bool(const std::string&)>& probe,
 std::function<void(std::string)> progress={},std::shared_ptr<LineSession>* session=nullptr);
}
