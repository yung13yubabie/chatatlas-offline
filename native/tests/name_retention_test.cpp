// Owned synthetic archives. Display-name evidence must not cross identities.
#include "core.hpp"
#include <iostream>
using namespace atlas;
namespace {
int checks=0;
void require(bool value,const char* message){++checks;if(!value)throw Error(message);}
J item(const std::string& sender,const std::string& name,bool resolved){return {{"source_id","message"},{"kind","message"},{"text","完整人工訊息"},{"sender_id",sender},{"sender_name",name},{"metadata",{{"sender_name_resolved",resolved}}}};}
}
int main(int argc,char** argv){try{
 require(argc==2,"owned fixture path required");auto root=absoluteNormalizedPath(path8(argv[1]));require(fs::is_directory(root)&&fs::is_empty(root),"fixture starts empty");Store store(root/"archive");
 const std::string mid="u"+std::string(32,'a'),otherMid="u"+std::string(32,'b');
 J chat={{"source_id",mid},{"title","人工同學"},{"kind","direct"},{"name_resolved",true}};
 auto cid=store.putChat("synthetic-name-account",chat);chat["title"]="未命名對話（1）";chat["name_resolved"]=false;store.putChat("synthetic-name-account",chat);
 require(store.db.rows("SELECT title FROM chats WHERE id=?",{cid})[0]["title"]=="人工同學","an explicitly unresolved source cannot replace an existing readable chat name");
 chat["title"]="更新後的名稱";chat["name_resolved"]=true;store.putChat("synthetic-name-account",chat);require(store.db.rows("SELECT title FROM chats WHERE id=?",{cid})[0]["title"]=="更新後的名稱","new resolved chat names remain authoritative");
 J raw={{"source_id",otherMid},{"title",otherMid},{"kind","direct"}};auto rawCid=store.putChat("synthetic-name-account",raw);raw["title"]="未命名對話（2）";raw["name_resolved"]=false;store.putChat("synthetic-name-account",raw);require(store.db.rows("SELECT title FROM chats WHERE id=?",{rawCid})[0]["title"]==raw["title"],"a legacy raw identifier is replaced by an understandable unresolved label");
 chat["name_resolved"]=false;chat["title"]="另一來源的對話";auto separate=store.putChat("synthetic-other-account",chat);require(separate!=cid&&store.db.rows("SELECT title FROM chats WHERE id=?",{separate})[0]["title"]==chat["title"],"display name evidence never crosses source accounts");
 store.putItems(cid,J::array({item(mid,"人工甲",true)}));auto incoming=item(mid,"名稱待確認",false);incoming["text"]="更新且保留完整文字";store.putItems(cid,J::array({incoming}));auto saved=store.db.rows("SELECT * FROM items WHERE chat_id=?",{cid})[0];
 require(saved["sender_name"]=="人工甲"&&saved["text"]==incoming["text"],"unresolved names retain earlier evidence without dropping new message text");require(J::parse(saved["metadata"].get<std::string>()).value("sender_name_from_previous",false),"retained name evidence has explicit local provenance");
 incoming=item(otherMid,"另一位名稱待確認",false);store.putItems(cid,J::array({incoming}));saved=store.db.rows("SELECT * FROM items WHERE chat_id=?",{cid})[0];require(saved["sender_name"]==incoming["sender_name"]&&saved["sender_id"]==otherMid,"a changed sender never inherits another person's name");
 incoming=item(otherMid,"人工乙",true);store.putItems(cid,J::array({incoming}));require(store.db.rows("SELECT sender_name FROM items WHERE chat_id=?",{cid})[0]["sender_name"]=="人工乙","new resolved sender names update normally");
 incoming=item(otherMid,"未知成員",false);incoming["metadata"]=J::object();store.putItems(cid,J::array({incoming}));require(store.db.rows("SELECT sender_name FROM items WHERE chat_id=?",{cid})[0]["sender_name"]=="未知成員","older sources without evidence flags preserve their established update behavior");
 store.putItems(rawCid,J::array({item(mid,mid,true)}));store.putItems(rawCid,J::array({item(mid,"名稱待確認",false)}));require(store.db.rows("SELECT sender_name FROM items WHERE chat_id=?",{rawCid})[0]["sender_name"]=="名稱待確認","legacy raw sender identifiers are not mistaken for names");
 auto unresolved=item(mid,"舊的未知成員",false);store.putItems(rawCid,J::array({unresolved}));auto next=item(mid,"新的未知成員",false);store.putItems(rawCid,J::array({next}));require(store.db.rows("SELECT sender_name FROM items WHERE chat_id=?",{rawCid})[0]["sender_name"]==next["sender_name"],"explicitly unresolved old labels do not become false resolved evidence");
 require(store.check({},false).empty(),"name preservation keeps archive identities and relations valid");std::cout<<checks<<" name evidence retention checks passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
