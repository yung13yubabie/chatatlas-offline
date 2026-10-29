#include "line_batch.hpp"
#include "line_cipher.hpp"
#include "line_database.hpp"
#include <algorithm>

namespace atlas {
namespace {
constexpr size_t maxSources=64,maxChats=100000;
constexpr uint64_t maxCatalogBytes=64ULL*1024*1024;
std::string text(const J& value,const char* name){
 if(!value.is_object()||!value.contains(name)||!value[name].is_string())return {};
 return value[name].get<std::string>();
}
struct KeyCleanup {std::string& value;~KeyCleanup(){eraseLineKey(value);}};
}
LineBatchSource::LineBatchSource(std::vector<std::shared_ptr<Source>> selected,Control* requested):sources(std::move(selected)){
 Control fallback;auto& control=requested?*requested:fallback;control.check();
 if(sources.empty()||sources.size()>maxSources)throw CapabilityError("請選擇 1 至 64 份 LINE 來源資料庫。");
 std::set<std::string> accounts;std::vector<std::string> accountParts{"line-database-batch"};uint64_t bytes=0;
 for(size_t index=0;index<sources.size();++index){
  control.check();auto source=sources[index];
  if(!source||source->account.empty()||!accounts.insert(source->account).second)throw CapabilityError("批次資料庫來源重複或無法識別。");
  accountParts.push_back(source->account);
  auto original=source->chatsWithControl(control);if(!original.is_array())throw CapabilityError("批次來源聊天室清單格式不正確。");
  originals.push_back(original);std::string filename;
  if(auto database=std::dynamic_pointer_cast<LineDatabaseSource>(source))filename=utf8(database->databasePath().filename());
  const auto label="來源 "+std::to_string(index+1)+(filename.empty()?std::string():" · "+filename);
  for(const auto& chat:original){
   control.check();validateChat(chat);auto visible=chat;auto id=text(chat,"source_id");
   visible["source_id"]=key({"line-batch-chat",source->account,id});
   if(!text(chat,"parent_source_id").empty())visible["parent_source_id"]=key({"line-batch-chat",source->account,text(chat,"parent_source_id")});
   visible["source_account"]=source->account;visible["source_database"]=filename;
   visible["source_label"]=label;visible["original_source_id"]=id;
   bytes+=visible.dump().size();if(catalog.size()>=maxChats||bytes>maxCatalogBytes)throw CapabilityError("批次聊天室清單過多，請減少選取的資料庫。");
   if(!routes.emplace(text(visible,"source_id"),Route{source,chat,visible}).second)throw CapabilityError("同一資料庫內的聊天室識別重複。");
   catalog.push_back(std::move(visible));
  }
 }
 // The group identity is only for the live selection, never an archive account.
 std::sort(accountParts.begin()+1,accountParts.end());account="line-batch:"+key(accountParts);control.check();
}
const LineBatchSource::Route& LineBatchSource::route(const J& chat)const{
 auto found=routes.find(text(chat,"source_id"));
 if(found==routes.end()||found->second.visible!=chat||found->second.source->account!=text(chat,"source_account"))throw CapabilityError("批次聊天室與資料庫來源不符，請重新連接。");
 return found->second;
}
J LineBatchSource::chats(){Control control;return chatsWithControl(control);}
J LineBatchSource::chatsWithControl(Control& control){
 control.check();for(size_t i=0;i<sources.size();++i){if(sources[i]->chatsWithControl(control)!=originals[i])throw CapabilityError("批次來源聊天室已變更，請重新連接。");}
 for(const auto& chat:catalog){control.check();route(chat);}
 control.check();return catalog;
}
std::string LineBatchSource::accountFor(const J& chat)const{return route(chat).source->accountFor(route(chat).original);}
J LineBatchSource::storageChat(const J& chat)const{const auto& selected=route(chat);return selected.source->storageChat(selected.original);}
Batch LineBatchSource::read(const J& chat,const std::set<std::string>& kinds,const std::string& since,const std::string& until,Control& control){
 control.check();const auto& selected=route(chat);return selected.source->read(selected.original,kinds,since,until,control);
}
fs::path LineBatchSource::download(const J& chat,const J& attachment,const fs::path& stage,Control& control){
 control.check();const auto& selected=route(chat);return selected.source->download(selected.original,attachment,stage,control);
}
size_t LineBatchOpenResult::readyCount()const{return std::count_if(entries.begin(),entries.end(),[](const auto& entry){return entry.status=="ready";});}
std::string LineBatchOpenResult::summary()const{
 return "已核對 "+std::to_string(entries.size())+" 份資料庫：可讀取 "+std::to_string(readyCount())+" 份，略過 "+std::to_string(skippedCount())+" 份。"+(source?"各來源分開保存，不會合併為同一個帳號。":"未找到可讀取的 LINE 對話資料庫，請保持 LINE 登入，或重新選擇來源。");
}
LineBatchOpenResult openLineDatabases(const std::vector<fs::path>& paths,Control& control,std::function<void(std::string)> progress,LineBatchKeyScanner scanner){
 control.check();if(paths.empty()||paths.size()>maxSources)throw CapabilityError("一次請選擇 1 至 64 份 LINE 資料庫。");
 LineBatchOpenResult result;std::vector<size_t> pending;std::vector<fs::path> accepted;
 for(const auto& path:paths){
  control.check();LineBatchEntry entry{path,"invalid","無法讀取或不是支援的加密 LINE 資料庫。"};
  try{
   if(path.empty())throw CapabilityError("empty database path");
   entry.path=absoluteNormalizedPath(path);noLinks(entry.path);LineCipher::preflight(entry.path,&control);
   bool duplicate=false;for(const auto& previous:accepted){control.check();std::error_code ec;if(fs::equivalent(previous,entry.path,ec)&&!ec){duplicate=true;break;}}
   if(duplicate){entry.status="duplicate";entry.reason="同一份實體資料庫已列入本批次。";}
   else{entry.status="unavailable";entry.reason="未取得可開啟此資料庫的本機金鑰。";pending.push_back(result.entries.size());accepted.push_back(entry.path);}
  }catch(const Cancelled&){throw;}catch(const Error&){}catch(const fs::filesystem_error&){}
  result.entries.push_back(std::move(entry));
 }
 std::vector<std::shared_ptr<Source>> opened;
 if(!pending.empty()){
  if(!scanner)scanner=[](Control& scope,const LineBatchKeyVisitor& visitor,std::function<void(std::string)> report){visitLineDatabaseKeys(scope,visitor,std::move(report));};
  scanner(control,[&](LineKeyCandidates& keys,std::shared_ptr<LineSession> session,Tick budget){
   for(auto index:pending){
    control.check();if(budget)budget();auto& entry=result.entries[index];if(entry.status!="unavailable")continue;
    if(progress)progress("正在核對資料庫 "+std::to_string(index+1)+" / "+std::to_string(result.entries.size())+"…");
    std::shared_ptr<LineDatabaseSource> source;
    auto passphrase=keys.findMatch(control,[&](const std::string& candidate){
     // Once a key opens the cipher, an unsupported schema is a terminal result
     // for this file. Do not retry thousands of other keys against a feature DB.
     try{LineCipher check(entry.path,candidate,&control);}catch(const Cancelled&){throw;}catch(const Error&){return false;}catch(const fs::filesystem_error&){return false;}
     try{source=std::make_shared<LineDatabaseSource>(entry.path,candidate,fs::path{},&control);source->setSession(session);}
     catch(const Cancelled&){throw;}catch(const Error&){source.reset();entry.status="unsupported";entry.reason="已找到解密資訊，但不是支援的 LINE 對話資料庫或來源已變更。";}
     catch(const fs::filesystem_error&){source.reset();entry.status="unsupported";entry.reason="資料庫讀取期間無法繼續核對來源。";}
     return true;
    },budget);KeyCleanup cleanup{passphrase};
    if(source){entry.status="ready";entry.reason="已唯讀核對 LINE 對話格式。";opened.push_back(std::move(source));}
   }
   return std::all_of(pending.begin(),pending.end(),[&](size_t index){return result.entries[index].status!="unavailable";});
  },progress);
 }
 control.check();if(!opened.empty())result.source=std::make_shared<LineBatchSource>(std::move(opened),&control);
 return result;
}
}
