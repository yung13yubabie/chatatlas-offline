// Artificial browser-fixture extension. Never compiled into the consumer EXE.
#include "core.hpp"
#include <iostream>
using namespace atlas;
int main(int argc,char**argv){try{
 if(argc!=2)throw Error("Expected an existing artificial archive directory.");
 const fs::path root=path8(argv[1]);
 if(!fs::is_regular_file(root/"archive.sqlite3"))throw Error("Artificial archive is missing.");
 Store store(root);
 const auto accounts=store.db.rows("SELECT DISTINCT account FROM chats");
 if(accounts.size()!=1||accounts[0]["account"]!="synthetic-native-account")throw Error("Only the native artificial fixture is accepted.");
 const auto chats=store.db.rows("SELECT id FROM chats WHERE source_id='project' AND account='synthetic-native-account'");
 if(chats.size()!=1)throw Error("Artificial project room is missing.");
 const auto cid=chats[0]["id"].get<std::string>();
 constexpr size_t chunk=768*1024;
 std::string payload(chunk*2,'\0');for(size_t i=0;i<payload.size();++i)payload[i]=static_cast<char>(i%256);payload+="END";
 J result=J::array();
 for(const auto& fixture:std::vector<std::pair<std::string,std::string>>{{"多段下載範例",payload},{"空檔案範例",""}}){
  const auto&name=fixture.first;const auto&bytes=fixture.second;const auto hash=sha(bytes);
  const auto relative="assets/"+hash.substr(0,2)+"/"+hash+".bin";
  atomicText(within(root,relative),bytes);
  store.db.transaction([&]{
   store.putItems(cid,J::array({{{"source_id",name},{"kind","message"},{"text",name}}}));
   const auto aid=store.putAttachment(cid,{{"source_id",name},{"item_source_id",name},{"name",name+".bin"},{"expected_size",bytes.size()},{"expected_sha256",hash}});
   store.saveBlob(aid,relative,digest(within(root,relative)));
  });
  result.push_back({{"name",name},{"size",bytes.size()},{"sha256",hash}});
 }
 exportHtml(store);
 for(auto&item:result){const auto rows=store.db.rows("SELECT * FROM blobs WHERE sha256=?",{item["sha256"]});item["parts"]=downloadParts(store,rows.at(0))["parts"].size();}
 std::cout<<result.dump()<<"\n";return 0;
 }catch(const Error&e){std::cerr<<e.what()<<"\n";return 2;}catch(...){std::cerr<<"Artificial reader fixture could not be created.\n";return 2;}}
