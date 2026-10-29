#pragma once
#include "core.hpp"

namespace atlas {
// Immutable, bounded snapshot of the authorized line-summary public history API.
// This source does not open LINE databases or retrieve media files.
class LineSummarySource final : public Source {
 J chat,items=J::array();
 std::vector<std::string> instants;
 std::string from,to,preciseFrom,preciseTo;
 public:
 explicit LineSummarySource(const fs::path&,Control* control=nullptr);
 J chats() override;
 Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control&) override;
 fs::path download(const J&,const J&,const fs::path&,Control&) override;
 const std::string& since() const {return from;}
 const std::string& until() const {return to;}
};
}
