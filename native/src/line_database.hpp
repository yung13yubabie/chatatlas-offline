#pragma once
#include "core.hpp"
#include <memory>

namespace atlas {
class LineCipher;
class LineMediaResolver;
class LineDatabaseIdentity;
class LineSession;

// Reads one explicitly selected LINE database. The archive uses its independent
// system SQLite connection; no decrypted LINE database is written to disk.
class LineDatabaseSource final : public Source {
 fs::path selectedDatabase;
 std::unique_ptr<LineDatabaseIdentity> identity;
 std::unique_ptr<LineCipher> database;
 std::unique_ptr<LineMediaResolver> media;
 J catalog=J::array();
 std::map<std::string,std::set<std::string>> tables;
 std::map<std::string,std::string> attachmentChats;
 std::map<std::string,std::string> chatServices;
 std::map<std::string,size_t> unknownSenderNumbers;
 bool hasTable(const std::string&) const;
 bool hasColumn(const std::string&,const std::string&) const;
 J nameRows(const std::string&,const std::string&,const std::vector<std::string>&,Control&);
 void verifyIdentity() const;
 void inspectSchema(Control&);
 void loadChats(Control&);
 std::map<std::string,std::string> names(Control&);
 public:
 explicit LineDatabaseSource(const fs::path&,const std::string& passphrase,
                             fs::path mediaRoot={},Control* control=nullptr);
 ~LineDatabaseSource() override;
 J chats() override;
 J chatsWithControl(Control&) override;
 Batch read(const J&,const std::set<std::string>&,const std::string& since,
            const std::string& until,Control&) override;
 fs::path download(const J&,const J&,const fs::path&,Control&) override;
 // Explicitly chosen downloaded originals can be associated without trusting
 // a guessed cache filename. This does not modify LINE or a collection.
 void bindMediaFile(const J& chat,const std::string& messageId,const fs::path&,
                    bool thumbnail=false,Control* control=nullptr);
 const fs::path& databasePath() const {return selectedDatabase;}
 void setSession(std::shared_ptr<LineSession>);
};
}
