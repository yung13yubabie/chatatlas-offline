#pragma once
#include "core.hpp"
#include <memory>

namespace atlas {
class LineSession;
// Only explicit metadata URLs are supported. Relative OBS paths are anchored to
// LINE's documented OBS host; no object URL or authentication token is invented.
std::string normalizeLineMediaUrl(const std::string&);
// Native media-key derivation, also checked against independent synthetic vectors.
std::array<unsigned char,76> deriveLineMediaKeys(const std::string& keyMaterialBase64);
void decryptLineMediaFile(const fs::path& ciphertext,const fs::path& destination,const std::string& keyMaterialBase64,Control&,bool video=false);
std::string lineMediaTalkMeta(const std::string& messageId);
std::string lineMediaObjectUrl(const std::string& service,const std::string& messageId,bool preview=false);
class LineMediaResolver {
 struct Binding { fs::path file; std::string url; J descriptor; std::string keyMaterial; bool privateResource=false; std::string messageId; };
 fs::path database,root;
 std::shared_ptr<LineSession> session;
 std::string cachedObsToken;
 std::chrono::steady_clock::time_point tokenTime{};
 std::map<std::string,std::string> chatServices;
 std::map<std::string,fs::path> selected;
 std::map<std::string,Binding> downloads;
 public:
 explicit LineMediaResolver(const fs::path& selectedDatabase,const fs::path& mediaRoot={});
 ~LineMediaResolver();
 LineMediaResolver(const LineMediaResolver&)=delete;
 LineMediaResolver& operator=(const LineMediaResolver&)=delete;
 void setSession(std::shared_ptr<LineSession>);
 void setChatService(const std::string& chatId,const std::string& service);
 void bindFile(const std::string& chatId,const std::string& messageId,const fs::path&,bool thumbnail=false,Control* =nullptr);
 void bindRelativeFile(const std::string& chatId,const std::string& messageId,const std::string& relative,bool thumbnail=false,Control* =nullptr);
 J attachments(const std::string& chatId,const J& rawMessage,Control* =nullptr);
 fs::path copy(const std::string& chatId,const J& descriptor,const fs::path& stage,Control&);
};
// Original-file fill-in for an already collected, explicitly selected row.
J pendingLineAttachments(Store&,size_t limit=1000);
void importLineAttachment(Store&,const std::string& attachmentId,const fs::path& original,Control&);
}
