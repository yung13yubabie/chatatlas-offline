#pragma once
#include "core.hpp"
#include "line_process.hpp"

namespace atlas {
// UI identities are unique across databases. The archive receives each source's
// original account/chat identity, so switching between single and batch reads
// neither merges accounts nor duplicates an already collected chat.
class LineBatchSource final : public Source {
 struct Route {std::shared_ptr<Source> source;J original,visible;};
 std::vector<std::shared_ptr<Source>> sources;
 std::vector<J> originals;
 std::map<std::string,Route> routes;
 J catalog=J::array();
 const Route& route(const J&)const;
 public:
 explicit LineBatchSource(std::vector<std::shared_ptr<Source>>,Control* =nullptr);
 size_t sourceCount()const noexcept{return sources.size();}
 J chats()override;
 J chatsWithControl(Control&)override;
 std::string accountFor(const J&)const override;
 J storageChat(const J&)const override;
 Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control&)override;
 fs::path download(const J&,const J&,const fs::path&,Control&)override;
};

struct LineBatchEntry {
 fs::path path;
 // ready, duplicate, invalid, unsupported, unavailable. No keys or session data.
 std::string status,reason;
};
struct LineBatchOpenResult {
 std::shared_ptr<LineBatchSource> source;
 std::vector<LineBatchEntry> entries;
 size_t readyCount()const;
 size_t skippedCount()const{return entries.size()-readyCount();}
 std::string summary()const;
};
using LineBatchKeyVisitor=std::function<bool(LineKeyCandidates&,std::shared_ptr<LineSession>,Tick)>;
using LineBatchKeyScanner=std::function<void(Control&,const LineBatchKeyVisitor&,std::function<void(std::string)>)>;
// Explicitly selected paths or the bounded locator's result only. One scan per
// verified LINE process is shared across files; keys and sessions never cross
// process boundaries. An injected scanner is for owned synthetic fixtures.
LineBatchOpenResult openLineDatabases(const std::vector<fs::path>&,Control&,
 std::function<void(std::string)> progress={},LineBatchKeyScanner scanner={});
}
