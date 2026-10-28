#include "core.hpp"
#include <iostream>
#include <fstream>
using namespace atlas;
static int passed=0;static void expect(bool ok,const char*msg){if(!ok)throw std::runtime_error(msg);passed++;}template<class F>void rejected(F fn,const char*msg){bool failed=false;try{fn();}catch(...){failed=true;}expect(failed,msg);}
static void containmentTests(const fs::path&root){
 expect(canonicalHash(std::string(64,'a')), "canonical lowercase hash accepted");
 for(const auto&value:std::vector<std::string>{std::string(64,'A'),std::string(63,'a'),"../synthetic",""})
  expect(!canonicalHash(value), "invalid hash rejected");
 std::vector<std::string>bad={"../../../outside-synthetic",utf8(root.parent_path()/"absolute-synthetic"),"C:/outside-synthetic","..\\outside-synthetic",std::string("bad\0name",8),std::string(64,'A'),std::string(64,'0'),""};
 for(size_t n=0;n<bad.size();n++){
  auto p=root.parent_path()/(root.filename().string()+"-identity-"+std::to_string(n));
  {Store s(p);std::string cid;
   s.db.transaction([&]{cid=s.putChat("synthetic-regression",{{"source_id","room"},{"title","Artificial test"}});s.putItems(cid,J::array({{{"source_id","message"},{"kind","message"},{"text","Artificial text"}}}));});
   exportHtml(s);auto entry=readText(p/"index.html");
   auto generations=std::distance(fs::directory_iterator(p/"reader"),fs::directory_iterator{});
   s.db.execute("PRAGMA foreign_keys=OFF");s.db.execute("UPDATE chats SET id=?",{bad[n]});s.db.execute("UPDATE items SET chat_id=?",{bad[n]});s.db.execute("PRAGMA foreign_keys=ON");
   auto problems=s.check({},false);expect(std::any_of(problems.begin(),problems.end(),[](const J&v){return v["code"]=="identity";}),"tampered identity detected");
   rejected([&]{exportHtml(s);},"tampered identity cannot publish reader");expect(readText(p/"index.html")==entry,"invalid identity preserves prior reader");
   expect(std::distance(fs::directory_iterator(p/"reader"),fs::directory_iterator{})==generations,"invalid identity creates no generation");
   auto zip=p;zip+=".zip";rejected([&]{makeZip(s,zip);},"invalid identity cannot produce ZIP");expect(!fs::exists(zip),"invalid identity creates no ZIP");
  }fs::remove_all(p);
 }
 {auto p=root.parent_path()/(root.filename().string()+"-bad-hash");{Store s(p);Control c;DemoSource source;collect(s,source,source.chats(),c);auto blob=s.db.rows("SELECT * FROM blobs LIMIT 1")[0];blob["sha256"]="../synthetic";rejected([&]{downloadParts(s,blob);},"download rejects malformed hash directly");s.db.execute("UPDATE blobs SET sha256=upper(sha256)");expect(!s.identityProblems().empty(),"stored hash must be lowercase");rejected([&]{exportHtml(s);},"uppercase checksum cannot publish reader");}fs::remove_all(p);}
 {auto p=root.parent_path()/(root.filename().string()+"-reader-link"),outside=root.parent_path()/(root.filename().string()+"-outside");
  {Store s(p);s.db.transaction([&]{auto cid=s.putChat("synthetic-regression",{{"source_id","room"},{"title","Artificial test"}});s.putItems(cid,J::array({{{"source_id","message"},{"kind","message"}}}));});exportHtml(s);auto entry=readText(p/"index.html");fs::remove_all(p/"reader");fs::create_directories(outside);std::error_code ec;fs::create_directory_symlink(outside,p/"reader",ec);if(!ec){rejected([&]{exportHtml(s);},"reader symlink escape rejected");expect(fs::is_empty(outside),"reader link destination unchanged");expect(readText(p/"index.html")==entry,"reader link preserves old entrypoint");}else std::cout<<"SKIP: account cannot create directory links\n";
  }fs::remove_all(p);fs::remove_all(outside);
 }
}
int main(int argc,char**argv){auto root=fs::temp_directory_path()/path8("atlas-native-test-"+sha(now()+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())).substr(0,12));try{
 expect(sha("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855","empty SHA");expect(sha("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","abc SHA");expect(base64("abc")=="YWJj"&&base64("a")=="YQ==","base64");expect(normalizeTime("2026-01-01T08:30:00+08:00")=="2026-01-01T00:30:00.000+00:00","time zone");rejected([&]{normalizeTime("2026-02-30T08:00:00Z");},"bad date");rejected([&]{normalizeTime("2026-01-01T08:00:00");},"missing zone");expect(safeName("CON")=="_CON","Windows reserved name");rejected([&]{within(root,"../private");},"path traversal");
 {
 Store s(root);Control control;DemoSource demo;auto first=collect(s,demo,demo.chats(),control);expect(first["items"]==19,"native demo count");expect(first["downloaded"]==2&&demo.downloads==2,"exact content reuse");expect(s.check().empty(),"healthy archive");auto count=s.summary()["items"];DemoSource second(2);auto update=collect(s,second,second.chats(),control);expect(update["downloaded"]==0&&second.downloads==0&&update["reused"]==3,"second capture has zero downloads");expect(s.summary()["items"]==count,"message update has no duplicates");auto cid=s.db.rows("SELECT id FROM chats WHERE source_id='project'")[0]["id"].get<std::string>();expect(s.db.rows("SELECT text FROM items WHERE chat_id=? AND source_id='m1'",{cid})[0]["text"].get<std::string>().find("已更新")!=std::string::npos,"latest message replaces old");
 rejected([&]{Store other(root);},"archive locking");
 rejected([&]{s.db.transaction([&]{s.putItems(cid,J::array({{{"source_id","orphan"},{"kind","comment"}}}));});},"context parent required");
 rejected([&]{s.db.transaction([&]{s.putItems(cid,J::array({{{"source_id","a"},{"kind","comment"},{"parent_source_id","b"}},{{"source_id","b"},{"kind","comment"},{"parent_source_id","a"}}}));});},"cycle rejected");
 rejected([&]{s.db.transaction([&]{s.putItems(cid,J::array({{{"source_id","m1"},{"kind","album"}}}));});},"kind mutation rejected");
 rejected([&]{s.db.transaction([&]{s.putAttachment(cid,{{"source_id","missing-file"},{"item_source_id","absent"},{"name","file"}});});},"attachment parent required");
 s.db.transaction([&]{s.putItems(cid,J::array({{{"source_id","m2"},{"kind","message"},{"text","must disappear"},{"title","must disappear"},{"state","recalled"}}}));});auto recalled=s.db.rows("SELECT text,title FROM items WHERE chat_id=? AND source_id='m2'",{cid})[0];expect(recalled["text"]==""&&recalled["title"]=="","recall hides body and title");expect(s.db.rows("SELECT saved_before FROM attachments WHERE chat_id=? AND source_id='checklist'",{cid})[0]["saved_before"]==1,"recall retains file relation");
 exportHtml(s);auto before=readText(root/"index.html");expect(before.find("knowledge-map")!=std::string::npos,"knowledge graph HTML");expect(before.find("connect-src 'none'")!=std::string::npos,"offline CSP");rejected([&]{exportHtml(s,[]{throw Cancelled();});},"cancelled publication");expect(readText(root/"index.html")==before,"previous reader survives cancellation");
 auto blob=s.db.rows("SELECT * FROM blobs LIMIT 1")[0];auto file=within(root,blob["path"]);atomicText(file,"corrupted");expect(!s.check().empty(),"corruption reported");DemoSource repair;collect(s,repair,repair.chats(),control);expect(s.check().empty(),"corruption repaired");
 s.db.transaction([&]{s.db.execute("UPDATE attachments SET blob_id=NULL,status='pending'");s.db.execute("DELETE FROM blobs");});DemoSource recovered;auto recovery=collect(s,recovered,recovered.chats(),control);expect(recovery["downloaded"]==0&&recovered.downloads==0,"interrupted registration reuses copied files");exportHtml(s);expect(s.check().empty(),"recovered archive healthy");
 auto info=downloadParts(s,s.db.rows("SELECT * FROM blobs LIMIT 1")[0]);atomicText(within(root,info["parts"][0]["path"]),"broken");auto problems=s.check();expect(std::any_of(problems.begin(),problems.end(),[](const J&v){return v["code"]=="reader_data";}),"reader damage is distinct");exportHtml(s);expect(s.check().empty(),"rebuilding repairs reader without download");
 atomicText(within(root,"assets/unknown.bin"),"kept");auto issues=s.check();expect(std::any_of(issues.begin(),issues.end(),[](const J&v){return v["code"]=="unregistered_file";}),"orphan file reported");
 auto zip=root.parent_path()/(root.filename().string()+".zip");makeZip(s,zip);expect(fs::file_size(zip)>0,"ZIP created");rejected([&]{makeZip(s,zip);},"existing ZIP not overwritten");if(argc>1&&std::string(argv[1])=="--keep"){std::cout<<utf8(root)<<"\n"<<utf8(zip)<<"\n";}else fs::remove(zip);
 auto job=s.db.rows("SELECT id FROM jobs ORDER BY id DESC LIMIT 1")[0]["id"];s.db.execute("UPDATE jobs SET status='running' WHERE id=?",{job});
 }
 {Store again(root);expect(again.db.rows("SELECT status FROM jobs ORDER BY id DESC LIMIT 1")[0]["status"]=="interrupted","interrupted job recovered");}

 {auto p=root.parent_path()/(root.filename().string()+"-retry");{Store s(p);struct Temporary:DemoSource{int attempts=0;fs::path download(const J&c,const J&a,const fs::path&d,Control&control)override{if(attempts++==0)throw RetryableDownload("temporary");return DemoSource::download(c,a,d,control);}}source;Control c;auto result=collect(s,source,source.chats(),c);expect(result["failed"]==0&&source.attempts==3,"temporary transfer retries safely");}fs::remove_all(p);}
 {auto p=root.parent_path()/(root.filename().string()+"-expired");{Store s(p);struct Expired:DemoSource{fs::path download(const J&,const J&,const fs::path&,Control&)override{throw DownloadUnavailable("expired");}}source;Control c;auto result=collect(s,source,source.chats(),c);expect(result["unavailable"]==3&&result["downloaded"]==0,"expired attachments are distinct");}fs::remove_all(p);}
 {auto p=root.parent_path()/(root.filename().string()+"-changed");{Store s(p);struct Changed:DemoSource{int calls=0;fs::path download(const J&,const J&,const fs::path&,Control&)override{calls++;throw CapabilityError("identity changed");}}source;Control c;rejected([&]{collect(s,source,source.chats(),c);},"identity failure stops capture");expect(source.calls==1,"identity failure is never retried");expect(s.db.rows("SELECT status FROM jobs LIMIT 1")[0]["status"]=="failed","identity failure is visible");}fs::remove_all(p);}
 {auto p=root.parent_path()/(root.filename().string()+"-bytes");{Store s(p);struct Wrong:DemoSource{fs::path download(const J&,const J&,const fs::path&d,Control&)override{auto p=d/"payload.bin";atomicText(p,"wrong bytes");return p;}}source;Control c;auto result=collect(s,source,source.chats(),c);expect(result["failed"]==3&&result["downloaded"]==0,"wrong bytes never become saved");expect(s.db.rows("SELECT count(*) AS n FROM blobs")[0]["n"]==0,"failed payload is not registered");}fs::remove_all(p);}
 {auto p=root.parent_path()/(root.filename().string()+"-separate");{Store s(p);std::string one,two;s.db.transaction([&]{one=s.putChat("account-a",{{"source_id","same"},{"title","Same name"},{"kind","group"}});two=s.putChat("account-b",{{"source_id","same"},{"title","Same name"},{"kind","group"}});});expect(one!=two,"same-name chats from separate accounts stay separate");}fs::remove_all(p);}
 containmentTests(root);
 if(argc<=1||std::string(argv[1])!="--keep")fs::remove_all(root);std::cout<<"native core: "<<passed<<" checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAILED after "<<passed<<" checks: "<<e.what()<<"\n";return 1;}}
