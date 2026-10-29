#include "line_summary.hpp"
#include "file.hpp"
#include <algorithm>
#include <array>

namespace atlas {
namespace {
constexpr uint64_t snapshotLimit=64ULL*1024*1024;
void fields(const J& value,const std::set<std::string>& allowed){
 if(!value.is_object())throw CapabilityError("LINE 快照欄位必須是物件。");
 for(auto it=value.begin();it!=value.end();++it)if(!allowed.count(it.key()))throw CapabilityError("LINE 快照含不支援的欄位，請重新產生。");
}
std::string stringField(const J& value,const char* name,size_t maximum=4*1024*1024,bool nullable=false){
 if(!value.contains(name)||(!value[name].is_string()&&!(nullable&&value[name].is_null())))throw CapabilityError("LINE 快照缺少必要欄位或欄位型別不正確。");
 if(value[name].is_null())return {};
 auto text=value[name].get<std::string>();
 if(text.size()>maximum||text.find('\0')!=std::string::npos)throw CapabilityError("LINE 快照的單一欄位過大或含不支援的字元。");
 return text;
}
std::string identifier(const J& value,const char* name){
 auto text=stringField(value,name,1024);
 if(text.empty()||std::any_of(text.begin(),text.end(),[](unsigned char c){return c<32||c==127;}))throw CapabilityError("LINE 快照缺少穩定的來源識別。");
 return text;
}
// normalizeTime validates the complete timezone-aware date and normalizes UTC.
// Compare a fixed-width nine-digit fraction before the store's millisecond conversion.
std::string instant(const std::string& input){
 auto normalized=normalizeTime(input);
 if(normalized.empty())throw CapabilityError("LINE 快照必須有完整時間範圍。");
 auto at=input.find('.',19);std::string fraction;
 if(at!=std::string::npos){++at;while(at<input.size()&&input[at]>='0'&&input[at]<='9')fraction+=input[at++];}
 fraction.append(9-fraction.size(),'0');
 return normalized.substr(0,19)+"."+fraction+"+00:00";
}
std::string readSnapshot(const fs::path& path,Control& control){
 control.check();noLinks(path);VerifiedInput input(path);
 if(input.size()>snapshotLimit)throw CapabilityError("LINE 快照超過 64 MiB，請縮短期間後分批匯入。");
 std::string bytes;bytes.reserve(static_cast<size_t>(input.size()));std::array<char,16384> buffer{};
 while(input){control.check();input.read(buffer.data(),buffer.size());auto size=static_cast<size_t>(input.gcount());
  if(size>snapshotLimit-bytes.size())throw CapabilityError("LINE 快照超過 64 MiB，請縮短期間後分批匯入。");
  bytes.append(buffer.data(),size);
 }
 if(input.bad()||bytes.size()!=input.size())throw CapabilityError("LINE 快照在讀取期間改變，請重新產生。");
 control.check();return bytes;
}
J parseSnapshot(const std::string& bytes,Control& control){
 std::vector<std::set<std::string>> keys;
 try{return J::parse(bytes,[&](int depth,J::parse_event_t event,J& value){
  control.check();if(depth>64)throw CapabilityError("LINE 快照巢狀層數過多。");
  if(event==J::parse_event_t::object_start)keys.emplace_back();
  else if(event==J::parse_event_t::key){if(keys.empty()||!keys.back().insert(value.get<std::string>()).second)throw CapabilityError("LINE 快照有重複欄位，未匯入。");}
  else if(event==J::parse_event_t::object_end)keys.pop_back();
  return true;
 });}catch(const J::exception&){throw CapabilityError("LINE 快照不是有效的 UTF-8 JSON，請重新產生。");}
}
}

LineSummarySource::LineSummarySource(const fs::path& path,Control* requested){
 Control fallback;auto& control=requested?*requested:fallback;
 const auto bytes=readSnapshot(path,control);auto snapshot=parseSnapshot(bytes,control);
 fields(snapshot,{"format","schema","account_id","chat","since","until","local_range_complete","messages"});
 if(stringField(snapshot,"format",128)!="chatatlas.line-summary"||!snapshot.contains("schema")||!snapshot["schema"].is_number_integer()||snapshot["schema"]!=1)throw CapabilityError("LINE 快照格式或版本不相容。");
 if(!snapshot.contains("local_range_complete")||!snapshot["local_range_complete"].is_boolean()||!snapshot["local_range_complete"].get<bool>())throw CapabilityError("LINE 快照的本機指定期間尚未完整讀取，請完成分頁後重新產生。");
 account="line-summary:"+identifier(snapshot,"account_id");
 if(!snapshot.contains("chat"))throw CapabilityError("LINE 快照缺少聊天室。");
 chat=snapshot["chat"];fields(chat,{"source_id","title","kind"});identifier(chat,"source_id");
 if(stringField(chat,"title",4096).empty()||!std::set<std::string>{"group","direct","room","community"}.count(stringField(chat,"kind",32)))throw CapabilityError("LINE 快照聊天室名稱或類型不正確。");
 validateChat(chat);
 auto originalFrom=stringField(snapshot,"since",48),originalTo=stringField(snapshot,"until",48);
 preciseFrom=instant(originalFrom);preciseTo=instant(originalTo);from=normalizeTime(originalFrom);to=normalizeTime(originalTo);
 if(preciseFrom>=preciseTo)throw CapabilityError("LINE 快照的起訖期間不正確。");
 // Capture history and archive timestamps use milliseconds. Reject unrepresentable
 // bounds instead of silently widening or shrinking the imported coverage.
 if(instant(from)!=preciseFrom||instant(to)!=preciseTo)throw CapabilityError("LINE 快照的起訖期間需使用毫秒或更低精度，請重新選擇期間。");
 if(!snapshot.contains("messages")||!snapshot["messages"].is_array())throw CapabilityError("LINE 快照缺少訊息陣列。");
 struct Row{J item;std::string time;};std::vector<Row> rows;std::set<std::string> identifiers;
 for(auto& raw:snapshot["messages"]){
  control.check();fields(raw,{"message_id","type","sender","content","sent_at","urls","filename","url","title","description","thread_id","thread_root_message_id"});
  auto id=identifier(raw,"message_id"),type=identifier(raw,"type"),sender=stringField(raw,"sender",4*1024*1024,true),sent=stringField(raw,"sent_at",48);
  if(!identifiers.insert(id).second)throw CapabilityError("LINE 快照有重複訊息識別，未匯入。");
  auto time=instant(sent);if(time<preciseFrom||time>=preciseTo)throw CapabilityError("LINE 快照含指定期間以外的訊息，未匯入。");
  std::string content;
  if(type!="link"||raw.contains("content"))content=stringField(raw,"content",4*1024*1024,type!="text");
  for(auto name:{"filename","url","title","description"})if(raw.contains(name))stringField(raw,name,4*1024*1024,true);
  if(raw.contains("urls")){
   if(!raw["urls"].is_array())throw CapabilityError("LINE 快照連結列表格式不正確。");
   for(auto& url:raw["urls"])if(!url.is_string()||url.get_ref<const std::string&>().size()>4*1024*1024)throw CapabilityError("LINE 快照連結列表格式不正確。");
  }
  const bool thread=raw.contains("thread_id");
  if(thread!=raw.contains("thread_root_message_id"))throw CapabilityError("LINE 快照討論串識別不完整。");
  if(thread){identifier(raw,"thread_id");identifier(raw,"thread_root_message_id");}
  J metadata={{"source","line-summary"},{"original_type",type},{"original_sent_at",sent},{"snapshot_since",originalFrom},{"snapshot_until",originalTo},{"attachment_files_imported",false}};
  for(auto name:{"urls","filename","url","title","description","thread_id","thread_root_message_id"})if(raw.contains(name))metadata[name]=raw[name];
  std::string text=content;
  if(type!="text"){
   text="[LINE 類型："+type+"；此來源未提供附件原檔]";
   if(!content.empty())text+="\n"+content;
   if(type=="link")for(auto name:{"title","url","description"})if(raw.contains(name)&&raw[name].is_string()&&!raw[name].get_ref<const std::string&>().empty())text+="\n"+raw[name].get<std::string>();
   if(raw.contains("filename")&&raw["filename"].is_string())text+="\n檔名："+raw["filename"].get<std::string>();
  }
  J item={{"source_id",id},{"kind","message"},{"text",text},{"sender_name",sender},{"sender_id",nullptr},{"sent_at",sent},{"metadata",metadata},{"ordinal",0}};
  validateItem(item);rows.push_back({std::move(item),std::move(time)});
 }
 std::sort(rows.begin(),rows.end(),[](const Row& left,const Row& right){return left.time!=right.time?left.time<right.time:left.item["source_id"].get_ref<const std::string&>()<right.item["source_id"].get_ref<const std::string&>();});
 for(size_t index=0;index<rows.size();++index){control.check();rows[index].item["ordinal"]=index;items.push_back(std::move(rows[index].item));instants.push_back(std::move(rows[index].time));}
 control.check();
}
J LineSummarySource::chats(){return J::array({chat});}
Batch LineSummarySource::read(const J& selected,const std::set<std::string>& kinds,const std::string& since,const std::string& until,Control& control){
 control.check();if(selected!=chat)throw CapabilityError("選取的聊天室與 LINE 快照不符，請重新載入。");
 if(kinds.empty()||!kinds.count("message"))throw CapabilityError("LINE 快照只支援對話訊息，請選擇對話文字。");
 for(auto& kind:kinds)if(kind!="message"&&kind!="thread")throw CapabilityError("LINE 快照不提供相簿或記事本，請只選擇對話訊息。");
 auto start=since.empty()?preciseFrom:instant(since),end=until.empty()?preciseTo:instant(until);
 if(start<preciseFrom||end>preciseTo||start>=end)throw CapabilityError("整理期間超出 LINE 快照已讀取的範圍，請縮小期間或重新產生快照。");
 Batch result;result.coverage="local_only";
 for(size_t index=0;index<items.size();++index){control.check();if(instants[index]>=start&&instants[index]<end)result.items.push_back(items[index]);}
 control.check();return result;
}
fs::path LineSummarySource::download(const J&,const J&,const fs::path&,Control& control){control.check();throw DownloadUnavailable("LINE 快照不提供附件原檔。");}
}
