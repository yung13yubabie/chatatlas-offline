#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace atlas {
using J=nlohmann::json;namespace fs=std::filesystem;using Tick=std::function<void()>;
struct Error:std::runtime_error{using std::runtime_error::runtime_error;};
struct IntegrityError:Error{using Error::Error;};
struct CapabilityError:Error{using Error::Error;};
struct RetryableDownload:Error{using Error::Error;};
struct DownloadUnavailable:Error{using Error::Error;};
struct Cancelled:Error{Cancelled():Error("已停止，完成的內容仍會保留。") {}};
struct Control{std::atomic<bool>stopped{false},paused{false};void check()const;void wait(int milliseconds)const;};
class Sha256{std::array<uint32_t,8>h;std::array<unsigned char,64>buffer{};uint64_t length=0;size_t used=0;void block(const unsigned char*);public:Sha256();void update(const void*,size_t);std::string finish();};
std::string sha(std::string_view),key(const std::vector<std::string>&),now(),utf8(const fs::path&),safeName(std::string),base64(std::string_view),readText(const fs::path&,uint64_t max=64*1024*1024),safeJson(const J&);
fs::path path8(const std::string&),privateDir(fs::path),within(const fs::path&,const std::string&);
// Uses filesystem identity, not case-folded path spelling. Missing target suffixes are allowed.
std::optional<fs::path> relativeToDirectory(const fs::path& target,const fs::path& root);
void noLinks(const fs::path&),atomicText(const fs::path&,const std::string&),copyVerified(const fs::path&,const fs::path&,int64_t expectedSize=-1,const std::string& expectedHash="",Tick={});
struct Digest{std::string hash;uint64_t size=0;uint32_t crc=0;};Digest digest(const fs::path&,Tick={});
bool canonicalHash(const std::string&);
std::string normalizeTime(const std::string&);void validateChat(const J&),validateItem(J&),validateAttachment(J&);
class Lock{void*handle=nullptr;int fd=-1;public:explicit Lock(const fs::path&);~Lock();Lock(const Lock&)=delete;};
// With Tick and no SQL parameters, pass J::array(); J{} / nullptr denotes one SQL NULL argument.
class Db{void*db=nullptr;bool trustedSchemaOff=false,cellSizeCheck=false;public:J protectionStatus()const;std::string protectionNotice()const;explicit Db(const fs::path&,bool create=true);~Db();Db(const Db&)=delete;J rows(const std::string&,const J&args=J::array(),Tick={});void exec(const std::string&);void execute(const std::string&,const J&args=J::array());void transaction(const std::function<void()>&);int64_t lastId();void backup(const fs::path&,Tick={});};
int archiveSchemaVersion(const fs::path&);
J validatedCaptureOptions(const J& raw);
class Store{Lock lock;public:fs::path root;Db db;fs::path upgradeSnapshot;explicit Store(const fs::path&,Tick={},bool allowUpgrade=false);J history(size_t limit=100,size_t offset=0);int64_t historyCount();void deleteHistory(int64_t);void recordCaptureState(int64_t);std::string putChat(const std::string&,const J&);void putItems(const std::string&,J,Tick={});std::string putAttachment(const std::string&,J);bool reusable(const std::string&,Tick={}),reuseKnown(const std::string&,Tick={});void saveBlob(const std::string&,const std::string&,const Digest&,bool preview=false,Tick={});J identityProblems(Tick={}),check(Tick={},bool reader=true),summary();};
struct Batch{J items=J::array(),attachments=J::array();std::string coverage="unknown";};
class Source{public:std::string account;virtual~Source()=default;virtual J chats()=0;virtual J chatsWithControl(Control&c){c.check();auto result=chats();c.check();return result;}virtual Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control&)=0;virtual fs::path download(const J&,const J&,const fs::path&,Control&)=0;};
const std::set<std::string>allKinds={"message","album","album_item","note","comment","thread"};
J collect(Store&,Source&,const J&,Control&,std::set<std::string> kinds=allKinds,const std::string&since="",const std::string&until="",std::function<void(std::string)>progress={},const std::string&messageMode="update");
fs::path exportHtml(Store&,Tick={},std::string* warning=nullptr);fs::path makeZip(Store&,const fs::path&,Tick={},std::string* warning=nullptr);J downloadParts(Store&,const J&,Tick={});bool validParts(Store&,const J&,Tick={});
class DemoSource:public Source{int revision;public:int downloads=0;explicit DemoSource(int v=1);J chats()override;Batch read(const J&,const std::set<std::string>&,const std::string&,const std::string&,Control&)override;fs::path download(const J&,const J&,const fs::path&,Control&)override;};
extern const char*web_template;extern const char*web_css;extern const char*web_script;extern const char*worker_script;
}
