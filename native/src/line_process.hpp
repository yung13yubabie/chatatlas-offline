#pragma once
#include "core.hpp"
#include <memory>

namespace atlas {
// Wipes a caller-owned passphrase after the encrypted reader has consumed it.
void eraseLineKey(std::string&) noexcept;

class LineKeyCandidates;
class LineSession;
using LineDatabaseKeyVisitor=std::function<bool(LineKeyCandidates&,std::shared_ptr<LineSession>,Tick)>;

// Credentials observed in one signed LINE process. Associate them with a
// database only after a key from that same process opens it. None of these
// values may enter JSON, settings or logs.
class LineSession final {
 struct Impl;std::unique_ptr<Impl> impl;
 explicit LineSession(const std::string&,const std::string&,const std::string&);
 friend class LineSessionCandidates;
 friend void visitLineDatabaseKeys(Control&,const LineDatabaseKeyVisitor&,
  std::function<void(std::string)>,bool);
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

// Scan each verified, current-user/session LINE process once. The visitor receives
// only that process's candidate pool and observed session; credentials from other
// processes are never combined. Return true to stop after all requested sources
// have matched. A normal return does not mean any database was readable.
// The borrowed pool and deadline check are valid only during this callback and
// must not be retained. observeSession=false is for callers without media access.
void visitLineDatabaseKeys(Control&,const LineDatabaseKeyVisitor&,
 std::function<void(std::string)> progress={},bool observeSession=true);

// Read-only, current-user Windows LINE processes with a verified signed publisher.
// The probe must validate the candidate against the explicitly selected database.
// The returned key belongs to the caller and must be erased with eraseLineKey().
std::string findLineDatabaseKey(const fs::path& database,Control&,
 const std::function<bool(const std::string&)>& probe,
 std::function<void(std::string)> progress={},std::shared_ptr<LineSession>* session=nullptr);
}
