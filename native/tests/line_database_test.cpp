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
void nameMetadataChecks(const fs::path& root,Control& control){
 const auto a="u"+std::string(32,'a'),b="u"+std::string(32,'b'),c="u"+std::string(32,'c'),d="u"+std::string(32,'d');
 const auto badProfile="u"+std::string(32,'e'),secretProfile="u"+std::string(32,'f'),self="u"+std::string(32,'0'),cached="u"+std::string(32,'1');
 const auto unnamedChat="u"+std::string(32,'2'),missingSender="u"+std::string(32,'3'),otherMissingSender="u"+std::string(32,'4');
 const auto group="c"+std::string(32,'5'),emptyGroup="c"+std::string(32,'6'),square="s"+std::string(32,'7'),room="r"+std::string(32,'8');
 const auto ambiguous="u"+std::string(32,'9'),squarePerson="p"+std::string(32,'1');
 const auto path=root/"names-and-schema-variants.edb";
 {
  LineCipher writer(path,secret,nullptr,true);
  writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");
  writer.rows("CREATE TABLE _CHAT(_ID TEXT PRIMARY KEY,_title TEXT)");
  writer.rows("CREATE TABLE _CONTACT(_MID TEXT,_DISPLAYNAME TEXT,_displayNameOverridden TEXT,_profileInfo TEXT)");
  writer.rows("CREATE TABLE _PROFILE(_MID TEXT PRIMARY KEY,_ProfileInfo TEXT)");
  writer.rows("CREATE TABLE _groupChat(_chatMid TEXT PRIMARY KEY,_displayName TEXT)");
  writer.rows("CREATE TABLE _squareChat(_squareChatMid TEXT PRIMARY KEY,_chatName TEXT)");
  writer.rows("CREATE TABLE _squareMember(_SQUAREMEMBERMID TEXT PRIMARY KEY,_PROFILEINFO TEXT)");
  writer.rows("CREATE TABLE _room(_mid TEXT PRIMARY KEY,_title TEXT)");
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{a," 真實名稱甲 ","　 \t",nullptr});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{b,"原名乙","同名成員",nullptr});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{c,"原名丙","同名成員",nullptr});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{d,nullptr,nullptr,J{{"MID",d},{"profile",{{"DISPLAY_NAME","個人資料內的名字"}}}}.dump()});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{badProfile,nullptr,nullptr,J{{"mid",a},{"displayName","不可套用別人的名字"}}.dump()});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{secretProfile,nullptr,nullptr,J{{"mid",secretProfile},{"displayName","SYNTHETIC_PROFILE_CREDENTIAL"},{"authToken","SYNTHETIC_PROFILE_CREDENTIAL"}}.dump()});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{squarePerson,squarePerson,nullptr,nullptr});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{ambiguous,"衝突顯示名一",nullptr,nullptr});
  writer.rows("INSERT INTO _CONTACT VALUES(?,?,?,?)",{ambiguous,"衝突顯示名二",nullptr,nullptr});
  writer.rows("INSERT INTO _PROFILE VALUES(?,?)",{self,J{{"mid",self},{"displayName","自己的顯示名稱"},{"authToken","SYNTHETIC_OWN_PROFILE_SECRET"}}.dump()});
  writer.rows("INSERT INTO _squareMember VALUES(?,?)",{squarePerson,J{{"displayName","社群成員名稱"}}.dump()});
  writer.rows("INSERT INTO _groupChat VALUES(?,?)",{group,"群組替代名稱"});writer.rows("INSERT INTO _groupChat VALUES(?,?)",{emptyGroup,"　"});
  writer.rows("INSERT INTO _squareChat VALUES(?,?)",{square,"社群替代名稱"});writer.rows("INSERT INTO _room VALUES(?,?)",{room,"家人小組"});
  for(const auto& mid:{a,b,c,d,badProfile,secretProfile,cached,unnamedChat,ambiguous,group,emptyGroup,square,room})writer.rows("INSERT INTO _CHAT VALUES(?,?)",{mid,mid==cached?"聊天室保留名稱":mid==group?"不應蓋掉群組真名":mid==unnamedChat?mid:""});
  message(writer,"sender-a",group,start,0,"完整文字",J::object(),a);message(writer,"sender-b",group,start+1,0,"另一段完整文字",J::object(),b);message(writer,"sender-c",group,start+2,0,"同名但不同成員",J::object(),c);
  message(writer,"sender-self",group,start+3,0,"自己發送的訊息",J::object(),self);message(writer,"sender-square",group,start+4,0,"非好友社群成員",J::object(),squarePerson);
  message(writer,"sender-unknown",group,start+5,0,"名稱未同步仍保留訊息",J::object(),missingSender);message(writer,"sender-unknown-two",group,start+6,0,"另一位未同步成員",J::object(),otherMissingSender);
  message(writer,"sender-cache",group,start+7,0,"聊天室快取名稱",J::object(),cached);
  message(writer,"system",group,start+8,18,"甲加入了群組",J::object(),self);
  message(writer,"system-json",group,start+9,18,J{{"message","保留安全的系統文字"},{"keyMaterial","SYNTHETIC_SYSTEM_TRANSPORT_SECRET"}}.dump(),J::object(),self);
  message(writer,"other-type",group,start+10,99,"不明格式的原始可讀文字",J::object(),self);
  message(writer,"call",group,start+11,6,"通話結束",J::object(),self);message(writer,"location",group,start+12,15,"集合位置",J::object(),self);
  message(writer,"link",group,start+13,12,"",{{"title","網站標題"},{"url","https://example.invalid/link"}},self);
  message(writer,"ext-image",group,start+14,21,"",{{"FILE_NAME","擴充圖片.png"}},self);
  message(writer,"private-body",group,start+15,1,J{{"keyMaterial","SYNTHETIC_IMAGE_TRANSPORT_SECRET"},{"fileName","圖片.png"}}.dump(),{{"FILE_NAME","圖片.png"}},self);
  message(writer,"private-name",group,start+16,14,"",{{"FILE_NAME","SYNTHETIC_FILENAME_CREDENTIAL"},{"keyMaterial","SYNTHETIC_FILENAME_CREDENTIAL"}},self);
 }
 const auto before=digest(path).hash;LineDatabaseSource source(path,secret,{},&control);
 require(chat(source,a)["title"]=="真實名稱甲"&&chat(source,a)["name_resolved"]==true,"case-insensitive name metadata and blank local alias retain the synced display name");
 require(chat(source,b)["title"]=="同名成員"&&chat(source,c)["title"]=="同名成員"&&chat(source,b)["source_id"]!=chat(source,c)["source_id"],"identical display names never merge distinct original MIDs");
 require(chat(source,d)["title"]=="個人資料內的名字"&&chat(source,d)["name_resolved"]==true,"an explicit same-identity profile object can supply a missing display name");
 require(chat(source,badProfile)["name_resolved"]==false&&chat(source,badProfile)["title"].get<std::string>().find("不可套用") == std::string::npos,"a different MID inside a cached profile is never assigned to this chat");
 require(chat(source,secretProfile)["name_resolved"]==false&&chat(source,secretProfile)["title"].get<std::string>().find("SYNTHETIC_")==std::string::npos,"credential material is never used as a display name");
 require(chat(source,cached)["title"]=="聊天室保留名稱"&&chat(source,cached)["kind"]=="direct","retained direct-chat title fills an absent contact record for that exact MID");
 require(chat(source,group)["title"]=="群組替代名稱"&&chat(source,square)["title"]=="社群替代名稱"&&chat(source,room)["title"]=="家人小組","optional name columns provide group, community and room names without replacing stronger labels");
 auto unknown=chat(source,unnamedChat),unresolvedGroup=chat(source,emptyGroup);
 require(unknown["kind"]=="direct"&&unknown["name_resolved"]==false&&unknown["title"].get<std::string>().starts_with("未命名對話（")&&unknown["title"].get<std::string>().find(unnamedChat)==std::string::npos,"unresolved direct chats use a clearly unnamed numbered label instead of their MID");
 require(unresolvedGroup["name_resolved"]==false&&unresolvedGroup["title"].get<std::string>().starts_with("未命名群組（"),"blank group names remain visibly unresolved");
 require(chat(source,ambiguous)["name_resolved"]==false,"conflicting duplicate contact labels are not guessed into a real name");
 auto batch=source.read(chat(source,group),{"message","thread"},"","",control);
 const auto item=[&](const std::string& mid)->J{for(const auto& row:batch.items)if(row["source_id"]==mid)return row;throw std::runtime_error("missing name fixture message");};
 require(item("sender-a")["sender_name"]=="真實名稱甲"&&item("sender-self")["sender_name"]=="自己的顯示名稱","friend and own-profile sender names are resolved without changing sender IDs");
 require(item("sender-b")["sender_name"]==item("sender-c")["sender_name"]&&item("sender-b")["sender_id"]!=item("sender-c")["sender_id"],"same-name senders retain their separate original identities");
 require(item("sender-square")["sender_name"]=="社群成員名稱","a valid member profile can replace an unusable MID-valued contact name");
 require(item("sender-cache")["sender_name"]=="聊天室保留名稱","a direct-chat cache label can also name the matching sender");
 auto unknownSender=item("sender-unknown"),otherUnknown=item("sender-unknown-two");
 require(unknownSender["sender_id"]==missingSender&&unknownSender["sender_name"].get<std::string>().starts_with("未知成員（")&&unknownSender["sender_name"]!=otherUnknown["sender_name"]&&unknownSender["metadata"]["sender_name_resolved"]==false,"unknown senders have separate numbered display labels while preserving original MIDs");
 require(item("system")["text"]=="[系統訊息]\n甲加入了群組"&&item("system-json")["text"]=="[系統訊息]\n保留安全的系統文字","chat events show human labels and keep readable system text without transport secrets");
 require(item("other-type")["text"]=="[其他訊息]\n不明格式的原始可讀文字","unrecognized message types retain readable content without exposing numeric engineering labels");
 require(item("call")["text"].get<std::string>().starts_with("[通話]")&&item("location")["text"].get<std::string>().starts_with("[位置]"),"documented call and location types use the correct human labels");
 require(item("link")["text"].get<std::string>().find("https://example.invalid/link")!=std::string::npos,"documented link messages retain their visible link data");
 require(std::any_of(batch.attachments.begin(),batch.attachments.end(),[](const J& row){return row["item_source_id"]=="ext-image"&&row["kind"]=="image";}),"documented extended-image messages are passed to the image resolver");
 require(batch.items.dump().find("SYNTHETIC_")==std::string::npos,"profile, system payload and filename credentials never enter displayed or archived message text");
 require(source.read(chat(source,group),{"message"},"","",control).items==batch.items,"unresolved sender numbering is stable across repeated reads of this source");
 require(digest(path).hash==before,"name resolution never changes the selected source database");
 const auto incomplete=root/"optional-name-table-missing-id.edb";
 {LineCipher writer(incomplete,secret,nullptr,true);writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");writer.rows("CREATE TABLE _contact(unrecognized_identity TEXT,_displayName TEXT)");message(writer,"retained",unnamedChat,start,0,"名稱表不相容仍保留對話",J::object(),missingSender);}
 LineDatabaseSource incompleteSource(incomplete,secret);auto incompleteChat=chat(incompleteSource,unnamedChat);
 require(incompleteChat["name_resolved"]==false&&incompleteSource.read(incompleteChat,{"message"},"","",control).items[0]["text"]=="名稱表不相容仍保留對話","optional unsupported name metadata does not hide the actual message history");
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
 nameMetadataChecks(root,control);
 auto incompatible=root/"incompatible.edb";{LineCipher writer(incompatible,secret,nullptr,true);writer.rows("CREATE TABLE _message(_id TEXT,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");}
 rejects([&]{LineDatabaseSource unsupported(incompatible,secret);},"missing unique message primary key rejected");
 std::cout<<checks<<" native LINE database checks passed; encrypted synthetic fixtures only.\n";return 0;
 }catch(const std::exception& error){std::cerr<<"Native LINE database test failed: "<<error.what()<<"\n";return 1;}}
