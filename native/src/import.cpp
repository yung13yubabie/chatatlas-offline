#include "import.hpp"
#include "file.hpp"
#include <algorithm>
#include <cctype>
#include <limits>
#ifndef _WIN32
#include <sys/syscall.h>
#include <linux/fs.h>
#endif
namespace atlas { namespace {
constexpr uint64_t maxDatabase=1024ULL*1024*1024;
const std::vector<std::string> tables={"settings","chats","items","blobs","attachments","jobs","issues","capture_state"};
void checkTick(const Tick& tick){if(tick)tick();}
std::string value(const J& row,const char* name){if(!row.contains(name)||!row[name].is_string())throw IntegrityError("備份欄位格式不正確。");return row[name].get<std::string>();}
void rejectSidecars(const fs::path& source){for(auto suffix:{"-wal","-shm","-journal"}){auto p=source/(std::string("archive.sqlite3")+suffix);noLinks(p);if(fs::exists(p))throw Error("請使用已解壓縮的完整備份，不要匯入正在使用或含交易暫存檔的收藏。");}}
void exclusiveDirectory(const fs::path& p){noLinks(p);
#ifdef _WIN32
 PrivateSecurity security;if(!CreateDirectoryW(p.c_str(),&security.attributes))throw Error("匯入目的地已存在或無法建立，請改用新的資料夾名稱。");requirePrivateDirectory(p);
#else
 if(::mkdir(p.c_str(),0700))throw Error("匯入目的地已存在或無法建立，請改用新的資料夾名稱。");
#endif
}
void publishDirectory(const fs::path& from,const fs::path& to){noLinks(from);noLinks(to);
#ifdef _WIN32
 if(!MoveFileExW(from.c_str(),to.c_str(),MOVEFILE_WRITE_THROUGH))throw Error("匯入目的地已出現同名資料夾，未覆寫既有內容。");
#elif defined(SYS_renameat2)
 if(::syscall(SYS_renameat2,AT_FDCWD,from.c_str(),AT_FDCWD,to.c_str(),RENAME_NOREPLACE))throw Error("匯入目的地已存在或無法完成保存，未覆寫既有內容。");
#else
 throw CapabilityError("這個系統不支援安全發布新收藏。");
#endif
}
std::string quote(const std::string& id){return "\""+id+"\"";}
std::set<std::string> validateSchema(Db& db){
 auto app=db.rows("PRAGMA application_id"),version=db.rows("PRAGMA user_version");
 if(app.size()!=1||app[0].begin().value()!=0x4c564c54||version.size()!=1||!version[0].begin().value().is_number_integer())throw IntegrityError("這份備份不是可辨識的 ChatAtlas 收藏。");
 auto number=version[0].begin().value().get<int>();if(number!=1&&number!=2)throw IntegrityError("備份版本不受支援，未修改原檔。");
 std::set<std::string> found;for(const auto& row:db.rows("SELECT type,name,sql FROM sqlite_master")){
  auto type=value(row,"type"),name=value(row,"name");
  if(type=="trigger"||type=="view")throw IntegrityError("備份含有不受支援的資料庫物件，未執行其中內容。");
  if(type=="table"){
   if(std::find(tables.begin(),tables.end(),name)==tables.end()||(number==1&&name=="capture_state"))throw IntegrityError("備份含有未知資料表，未匯入。");
   auto sql=value(row,"sql");std::transform(sql.begin(),sql.end(),sql.begin(),[](unsigned char c){return std::toupper(c);});
   if(sql.rfind("CREATE TABLE ",0)!=0&&sql.rfind("CREATE TABLE\n",0)!=0&&sql.rfind("CREATE TABLE\t",0)!=0)throw IntegrityError("備份含有不受支援的虛擬或特殊資料表。");found.insert(name);
  }else if(type=="index"){
   if(name.rfind("sqlite_autoindex_",0)!=0&&name!="items_chat_time"&&name!="attachments_item"&&name!="jobs_chat")throw IntegrityError("備份含有未知索引，未匯入。");
  }else throw IntegrityError("備份含有未知資料庫物件。");
 }
 for(const auto& name:tables)if(name!="capture_state"&&!found.count(name))throw IntegrityError("備份缺少必要資料表。");
 if(number==2&&!found.count("capture_state"))throw IntegrityError("備份缺少採集狀態資料表。");
 auto integrity=db.rows("PRAGMA integrity_check");if(integrity.size()!=1||integrity[0].begin().value()!="ok"||!db.rows("PRAGMA foreign_key_check").empty())throw IntegrityError("備份資料庫或關聯檢查未通過。");
 return found;
}
void transferRows(Db& from,Store& to,const std::set<std::string>& present,Tick tick){
 to.db.transaction([&]{to.db.execute("PRAGMA defer_foreign_keys=ON");
  for(const auto& table:tables){if(!present.count(table))continue;checkTick(tick);
   auto target=to.db.rows("PRAGMA table_info("+quote(table)+")"),source=from.rows("PRAGMA table_info("+quote(table)+")");
   std::set<std::string> sourceColumns;for(const auto& col:source)sourceColumns.insert(value(col,"name"));
   std::vector<std::string> columns;for(const auto& col:target){auto name=value(col,"name");if(sourceColumns.erase(name))columns.push_back(name);else if(!(table=="jobs"&&name=="options"))throw IntegrityError("備份資料欄位不完整。");}
   if(!sourceColumns.empty())throw IntegrityError("備份含有未知欄位，未匯入。");
   std::string names,marks;for(const auto& name:columns){if(!names.empty()){names+=",";marks+=",";}names+=quote(name);marks+="?";
    if(!from.rows("SELECT 1 FROM "+quote(table)+" WHERE length(CAST("+quote(name)+" AS BLOB)) > 4194304 LIMIT 1").empty())throw IntegrityError("備份包含過大的單一欄位。");
   }
   for(uint64_t offset=0;;offset+=32){checkTick(tick);auto rows=from.rows("SELECT "+names+" FROM "+quote(table)+" NOT INDEXED LIMIT 32 OFFSET "+std::to_string(offset));if(rows.empty())break;
    for(const auto& row:rows){checkTick(tick);J args=J::array();for(const auto& name:columns)args.push_back(row[name]);to.db.execute("INSERT INTO "+quote(table)+"("+names+") VALUES("+marks+")",args);}
   }
  }
  if(!present.count("capture_state"))to.db.execute("INSERT INTO capture_state(chat_id,job_id,status,started_at,finished_at,coverage,counts,options) SELECT chat_id,id,status,started_at,finished_at,coverage,counts,options FROM jobs WHERE id IN (SELECT max(id) FROM jobs GROUP BY chat_id)");
  to.db.execute("UPDATE jobs SET status='interrupted',finished_at=? WHERE status='running'",{now()});
  to.db.execute("UPDATE capture_state SET status='interrupted',finished_at=? WHERE status='running'",{now()});
 });
}
void validateBlobPath(const J& blob){
 auto hash=value(blob,"sha256"),name=value(blob,"path");if(!canonicalHash(hash)||!blob["size"].is_number_integer()||blob["size"].get<int64_t>()<0)throw IntegrityError("備份附件校驗資料不正確。");
 const auto prefix="assets/"+hash.substr(0,2)+"/"+hash;bool good=false;for(auto ext:{".png",".jpg",".gif",".webp",".wav",".ogg",".mp3",".mp4",".bin"})if(name==prefix+ext)good=true;
 if(!good)throw IntegrityError("備份附件位置不正確，未讀取該位置。");
}
} // namespace
ImportResult importBackup(const fs::path& givenSource,const fs::path& givenDestination,Tick tick){
 checkTick(tick);auto source=fs::absolute(givenSource).lexically_normal(),dest=fs::absolute(givenDestination).lexically_normal();noLinks(source);noLinks(dest);
 if(!fs::is_directory(source)||!fs::is_regular_file(source/"archive.sqlite3"))throw Error("請選擇含 archive.sqlite3 的完整備份資料夾；單獨的離線網頁範例不是可匯入的收藏。");
 if(!fs::is_directory(dest.parent_path())||fs::exists(dest))throw Error("請在可寫入的位置使用尚未存在的新收藏資料夾。");
 // Store rejects repository ancestors, but the private input snapshot is made
 // before Store exists. Apply the same no-source-tree rule before any writes.
 for(auto node=dest.parent_path();!node.empty();){checkTick(tick);if(fs::exists(node/".git"))throw Error("匯入請放在原始碼以外的私人資料夾，避免不小心公開。");auto parent=node.parent_path();if(parent==node)break;node=parent;}
 if(relativeToDirectory(dest,source))throw Error("新的收藏不可放在原備份裡面。");
 rejectSidecars(source);auto sourceDb=source/"archive.sqlite3";{VerifiedInput in(sourceDb);if(in.size()>maxDatabase)throw Error("這份備份資料庫超過目前匯入的 1 GiB 上限。");}
 auto initial=digest(sourceDb,tick);if(initial.size>maxDatabase)throw Error("這份備份資料庫超過目前匯入的 1 GiB 上限。");auto stage=dest.parent_path()/path8(".ChatAtlas-Import-"+sha(now()+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())).substr(0,24));
 exclusiveDirectory(stage);auto input=stage/"source.sqlite3",payload=stage/"collection";
 try{
  checkTick(tick);if(relativeToDirectory(stage,source))throw Error("新的收藏不可放在原備份裡面。");copyVerified(sourceDb,input,static_cast<int64_t>(initial.size),initial.hash,tick);
  {
   Db snapshot(input,false);auto present=validateSchema(snapshot);Store imported(payload,tick);transferRows(snapshot,imported,present,tick);
   auto identities=imported.identityProblems(tick);if(!identities.empty())throw IntegrityError("備份識別或採集狀態不正確，未完成匯入。");
   for(const auto& blob:imported.db.rows("SELECT * FROM blobs")){checkTick(tick);validateBlobPath(blob);auto rel=value(blob,"path");copyVerified(within(source,rel),within(payload,rel),blob["size"].get<int64_t>(),value(blob,"sha256"),tick);}
   auto problems=imported.check(tick,false);for(const auto& problem:problems)if(value(problem,"code")!="unreferenced")throw IntegrityError("備份的附件、關聯或內容檢查未通過，未完成匯入。");
   exportHtml(imported,tick);
  }
  checkTick(tick);rejectSidecars(source);auto after=digest(sourceDb,tick);if(after.hash!=initial.hash||after.size!=initial.size)throw IntegrityError("匯入途中原備份已改變，未發布新收藏。");
  checkTick(tick);noLinks(stage);if(relativeToDirectory(dest,source))throw Error("新的收藏不可放在原備份裡面。");publishDirectory(payload,dest);
  // Never recursively delete a staging tree by pathname. Only our checked
  // input copy and now-empty staging directory are candidates for cleanup.
  fs::path retained=stage;
  try{noLinks(stage);noLinks(input);auto copy=digest(input);if(copy.hash==initial.hash&&copy.size==initial.size&&fs::remove(input)&&fs::remove(stage))retained.clear();}catch(...){}
  return {dest,retained};
 }catch(const Cancelled&){throw ImportFailure("匯入已停止，原備份未改動；未完成的私人副本已保留。",stage,true);}
 catch(const Error& e){throw ImportFailure(std::string(e.what())+" 原備份未改動；未完成的私人副本已保留。",stage);}
 catch(...){throw ImportFailure("匯入尚未完成，原備份未改動；未完成的私人副本已保留。",stage);}
}
}
