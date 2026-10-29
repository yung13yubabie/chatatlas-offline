#include "text_export.hpp"
#include "attachment_scope.hpp"
#include "file.hpp"
#include <atomic>
#include <limits>
#ifdef __linux__
#include <sys/syscall.h>
#endif

namespace atlas {
namespace {
constexpr size_t pageSize=500,maxSelectedChats=100000;
std::atomic<uint64_t> generations{0};
std::string text(const J& row,const char* field){
 return row.contains(field)&&row[field].is_string()?row[field].get<std::string>():std::string{};
}
std::string oneLine(std::string_view value){
 std::string result;
 for(unsigned char c:value){
  if(c=='\r'||c=='\n'||c=='\t')result+=' ';
  else if(c<32||c==127){const char* hex="0123456789abcdef";result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
  else result+=char(c);
 }
 return result;
}
std::string indented(std::string_view value){
 std::string result="  ";
 for(size_t at=0;at<value.size();++at){
  const auto c=static_cast<unsigned char>(value[at]);
  if(c=='\r'||c=='\n'){
   if(c=='\r'&&at+1<value.size()&&value[at+1]=='\n')++at;
   result+="\r\n  ";
  }else if((c<32&&c!='\t')||c==127){const char* hex="0123456789abcdef";result+="\\u00";result+=hex[c>>4];result+=hex[c&15];}
  else result+=char(c);
 }
 result+="\r\n";return result;
}
struct Snapshot {
 Db& database;bool active=true;
 explicit Snapshot(Db& database):database(database){database.rows("BEGIN");}
 ~Snapshot(){if(active)try{database.rows("ROLLBACK");}catch(...) {}}
 void commit(){database.rows("COMMIT");active=false;}
};
void createNewPrivateDirectory(const fs::path& directory){
 noLinks(directory);
#ifdef _WIN32
 requireLocalPrivatePath(directory);PrivateSecurity security;
 if(!CreateDirectoryW(directory.c_str(),&security.attributes))throw Error("文字匯出暫存位置已存在或無法建立。");
#else
 if(!fs::create_directory(directory))throw Error("文字匯出暫存位置已存在或無法建立。");
 fs::permissions(directory,fs::perms::owner_all,fs::perm_options::replace);
#endif
}
void publishDirectory(const fs::path& from,const fs::path& to){
 noLinks(from);noLinks(to);
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH))throw Error("文字匯出目的地已存在或無法完成保存。");
#elif defined(__linux__) && defined(SYS_renameat2)
 // Unlike rename(), RENAME_NOREPLACE also refuses an existing empty directory.
 if(syscall(SYS_renameat2,AT_FDCWD,from.c_str(),AT_FDCWD,to.c_str(),1)!=0)throw Error("文字匯出目的地已存在或無法完成保存。");
#else
 // The Windows application and Linux validation use the no-replace paths above.
 // Refuse publication on an unsupported platform rather than weaken that rule.
 (void)from;(void)to;throw CapabilityError("這個平台尚未提供不覆寫的文字匯出發布方式。");
#endif
}
struct Staging {
 fs::path path;bool owned=false;
 ~Staging(){if(owned)try{noLinks(path);fs::remove_all(path);}catch(...) {}}
};
std::string attachmentState(Store& store,const J& attachment,Tick tick){
 const auto state=text(attachment,"status"),variant=text(attachment,"variant");
 if(state=="saved"||state=="preview"){
  if(!store.reusable(text(attachment,"id"),tick))return "檔案遺失或校驗失敗";
  if(state=="preview"||variant=="thumbnail")return "僅有預覽圖";
  return "已保存原檔";
 }
 if(state=="pending")return "待下載";
 if(state=="unavailable")return "未取得";
 if(state=="failed")return "下載失敗";
 throw IntegrityError("附件狀態不正確，未發布文字匯出。");
}
}

fs::path exportChatText(Store& store,const std::vector<std::string>& chatIds,Tick tick){
 if(tick)tick();
 if(chatIds.empty()||chatIds.size()>maxSelectedChats)throw Error("請選擇要匯出的聊天室，過多時請分批處理。");
 std::set<std::string> seen;J chats=J::array();
 Snapshot snapshot(store.db);
 for(const auto& id:chatIds){
  if(tick)tick();
  if(!canonicalHash(id))throw IntegrityError("聊天室識別不正確，未匯出其他來源。");
  if(!seen.insert(id).second)continue;
  auto rows=store.db.rows("SELECT id,account,source_id,title FROM chats WHERE id=?",{id},tick);
  if(rows.size()!=1||text(rows[0],"id")!=key({"chat",text(rows[0],"account"),text(rows[0],"source_id")}))throw IntegrityError("選取的聊天室與收藏不符，未匯出其他來源。");
  chats.push_back(std::move(rows[0]));
 }
 const auto generatedAt=now();
 auto generation=sha(utf8(store.root)+generatedAt+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+std::to_string(generations.fetch_add(1))).substr(0,32);
 auto parent=privateDir(within(store.root,"text-exports"));
#ifdef _WIN32
 requirePrivateDirectory(parent);
#endif
 Staging staging{within(parent,".partial-"+generation)};
 createNewPrivateDirectory(staging.path);staging.owned=true;
 J exported=J::array();
 for(const auto& chat:chats){
  if(tick)tick();const auto cid=text(chat,"id");
  const auto filename=safeName(text(chat,"title"))+"-"+cid+".txt";
  auto destination=within(staging.path,filename);
  uint64_t bytes=0,items=0,attachments=0;Sha256 checksum;
  {
   ExclusiveFile file(destination);
   auto write=[&](const std::string& value){
    if(value.size()>std::numeric_limits<uint64_t>::max()-bytes)throw Error("文字匯出內容過大。");
    file.write(value);checksum.update(value.data(),value.size());bytes+=value.size();
   };
   write(std::string("\xef\xbb\xbf",3));
   write("聊藏對話紀錄\r\n聊天室："+oneLine(text(chat,"title"))+"\r\n收藏識別："+cid+"\r\n匯出時間："+generatedAt+"\r\n範圍：這個聊天室在收藏中已保存的對話，可能包含先前整理內容。\r\n附件原檔另存於收藏資料夾；未取得與僅預覽的狀態會另行標示。\r\n\r\n");
   bool first=true;std::string previousTime,previousId;int64_t previousOrdinal=0;
   while(true){
    if(tick)tick();
    const auto columns="SELECT id,source_id,sent_at,ordinal,sender_name,text,title,state,kind FROM items WHERE chat_id=? AND kind IN ('message','thread') ";
    J rows;
    if(first)rows=store.db.rows(std::string(columns)+"ORDER BY coalesce(sent_at,''),ordinal,id LIMIT 500",{cid},tick);
    else rows=store.db.rows(std::string(columns)+"AND (coalesce(sent_at,'')>? OR (coalesce(sent_at,'')=? AND (ordinal>? OR (ordinal=? AND id>?)))) ORDER BY coalesce(sent_at,''),ordinal,id LIMIT 500",{cid,previousTime,previousTime,previousOrdinal,previousOrdinal,previousId},tick);
    if(rows.empty())break;
    for(const auto& item:rows){
     if(tick)tick();const auto iid=text(item,"id");
     if(iid!=key({"item",cid,text(item,"source_id")}))throw IntegrityError("對話識別與聊天室不符，未發布文字匯出。");
     const auto sent=text(item,"sent_at"),sender=text(item,"sender_name"),state=text(item,"state");
     write("["+(sent.empty()?std::string("時間未記錄"):oneLine(sent))+"] "+(sender.empty()?std::string("未記錄發話者"):oneLine(sender))+"\r\n");
     if(state=="recalled")write("  [訊息已收回]\r\n");
     else if(state=="deleted")write("  [訊息已刪除]\r\n");
     else {
      const auto body=text(item,"text");
      write(body.empty()?std::string("  [未保存文字]\r\n"):indented(body));
     }
     auto media=store.db.rows("SELECT a.id,a.chat_id,a.item_id,a.source_id,a.variant,a.version,a.name,a.status FROM attachments a WHERE a.chat_id=? AND a.item_id=? AND "+std::string(requiredAttachmentSql)+" ORDER BY a.id",{cid,iid},tick);
     for(const auto& attachment:media){
      if(tick)tick();
      if(text(attachment,"id")!=key({"attachment",cid,iid,text(attachment,"source_id"),text(attachment,"variant"),text(attachment,"version")}))throw IntegrityError("附件識別與對話不符，未發布文字匯出。");
      write("  附件："+oneLine(text(attachment,"name"))+"（"+attachmentState(store,attachment,tick)+"）\r\n");++attachments;
     }
     write("\r\n");++items;
     previousTime=sent;previousId=iid;previousOrdinal=item["ordinal"].get<int64_t>();
    }
    first=false;if(rows.size()<pageSize)break;
   }
   if(tick)tick();file.sync();
  }
  exported.push_back({{"chat_id",cid},{"title",chat["title"]},{"file",filename},{"items",items},{"attachments",attachments},{"bytes",bytes},{"sha256",checksum.finish()}});
 }
 snapshot.commit();
 if(tick)tick();
 {
  ExclusiveFile manifest(within(staging.path,"export-complete.json"));
  manifest.write(safeJson({{"format","ChatAtlas-text-export"},{"version",1},{"complete",true},{"created_at",generatedAt},{"scope","stored_messages_in_selected_chats"},{"chats",exported}}));manifest.sync();
 }
 if(tick)tick();
 auto published=within(parent,generation);publishDirectory(staging.path,published);staging.owned=false;
 return published;
}
}
