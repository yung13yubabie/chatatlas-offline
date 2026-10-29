// Owned synthetic archives only. No live LINE process, DB, text or attachment.
#include "text_export.hpp"
#include <algorithm>
#include <iostream>
using namespace atlas;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw Error(message);}
template<class F>void rejects(F fn,const char* message){bool failed=false;try{fn();}catch(const Error&){failed=true;}require(failed,message);}
J manifest(const fs::path& directory){return J::parse(readText(directory/"export-complete.json"));}
std::string body(const fs::path& directory,size_t index=0){auto record=manifest(directory);return readText(within(directory,record["chats"][index]["file"].get<std::string>()));}
size_t completed(const fs::path& root){size_t count=0;auto directory=root/"text-exports";if(fs::exists(directory))for(const auto& entry:fs::directory_iterator(directory))if(fs::is_regular_file(entry.path()/"export-complete.json"))++count;return count;}
bool partialPresent(const fs::path& root){auto directory=root/"text-exports";if(fs::exists(directory))for(const auto& entry:fs::directory_iterator(directory))if(utf8(entry.path().filename()).starts_with(".partial-"))return true;return false;}
J message(const std::string& id,const std::string& content,int ordinal){return {{"source_id",id},{"kind","message"},{"text",content},{"sender_name","人工發話者"},{"sent_at","2026-10-29T00:00:00Z"},{"ordinal",ordinal},{"metadata",{{"keyMaterial","SYNTHETIC-NEVER-EXPORT-METADATA"},{"accessToken","SYNTHETIC-NEVER-EXPORT-TOKEN"}}}};}
std::string attachment(Store& store,const std::string& cid,const std::string& sid,const std::string& name,const std::string& variant="full"){
 return store.putAttachment(cid,{{"source_id",sid},{"item_source_id","main"},{"name",name},{"kind","file"},{"variant",variant},{"version","1"}});
}
fs::path saveAttachment(Store& store,const std::string& aid,const std::string& content,bool preview=false){
 const auto hash=sha(content),relative="assets/"+hash.substr(0,2)+"/"+hash+".bin";
 auto path=within(store.root,relative);atomicText(path,content);store.saveBlob(aid,relative,digest(path),preview);return path;
}
}
int main(int argc,char** argv){try{
 require(argc==2,"owned empty fixture directory required");auto fixture=absoluteNormalizedPath(path8(argv[1]));
 require(fs::is_directory(fixture)&&fs::is_empty(fixture),"fixture starts empty");
 Store store(fixture/"archive");std::string first,second,unselected;
 store.db.transaction([&]{
  first=store.putChat("synthetic-account-one",{{"source_id","same-room"},{"title","同名/聊天室"},{"kind","group"}});
  second=store.putChat("synthetic-account-two",{{"source_id","same-room"},{"title","同名/聊天室"},{"kind","group"}});
  unselected=store.putChat("synthetic-account-one",{{"source_id","not-selected"},{"title","不選取"},{"kind","group"}});
  J items=J::array({message("main","你好\n第二行\r\n第三行\t文字",0)});
  for(int index=0;index<501;++index)items.push_back(message("page-"+std::to_string(index),"人工分頁訊息 "+std::to_string(index),index+1));
  auto thread=message("thread","人工討論串",600);thread["kind"]="thread";thread["parent_source_id"]="main";items.push_back(thread);
  auto untimed=message("untimed","未記錄時間的訊息",0);untimed["sent_at"]=nullptr;items.push_back(untimed);
  auto recalled=message("recalled","SYNTHETIC-RECALLED-MUST-NOT-APPEAR",601);recalled["state"]="recalled";items.push_back(recalled);
  auto note=message("not-a-message","SYNTHETIC-NOTE-MUST-NOT-APPEAR",602);note["kind"]="note";items.push_back(note);
  store.putItems(first,items);
  store.putItems(second,J::array({message("main","SYNTHETIC-OTHER-ACCOUNT-PRIVATE",0)}));
  store.putItems(unselected,J::array({message("main","SYNTHETIC-UNSELECTED-CHAT-PRIVATE",0)}));
 });
 auto saved=attachment(store,first,"saved","原檔.txt");saveAttachment(store,saved,"synthetic saved payload");
 attachment(store,first,"saved","optional-preview-must-not-appear.png","thumbnail");
 auto preview=attachment(store,first,"preview","預覽圖.png","thumbnail");saveAttachment(store,preview,"synthetic preview payload",true);
 auto expired=attachment(store,first,"expired","過期圖片.jpg");store.db.execute("UPDATE attachments SET status='unavailable' WHERE id=?",{expired});
 auto failed=attachment(store,first,"failed","失敗檔案.zip");store.db.execute("UPDATE attachments SET status='failed' WHERE id=?",{failed});
 auto pending=attachment(store,first,"pending","待下載.bin");(void)pending;
 auto damaged=attachment(store,first,"damaged","損壞檔案.bin");auto damagedPath=saveAttachment(store,damaged,"synthetic later damaged bytes");atomicText(damagedPath,"damaged");

 auto firstOutput=exportChatText(store,{first,first});
 require(fs::is_directory(firstOutput)&&firstOutput.parent_path()==store.root/"text-exports","text export returns a generation below the archive");
 require(!partialPresent(store.root),"successful publication leaves no partial folder");
 auto info=manifest(firstOutput);require(info["complete"]==true&&info["chats"].size()==1&&info["chats"][0]["chat_id"]==first,"selection is deduplicated and completion manifest identifies the selected chat");
 auto exported=body(firstOutput);
 require(exported.starts_with(std::string("\xef\xbb\xbf",3)),"TXT is UTF8 with BOM");
 require(exported.find("聊天室：同名/聊天室")!=std::string::npos&&exported.find("人工發話者")!=std::string::npos,"Unicode title and sender are preserved");
 require(exported.find("  你好\r\n  第二行\r\n  第三行\t文字\r\n")!=std::string::npos,"multiline text is preserved with unambiguous indented CRLF lines");
 require(exported.find("[時間未記錄]")<exported.find("人工分頁訊息 0"),"nullable timestamp pagination starts before timed messages");
 require(info["chats"][0]["items"]==505&&exported.find("人工分頁訊息 500")!=std::string::npos&&exported.find("人工討論串")!=std::string::npos,"keyset pagination exports all messages and threads across the 500-row boundary");
 require(exported.find("SYNTHETIC-NEVER-EXPORT-METADATA")==std::string::npos&&exported.find("SYNTHETIC-NEVER-EXPORT-TOKEN")==std::string::npos,"item metadata and credential-shaped metadata never enter TXT");
 require(exported.find("SYNTHETIC-OTHER-ACCOUNT-PRIVATE")==std::string::npos&&exported.find("SYNTHETIC-UNSELECTED-CHAT-PRIVATE")==std::string::npos,"only explicitly selected source and chat text is exported");
 require(exported.find("SYNTHETIC-NOTE-MUST-NOT-APPEAR")==std::string::npos&&exported.find("SYNTHETIC-RECALLED-MUST-NOT-APPEAR")==std::string::npos&&exported.find("[訊息已收回]")!=std::string::npos,"nonconversation items and recalled message bodies are not published");
 require(exported.find("原檔.txt（已保存原檔）")!=std::string::npos&&exported.find("預覽圖.png（僅有預覽圖）")!=std::string::npos,"saved originals and previews have distinct verified labels");
 require(exported.find("過期圖片.jpg（未取得）")!=std::string::npos&&exported.find("失敗檔案.zip（下載失敗）")!=std::string::npos&&exported.find("待下載.bin（待下載）")!=std::string::npos,"unavailable failed and pending attachments are not reported saved");
 require(exported.find("損壞檔案.bin（檔案遺失或校驗失敗）")!=std::string::npos,"a registered but damaged blob is not called a saved original");
 require(exported.find("optional-preview-must-not-appear")==std::string::npos,"redundant preview does not obscure a saved original");
 const auto outputPath=within(firstOutput,info["chats"][0]["file"].get<std::string>());const auto outputDigest=digest(outputPath);
 require(outputDigest.hash==info["chats"][0]["sha256"].get<std::string>()&&outputDigest.size==info["chats"][0]["bytes"].get<uint64_t>(),"completion manifest checksum and length match actual TXT bytes");
 auto bothOutput=exportChatText(store,{first,second});auto both=manifest(bothOutput);
 require(both["chats"].size()==2&&both["chats"][0]["file"]!=both["chats"][1]["file"],"same chat names and source IDs from separate accounts never collide");
 require(body(bothOutput,0).find("SYNTHETIC-OTHER-ACCOUNT-PRIVATE")==std::string::npos&&body(bothOutput,1).find("SYNTHETIC-OTHER-ACCOUNT-PRIVATE")!=std::string::npos,"each source remains in its own TXT");
 auto repeated=exportChatText(store,{second});require(repeated!=firstOutput&&repeated!=bothOutput&&digest(outputPath).hash==outputDigest.hash,"later exports create a new generation without overwriting earlier files");
 const auto initialCompleted=completed(store.root);
 rejects([&]{exportChatText(store,{});},"empty scope is rejected");
 rejects([&]{exportChatText(store,{"../not-a-chat"});},"malformed identifiers cannot escape the export folder");
 rejects([&]{exportChatText(store,{sha("synthetic-missing-chat")});},"missing chat never broadens to other archive sources");
 bool cancelledBefore=false;try{exportChatText(store,{first},[]{throw Cancelled();});}catch(const Cancelled&){cancelledBefore=true;}
 require(cancelledBefore&&completed(store.root)==initialCompleted&&!partialPresent(store.root),"cancellation before export leaves earlier generations intact");
 bool staged=false,cancelledDuring=false;
 try{exportChatText(store,{first,second},[&]{if(partialPresent(store.root)){staged=true;throw Cancelled();}});}catch(const Cancelled&){cancelledDuring=true;}
 require(staged&&cancelledDuring&&completed(store.root)==initialCompleted&&!partialPresent(store.root),"cancellation after staging publishes no apparently complete TXT generation");
 bool wrotePartial=false,cancelledAfterWrite=false;
 try{exportChatText(store,{first,second},[&]{auto directory=store.root/"text-exports";for(const auto& entry:fs::directory_iterator(directory))if(utf8(entry.path().filename()).starts_with(".partial-")){for(const auto& partial:fs::directory_iterator(entry.path()))if(partial.path().extension()==".txt"&&fs::file_size(partial.path())>1024){wrotePartial=true;throw Cancelled();}}});}catch(const Cancelled&){cancelledAfterWrite=true;}
 require(wrotePartial&&cancelledAfterWrite&&completed(store.root)==initialCompleted&&!partialPresent(store.root),"cancellation in streamed message output removes the unpublished batch");
 store.db.execute("UPDATE chats SET account='synthetic-tampered-account' WHERE id=?",{first});
 rejects([&]{exportChatText(store,{first});},"tampered source namespace cannot publish text under another account");
 store.db.execute("UPDATE chats SET account='synthetic-account-one' WHERE id=?",{first});
 require(completed(store.root)==initialCompleted&&digest(outputPath).hash==outputDigest.hash,"invalid and cancelled exports preserve previously completed bytes");
 require(store.db.rows("SELECT count(*) AS n FROM items WHERE chat_id=?",{second})[0]["n"]==1,"TXT export never changes saved source content");
 std::cout<<checks<<" native chat text export checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Text export test failed: "<<e.what()<<'\n';return 1;}}
