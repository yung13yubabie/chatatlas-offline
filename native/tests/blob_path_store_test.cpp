#include "core.hpp"
#include <iostream>
using namespace atlas;
namespace {
size_t checks=0;const char* phase="setup";
void expect(bool ok,const char* why){++checks;if(!ok)throw Error(why);}
template<class F>void reject(F fn){bool failed=false;try{fn();}catch(const IntegrityError&){failed=true;}expect(failed,"Expected explicit blob-path integrity rejection");}
std::string str(const J&j,const char*k){return j.at(k).get<std::string>();}
std::map<std::string,std::string> snapshot(const fs::path&root){std::map<std::string,std::string> out;for(const auto&e:fs::recursive_directory_iterator(root))if(e.is_regular_file()&&e.path().filename()!=".vault-lock"&&e.path().filename().string().find("archive.sqlite3")!=0)out[e.path().lexically_relative(root).generic_string()]=digest(e.path()).hash;return out;}
}
int main(int argc,char**argv){try{
 if(argc!=2)throw Error("Provide empty owned fixture directory");auto base=fs::absolute(path8(argv[1]));if(!fs::is_directory(base)||!fs::is_empty(base))throw Error("Fixture directory must be empty");
 Store s(base/"collection");const auto cid=s.putChat("synthetic-blob-path-account",{{"source_id","room"},{"title","Artificial paths"},{"kind","group"}});s.putItems(cid,J::array({{{"source_id","message"},{"kind","message"},{"text","Artificial data only"}}}));
 const std::string bytes="Harmless arbitrary bytes: storage whitelist is not antivirus",hash=sha(bytes),canonical="assets/"+hash.substr(0,2)+"/"+hash+".bin";
 atomicText(within(s.root,canonical),bytes);auto d=digest(within(s.root,canonical));
 auto attach=[&](const char*id){return s.putAttachment(cid,{{"source_id",id},{"item_source_id","message"},{"name","artificial.bin"},{"expected_size",d.size},{"expected_sha256",hash}});};
 const auto aid=attach("first");s.saveBlob(aid,canonical,d);const auto second=attach("second");expect(s.reuseKnown(second),"Canonical dedupe remains usable");expect(s.db.rows("SELECT * FROM blobs").size()==1,"Dedupe must retain one blob");expect(s.reusable(aid),"Canonical blob reusable");expect(s.check().empty(),"Canonical initial integrity");exportHtml(s);
 auto original=s.db.rows("SELECT * FROM blobs").at(0);expect(validParts(s,original),"Pre-existing download cache must be valid");const auto bid=str(original,"id");
 phase="saveBlob rejected without registration";
 const std::string invalid="assets/noncanonical.bin",freshBytes="Independent harmless bytes for an unregistered digest";atomicText(within(s.root,invalid),freshBytes);auto fresh=digest(within(s.root,invalid));const auto freshAid=s.putAttachment(cid,{{"source_id","unregistered"},{"item_source_id","message"},{"name","inert.bin"},{"expected_size",fresh.size},{"expected_sha256",fresh.hash}});expect(s.db.rows("SELECT * FROM blobs WHERE sha256=?",{fresh.hash}).empty(),"Save fixture must not trip existing blob path mismatch");auto before=s.db.rows("SELECT * FROM blobs"),attachments=s.db.rows("SELECT * FROM attachments");reject([&]{s.saveBlob(freshAid,invalid,fresh);});expect(s.db.rows("SELECT * FROM blobs")==before&&s.db.rows("SELECT * FROM attachments")==attachments,"Rejected save changed database");s.db.execute("DELETE FROM attachments WHERE id=?",{freshAid});fs::remove(within(s.root,invalid));
 phase="registered path mutation with correct payload and cached downloads";
 for(const auto&rel:std::vector<std::string>{"assets/plain.bin","assets/inert.html","assets/inert.exe","assets/ff/"+hash+".bin","assets/"+hash.substr(0,2)+"/"+std::string(64,'a')+".bin","assets/"+hash.substr(0,2)+"/"+hash+".BIN","other/"+hash+".bin"}){
  atomicText(within(s.root,rel),bytes);auto actual=digest(within(s.root,rel));expect(actual.hash==hash&&actual.size==d.size,"Fixture must have real matching bytes");s.db.execute("UPDATE blobs SET path=? WHERE id=?",{rel,bid});auto blob=s.db.rows("SELECT * FROM blobs WHERE id=?",{bid}).at(0);expect(blob["sha256"]==original["sha256"]&&blob["size"]==original["size"],"Only path may change");auto files=snapshot(s.root);auto rows=s.db.rows("SELECT * FROM attachments");
  expect(!s.identityProblems().empty(),"Bad registered path must be identity problem");expect(!s.check().empty(),"Bad registered path must not pass check");reject([&]{s.reusable(aid);});reject([&]{s.reuseKnown(second);});expect(!validParts(s,blob),"Existing valid download cache must not mask invalid path");reject([&]{downloadParts(s,blob);});reject([&]{exportHtml(s);});bool zipRejected=false;try{makeZip(s,base/"rejected.zip");}catch(const Error&){zipRejected=true;}expect(zipRejected,"Malformed collection must reject ZIP");expect(!fs::exists(base/"rejected.zip"),"Rejected archive must not publish ZIP");expect(snapshot(s.root)==files,"Rejected operations changed old reader or attachment files");expect(s.db.rows("SELECT * FROM attachments")==rows,"Rejected operations changed attachment rows");
  s.db.execute("UPDATE blobs SET path=? WHERE id=?",{canonical,bid});fs::remove(within(s.root,rel));expect(s.reusable(aid)&&validParts(s,original),"Restored canonical blob remains reusable with cache");
 }
 expect(s.check().empty(),"Restored archive passes integrity");exportHtml(s);std::cout<<checks<<" blob path integration checks passed.\n";return 0;
 }catch(const std::exception&e){std::cerr<<"Blob path phase ["<<phase<<"]: "<<e.what()<<"\n";return 1;}}
