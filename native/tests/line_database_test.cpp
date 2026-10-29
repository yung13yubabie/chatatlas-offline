#include "line_database.hpp"
#include "line_cipher.hpp"
#include "demo_image.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace atlas;
namespace {
int checks=0;
const std::string secret="A1b2C3d4E5f60718293a4B5c6D7e8F90";
constexpr int64_t start=1790784000000LL; // 2026-10-01 00:00:00 +08:00
void require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F action,const char* reason){bool rejected=false;try{action();}catch(const Error&){rejected=true;}require(rejected,reason);}
template<class F>void cancels(F action,const char* reason){bool cancelled=false;try{action();}catch(const Cancelled&){cancelled=true;}require(cancelled,reason);}
J chat(LineDatabaseSource& source,const std::string& id){for(auto& value:source.chats())if(value["source_id"]==id)return value;throw std::runtime_error("missing synthetic chat");}
std::string number(int value){auto result=std::to_string(value);return std::string(5-result.size(),'0')+result;}
void message(LineCipher& writer,const std::string& id,const std::string& group,int64_t time,int type,const std::string& text,const J& metadata=J::object(),const J& sender="member"){
 writer.rows("INSERT INTO _message VALUES(?,?,?,?,?,?,?)",{id,group,sender,time,text,type,metadata.dump()});
}
void fixture(const fs::path& path){
 LineCipher writer(path,secret,nullptr,true);
 writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");
 writer.rows("CREATE INDEX message_chat_time ON _message(_chatId,_createdTime,_id)");
 writer.rows("CREATE TABLE _chat(_id TEXT PRIMARY KEY,_lastUpdatedTime INTEGER)");
 writer.rows("CREATE TABLE _groupChat(_chatMid TEXT PRIMARY KEY,_chatName TEXT)");
 writer.rows("CREATE TABLE _contact(_mid TEXT PRIMARY KEY,_displayName TEXT,_displayNameOverridden TEXT)");
 writer.rows("CREATE TABLE _room(_mid TEXT PRIMARY KEY)");
 writer.rows("CREATE TABLE _squareChat(_squareChatMid TEXT PRIMARY KEY,_name TEXT)");
 writer.rows("CREATE TABLE _squareMember(_squareMemberMid TEXT PRIMARY KEY,_displayName TEXT)");
 writer.rows("CREATE TABLE _squareThread(_threadMid TEXT PRIMARY KEY,_chatMid TEXT,_messageId TEXT)");
 writer.rows("BEGIN");
 for(const auto& id:{"group","member","room","square","unknown","empty"})writer.rows("INSERT INTO _chat VALUES(?,?)",{id,start});
 writer.rows("INSERT INTO _groupChat VALUES('group','人工群組')");writer.rows("INSERT INTO _groupChat VALUES('empty','空的人工群組')");
 writer.rows("INSERT INTO _contact VALUES('member','原始顯示名稱','自訂人工名稱')");
 writer.rows("INSERT INTO _room VALUES('room')");writer.rows("INSERT INTO _squareChat VALUES('square','人工社群')");
 writer.rows("INSERT INTO _squareMember VALUES('square-person','人工社群成員')");
 writer.rows("INSERT INTO _squareThread VALUES('thread','group','root-outside-range')");
 for(int index=0;index<503;++index)message(writer,"text-"+number(index),"group",start+index/3,0,"人工原文 #原生 "+std::to_string(index));
 message(writer,"image","group",start+1000,1,"",{{"FILE_NAME","人工圖片.png"}});
 message(writer,"file","group",start+1001,14,"",{{"FILE_NAME","人工附件.txt"}});
 message(writer,"missing","group",start+1002,1,"");
 message(writer,"thread-image","thread",start+1003,1,"討論串圖片",{{"FILE_NAME","討論串圖片.png"}},"square-person");
 message(writer,"next-day","group",start+86400000,0,"不應進入前一天的半開範圍");
 message(writer,"other","member",start,0,"其他聊天室不能串入");
 message(writer,"square-message","square",start,0,"社群成員名稱",J::object(),"square-person");
 message(writer,"unknown-message","unknown",start,16,R"({"keyMaterial":"SYNTHETIC_DO_NOT_PUBLISH"})",{{"url","https://example.invalid/synthetic"},{"title","人工連結"}},nullptr);
 writer.rows("COMMIT");
}
}

int main(int argc,char** argv){try{
 require(argc==2,"owned fixture directory required");auto root=path8(argv[1]);require(fs::is_directory(root),"fixture directory exists");
 auto path=root/"synthetic-line.edb";fixture(path);const auto original=digest(path);const auto permissions=fs::status(path).permissions();
 const auto png=std::string(reinterpret_cast<const char*>(demo_png),demo_png_size),file=std::string("人工附件原文\n");
 auto imagePath=root/"chosen-image.png",filePath=root/"chosen-attachment.txt";atomicText(imagePath,png);atomicText(filePath,file);
 Control control;LineDatabaseSource source(path,secret,root,&control);
 require(source.account.starts_with("line-db:")&&source.account.size()==72,"source identity hashes selected database path and filesystem object identity, never the passphrase");
 require(source.databasePath()==absoluteNormalizedPath(path),"selected database retained");
 require(source.chats().size()==6,"chat metadata, empty chats and message-derived chat IDs exposed without thread duplicate");
 auto group=chat(source,"group");require(group["title"]=="人工群組"&&group["kind"]=="group","group title comes from real LINE metadata columns");
 require(chat(source,"member")["title"]=="自訂人工名稱"&&chat(source,"member")["kind"]=="direct","direct chat uses local override display name");
 require(chat(source,"room")["kind"]=="room"&&chat(source,"square")["kind"]=="community","room and community classifications preserved");
 source.bindMediaFile(group,"image",imagePath,false,&control);source.bindMediaFile(group,"file",filePath,false,&control);source.bindMediaFile(group,"thread-image",imagePath,false,&control);
 auto batch=source.read(group,{"message","thread"},"2026-10-01T00:00:00+08:00","2026-10-02T00:00:00+08:00",control);
 require(batch.coverage=="local_only"&&batch.items.size()==507,"more than one 500-row page returned fully and half-open day excludes next day");
 require(batch.attachments.size()>=4,"image, file, missing cache and thread-image attachments remain explicit");
 require(batch.items[0]["source_id"]=="text-00000"&&batch.items[1]["source_id"]=="text-00001"&&batch.items[502]["source_id"]=="text-00502","timestamp ties preserve binary message ID ordering across pages");
 require(batch.items[0]["sender_id"]=="member"&&batch.items[0]["sender_name"]=="自訂人工名稱","real sender MID preserved separately from display name");
 auto thread=std::find_if(batch.items.begin(),batch.items.end(),[](const J& row){return row["source_id"]=="thread-image";});
 require(thread!=batch.items.end()&&(*thread)["metadata"]["thread_id"]=="thread"&&(*thread)["metadata"]["thread_root_message_id"]=="root-outside-range","thread rows use mapped parent scope and retain actual thread IDs");
 require((*thread)["parent_source_id"].is_null()&&(*thread)["sender_name"]=="人工社群成員","flat imported thread messages do not create unavailable parent foreign keys");
 require(source.read(group,{"message"},"2026-10-01T00:00:00.001+08:00","2026-10-01T00:00:00.002+08:00",control).items.size()==3,"millisecond range is half-open");
 require(source.read(chat(source,"empty"),{"message"},"","",control).items.empty(),"empty local chat is valid");
 auto link=source.read(chat(source,"unknown"),{"message"},"","",control);require(link.items.size()==1&&link.items[0]["sender_id"].is_null()&&link.items[0]["text"].get<std::string>().find("https://example.invalid/synthetic")!=std::string::npos,"link metadata and absent sender remain useful");
 require(link.items[0].dump().find("SYNTHETIC_DO_NOT_PUBLISH")==std::string::npos,"non-text transport keyMaterial is never rendered or archived as message text");
 rejects([&]{auto altered=group;altered["title"]="另一個群組";source.read(altered,{"message"},"","",control);},"modified selected chat rejected");
 rejects([&]{source.read(group,{"message","album"},"","",control);},"unsupported albums never silently complete");
 rejects([&]{source.read(group,{"thread"},"","",control);},"flat messages cannot pretend to be thread-only scope");
 rejects([&]{source.read(group,{"message"},"2026-10-01T00:00:01Z","2026-10-01T00:00:00Z",control);},"reversed scope rejected");
 rejects([&]{source.read(group,{"message"},"2026-10-01T00:00:00.000001Z","",control);},"submillisecond scope does not get silently widened");
 rejects([&]{source.bindMediaFile(group,"other",filePath,false,&control);},"a file cannot be bound to another chat's message");
 {
  Store store(root/"collection");auto count=collect(store,source,J::array({group}),control,{"message","thread"},"2026-09-30T16:00:00.000+00:00","2026-10-01T16:00:00.000+00:00");
  require(count["items"]==507&&count["downloaded"].get<int>()+count["reused"].get<int>()>=3&&count["unavailable"]>=1,"collector saves real native originals, reuses identical image bytes and reports missing local media");
  auto saved=store.db.rows("SELECT a.kind,a.status,b.sha256,b.size FROM attachments a LEFT JOIN blobs b ON b.id=a.blob_id ORDER BY a.id");
  require(std::count_if(saved.begin(),saved.end(),[](const J& row){return row["status"]=="saved";})>=3,"saved attachment records have actual blob copies");
  require(std::any_of(saved.begin(),saved.end(),[&](const J& row){return row["kind"]=="image"&&row["sha256"]==sha(png);}),"original image bytes exported without text placeholder substitution");
  require(std::any_of(saved.begin(),saved.end(),[&](const J& row){return row["kind"]=="file"&&row["sha256"]==sha(file);}),"ordinary file bytes copied natively");
  exportHtml(store);require(fs::is_regular_file(store.root/"index.html"),"native LINE content and media have an offline reader");
  auto problems=store.check();require(!problems.empty()&&std::all_of(problems.begin(),problems.end(),[](const J& row){return row["code"]=="pending_attachment";}),"only unavailable source media remains an explicit integrity issue");
  auto before=store.db.rows("SELECT id FROM items ORDER BY id");auto second=collect(store,source,J::array({group}),control,{"message","thread"},"2026-09-30T16:00:00.000+00:00","2026-10-01T16:00:00.000+00:00");
  require(store.db.rows("SELECT id FROM items ORDER BY id")==before&&second["reused"]>=3,"repeated native reads deduplicate messages and verify cached archived files");
  auto jobs=store.history();require(jobs.size()==2&&jobs[0]["coverage"]=="local_only","finite local coverage retained in capture history");
 }
 require(digest(path).hash==original.hash&&fs::status(path).permissions()==permissions,"source ciphertext and file permissions remain unchanged after read and collection");
 require(readText(path).substr(0,16)!=std::string("SQLite format 3\0",16),"source remains encrypted, no decrypted SQLite is published");
 rejects([&]{LineDatabaseSource wrong(path,std::string(32,'0'));},"incorrect database key rejected");
 auto otherRoot=privateDir(root/"other-account"),otherPath=otherRoot/"synthetic-line.edb";copyVerified(path,otherPath);LineDatabaseSource other(otherPath,secret);require(other.account!=source.account,"separate selected account paths cannot accidentally merge");
 auto replacedPath=root/"replaced-account.edb",retainedPath=root/"retained-account.edb";copyVerified(path,replacedPath);std::string firstIdentity;
 {LineDatabaseSource first(replacedPath,secret);firstIdentity=first.account;}
 fs::rename(replacedPath,retainedPath);copyVerified(path,replacedPath);LineDatabaseSource replacement(replacedPath,secret);
 require(replacement.account!=firstIdentity,"a different database file replacing the same selected path gets a distinct source namespace");
#ifndef _WIN32
 auto replacementChat=chat(replacement,"group");fs::rename(replacedPath,root/"live-retained-account.edb");copyVerified(path,replacedPath);
 rejects([&]{replacement.chats();},"an already connected source refuses a replaced selected path");
 rejects([&]{replacement.read(replacementChat,{"message"},"","",control);},"history cannot silently continue through a replaced account database path");
#endif
 Control stopped;stopped.stopped=true;cancels([&]{LineDatabaseSource cancelled(path,secret,{},&stopped);},"connection cancellation checked before opening source");
 cancels([&]{source.chatsWithControl(stopped);},"chat listing honors cancellation");
 cancels([&]{source.read(group,{"message"},"","",stopped);},"history read honors cancellation");
 cancels([&]{source.bindMediaFile(group,"image",imagePath,false,&stopped);},"file association honors cancellation");
 require(digest(path).hash==original.hash,"wrong-key, cancelled and second-account paths preserve selected source");
 // Optional metadata tables may be absent, but malformed supported schemas fail.
 auto minimal=root/"minimal.edb";{
  LineCipher writer(minimal,secret,nullptr,true);writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");message(writer,"only","fallback-chat",start,0,"只有訊息資料表");
  writer.rows("INSERT INTO _message VALUES(?,?,?,?,?,?,?)",{"nullable-type","fallback-chat",nullptr,start+1,"LINE 的空類型欄位仍是文字訊息",nullptr,nullptr});
  writer.rows("INSERT INTO _message VALUES(?,?,?,?,?,?,?)",{"nested-metadata","nested-chat",nullptr,start,"",16,std::string(64,'[')+"0"+std::string(64,']')});
 }
 LineDatabaseSource minimalSource(minimal,secret);require(minimalSource.chats().size()==2&&minimalSource.read(chat(minimalSource,"fallback-chat"),{"message"},"","",control).items.size()==2,"genuine required message schema works without optional name tables or non-null content type");
 rejects([&]{minimalSource.read(chat(minimalSource,"nested-chat"),{"message"},"","",control);},"deeply nested source metadata is rejected before recursion becomes unbounded");
 auto incompatible=root/"incompatible.edb";{LineCipher writer(incompatible,secret,nullptr,true);writer.rows("CREATE TABLE _message(_id TEXT,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");}
 rejects([&]{LineDatabaseSource unsupported(incompatible,secret);},"missing unique message primary key rejected");
 std::cout<<checks<<" native LINE database checks passed; encrypted synthetic fixtures only.\n";return 0;
 }catch(const std::exception& error){std::cerr<<"Native LINE database test failed: "<<error.what()<<"\n";return 1;}}
