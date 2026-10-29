#include "line_batch.hpp"
#include "line_cipher.hpp"
#include "line_database.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace atlas;
namespace {
int checks=0;
const std::string firstKey="A1b2C3d4E5f60718293a4B5c6D7e8F90",secondKey="1234567890aBcDeF1234567890aBcDeF";
void require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void rejects(F action,const char* reason){bool rejected=false;try{action();}catch(const Error&){rejected=true;}require(rejected,reason);}
template<class F>void cancels(F action,const char* reason){bool cancelled=false;try{action();}catch(const Cancelled&){cancelled=true;}require(cancelled,reason);}
void fixture(const fs::path& path,const std::string& secret,const std::string& body,bool supported=true){
 LineCipher writer(path,secret,nullptr,true);
 if(!supported){writer.rows("CREATE TABLE unrelated(value TEXT)");return;}
 writer.rows("CREATE TABLE _chat(_id TEXT PRIMARY KEY)");
 writer.rows("CREATE TABLE _groupChat(_chatMid TEXT PRIMARY KEY,_chatName TEXT)");
 writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_from TEXT,_createdTime INTEGER,_text TEXT,_contentType INTEGER,_contentMetadata TEXT)");
 writer.rows("INSERT INTO _chat VALUES('shared-chat')");writer.rows("INSERT INTO _groupChat VALUES('shared-chat','同名人工聊天室')");
 writer.rows("INSERT INTO _message VALUES('same-message','shared-chat','person',1790784000000,?,0,'{}')",{body});
 writer.rows("INSERT INTO _message VALUES('same-file','shared-chat','person',1790784000001,'',14,?)",{J({{"FILE_NAME","same-name.txt"}}).dump()});
}
J select(const J& chats,const std::string& account,const std::string& original="shared-chat"){
 for(const auto& chat:chats)if(chat.value("source_account","")==account&&chat.value("original_source_id","")==original)return chat;
 throw Error("missing owned fixture chat");
}
std::string status(const LineBatchOpenResult& result,const fs::path& path){for(const auto& entry:result.entries)if(entry.path==absoluteNormalizedPath(path))return entry.status;throw Error("missing fixture entry");}
class TreeSource final : public Source {
 public:
 explicit TreeSource(std::string name){account=std::move(name);}
 J chats()override{return J::array({{{"source_id","parent"},{"title","人工社群"},{"kind","community"}},{{"source_id","child"},{"title","人工子聊天室"},{"kind","subchat"},{"parent_source_id","parent"}}});}
 Batch read(const J& chat,const std::set<std::string>&,const std::string&,const std::string&,Control& control)override{
  control.check();if(chat!=chats()[1])throw Error("wrong synthetic chat");Batch batch;batch.coverage="local_only";
  J item={{"source_id","same-message"},{"kind","message"},{"text",account},{"sent_at","2026-10-01T00:00:00Z"}};validateItem(item);batch.items.push_back(item);return batch;
 }
 fs::path download(const J& chat,const J&,const fs::path& stage,Control&)override{if(chat!=chats()[1])throw Error("wrong synthetic download chat");auto path=stage/"payload.bin";atomicText(path,account);return path;}
};
}
int main(int argc,char** argv){try{
 require(argc==2,"owned fixture directory required");const auto root=absoluteNormalizedPath(path8(argv[1]));require(fs::is_directory(root),"owned fixture exists");
 const auto first=root/"first.edb",second=root/"second.edb",unsupported=root/"feature.edb",plaintext=root/"archive.sqlite3",missing=root/"missing.edb",alias=root/"alias.edb",history=root/"older-copy.edb";
 fixture(first,firstKey,"第一份來源原文");fixture(second,secondKey,"第二份來源原文");fixture(unsupported,firstKey,"",false);atomicText(plaintext,"SQLite format 3\nowned plain synthetic archive");
 fs::create_hard_link(first,alias);copyVerified(first,history);
 const auto originalFirst=digest(first).hash,originalSecond=digest(second).hash;
 auto sourceA=std::make_shared<LineDatabaseSource>(first,firstKey),sourceB=std::make_shared<LineDatabaseSource>(second,secondKey);
 auto fileA=root/"original-a.txt",fileB=root/"original-b.txt";atomicText(fileA,"人工第一份附件\n");atomicText(fileB,"人工第二份附件\n");Control control;
 sourceA->bindMediaFile(sourceA->chats()[0],"same-file",fileA,false,&control);sourceB->bindMediaFile(sourceB->chats()[0],"same-file",fileB,false,&control);
 LineBatchSource batch({sourceA,sourceB},&control);auto chats=batch.chats();
 require(batch.sourceCount()==2&&chats.size()==2,"batch lists each source independently");
 auto chatA=select(chats,sourceA->account),chatB=select(chats,sourceB->account);
 require(chatA["source_id"]!=chatB["source_id"]&&chatA["title"]==chatB["title"],"same chat ID and title in different databases have distinct selection identities");
 require(chatA["source_database"]=="first.edb"&&chatB["source_database"]=="second.edb"&&chatA["source_label"]=="來源 1"&&chatB["source_label"]=="來源 2","visible source labels stay readable while database identity remains available internally");
 require(batch.storageChat(chatA)==sourceA->chats()[0]&&batch.accountFor(chatA)==sourceA->account,"storage restores exact single-database account and chat identity");
 auto readA=batch.read(chatA,{"message"},"","",control),readB=batch.read(chatB,{"message"},"","",control);
 require(readA.items[0]["text"]=="第一份來源原文"&&readB.items[0]["text"]=="第二份來源原文","batch reads route to the selected database rather than the first matching chat ID");
 auto stageA=privateDir(root/"download-a"),stageB=privateDir(root/"download-b");
 auto full=[](const Batch& value){for(const auto& attachment:value.attachments)if(attachment["variant"]=="full")return attachment;throw Error("missing full synthetic file");};
 require(digest(batch.download(chatA,full(readA),stageA,control)).hash==digest(fileA).hash&&digest(batch.download(chatB,full(readB),stageB,control)).hash==digest(fileB).hash,"same attachment IDs and names route to different verified source bytes");
 rejects([&]{auto forged=chatA;forged["source_account"]=sourceB->account;batch.read(forged,{"message"},"","",control);},"forged account metadata cannot redirect a batch chat");
 rejects([&]{auto forged=chatA;forged["title"]="changed title";batch.storageChat(forged);},"modified visible chat cannot change stored title");
 rejects([&]{batch.read(sourceA->chats()[0],{"message"},"","",control);},"an unwrapped same-ID chat cannot bypass source routing");
 rejects([&]{LineBatchSource invalid({sourceA,sourceA});},"duplicate source namespace is refused");
 rejects([&]{LineBatchSource empty({});},"empty batch source is refused");
 {
  Store store(root/"collection");collect(store,*sourceA,sourceA->chats(),control,{"message"});auto before=store.db.rows("SELECT id FROM items ORDER BY id");
  auto result=collect(store,batch,chats,control,{"message"});
  require(store.db.rows("SELECT * FROM chats").size()==2&&store.db.rows("SELECT * FROM items").size()==4,"single-source then batch collection does not duplicate the first source or merge the second");
  require(result["reused"]>=1&&result["downloaded"]>=1,"single-source archived media is reused while the other source media is downloaded separately");
  auto accounts=store.db.rows("SELECT DISTINCT account FROM chats ORDER BY account");require(accounts.size()==2&&accounts.dump().find(batch.account)==std::string::npos,"aggregate selection namespace is never persisted as an account");
  require(store.db.rows("SELECT id FROM items WHERE chat_id=? ORDER BY id",{key({"chat",sourceA->account,"shared-chat"})})==before,"single-source message identities remain stable in a batch");
  require(store.db.rows("SELECT title FROM chats WHERE title<>'同名人工聊天室'").empty(),"UI provenance fields never contaminate stored original chat titles");
  exportHtml(store);require(fs::is_regular_file(store.root/"index.html"),"separate batch sources produce a real offline reader");
  require(store.check().empty(),"batch archive identity, relations and media checks pass");
 }
 {
  auto one=std::make_shared<TreeSource>("synthetic-tree-one"),two=std::make_shared<TreeSource>("synthetic-tree-two");LineBatchSource tree({one,two});auto all=tree.chats();
  auto childOne=select(all,one->account,"child"),childTwo=select(all,two->account,"child");
  require(childOne["parent_source_id"]!=childTwo["parent_source_id"],"same parent community IDs are namespaced independently in batch selection");
  Store store(root/"community-collection");collect(store,tree,J::array({childOne,childTwo}),control,{"message"});
  require(store.db.rows("SELECT * FROM chats").size()==4&&store.db.rows("SELECT * FROM items").size()==2,"batch subchat collection inserts each parent under its original account");
  require(store.identityProblems().empty(),"original parent-community relationships remain valid");
  auto stage=privateDir(root/"tree-download");require(readText(tree.download(childTwo,J::object(),stage,control))==two->account,"generic callback routing also uses the selected child source");
  one->account="changed-source";rejects([&]{tree.chats();},"source namespace changes invalidate a live batch catalog");
 }
 size_t scans=0,callbacks=0;
 auto scanner=[&](Control& scope,const LineBatchKeyVisitor& visit,std::function<void(std::string)>){
  ++scans;for(const auto& secret:{firstKey,secondKey}){LineKeyCandidates candidates;const auto bytes="!"+secret+"!";candidates.append(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),true);++callbacks;if(visit(candidates,nullptr,[&]{scope.check();}))break;}
 };
 auto opened=openLineDatabases({first,alias,second,unsupported,plaintext,missing,first,history},control,{},scanner);
 require(scans==1&&callbacks==2,"all selected files share one scanner traversal with per-process key candidate callbacks");
 require(opened.readyCount()==3&&opened.skippedCount()==5&&opened.source&&opened.source->sourceCount()==3,"valid files survive invalid, duplicate, unsupported and unavailable selections");
 require(status(opened,alias)=="duplicate"&&status(opened,unsupported)=="unsupported"&&status(opened,plaintext)=="invalid"&&status(opened,missing)=="invalid","each skipped input retains a non-secret specific category");
 require(status(opened,history)=="ready"&&opened.source->chats().size()==3,"same-key historical copy remains an explicitly separate selected source, never claimed to be the active account");
 require(opened.summary().find("可讀取 3 份")!=std::string::npos&&opened.summary().find("略過 5 份")!=std::string::npos,"batch summary discloses all skipped files");
 bool earlyStop=false;openLineDatabases({first},control,{},[&](Control&,const LineBatchKeyVisitor& visit,std::function<void(std::string)>){LineKeyCandidates candidates;candidates.append(reinterpret_cast<const unsigned char*>(firstKey.data()),firstKey.size(),true);earlyStop=visit(candidates,nullptr,{});});
 require(earlyStop,"all matched selected files allow scan termination without unrelated processes");
 auto unavailable=openLineDatabases({second},control,{},[&](Control&,const LineBatchKeyVisitor& visit,std::function<void(std::string)>){LineKeyCandidates candidates;candidates.append(reinterpret_cast<const unsigned char*>(firstKey.data()),firstKey.size(),true);visit(candidates,nullptr,{});});
 require(!unavailable.source&&unavailable.readyCount()==0&&unavailable.entries[0].status=="unavailable"&&unavailable.summary().find("未找到可讀取")!=std::string::npos,"no matching key returns an explicit failure summary instead of an empty successful source");
 bool scannedInvalid=false;auto invalid=openLineDatabases({plaintext,missing},control,{},[&](Control&,const LineBatchKeyVisitor&,std::function<void(std::string)>){scannedInvalid=true;});
 require(!scannedInvalid&&!invalid.source&&invalid.skippedCount()==2,"invalid file preflight never reads a process");
 Control stopped;stopped.stopped=true;cancels([&]{openLineDatabases({first},stopped,{},scanner);},"batch stops before opening files or scanning when already cancelled");
 cancels([&]{batch.read(chatA,{"message"},"","",stopped);},"batch read preserves cancellation");
 cancels([&]{openLineDatabases({first},control,{},[](Control&,const LineBatchKeyVisitor& visit,std::function<void(std::string)>){LineKeyCandidates candidates;visit(candidates,nullptr,[]{throw Cancelled();});});},"scanner budget cancellation propagates rather than being reported as an unsupported file");
 rejects([&]{openLineDatabases({},control,{},scanner);},"no input is not an automatic whole-computer search");
 rejects([&]{openLineDatabases(std::vector<fs::path>(65,first),control,{},scanner);},"input count is bounded before processing");
 require(digest(first).hash==originalFirst&&digest(second).hash==originalSecond,"batch verification and collection never alter selected database ciphertext");
 std::cout<<checks<<" native LINE batch source checks passed; only owned synthetic databases and key callbacks used.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"LINE batch test failed: "<<e.what()<<"\n";return 1;}}
