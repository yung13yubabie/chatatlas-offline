#include "core.hpp"
#include "import.hpp"
#include "file.hpp"
#include <cassert>
#include <iostream>
using namespace atlas;
namespace {
int checks=0;
const char* phase="initial fixture";
void require(bool value){++checks;assert(value);}
std::map<std::string,std::string> hashes(const fs::path& root){std::map<std::string,std::string> out;for(const auto& e:fs::recursive_directory_iterator(root))if(e.is_regular_file())out[e.path().lexically_relative(root).generic_string()]=digest(e.path()).hash;return out;}
template<class F> void reject(F fn){bool caught=false;try{fn();}catch(const Error&){caught=true;}require(caught);}
fs::path clone(const fs::path& source,const fs::path& to){fs::copy(source,to,fs::copy_options::recursive);return to;}
#ifdef _WIN32
std::wstring acl(const fs::path& p){PSECURITY_DESCRIPTOR sd=nullptr;require(GetNamedSecurityInfoW(const_cast<LPWSTR>(p.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,nullptr,&sd)==ERROR_SUCCESS);LPWSTR text=nullptr;require(ConvertSecurityDescriptorToStringSecurityDescriptorW(sd,SDDL_REVISION_1,DACL_SECURITY_INFORMATION,&text,nullptr));std::wstring result=text;LocalFree(text);LocalFree(sd);return result;}
std::map<std::string,std::wstring> allAcls(const fs::path& root){std::map<std::string,std::wstring> result;result["."]=acl(root);for(const auto&e:fs::recursive_directory_iterator(root))result[e.path().lexically_relative(root).generic_string()]=acl(e.path());return result;}
void broadDirectory(const fs::path& p){PSECURITY_DESCRIPTOR sd=nullptr;require(ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;WD)",SDDL_REVISION_1,&sd,nullptr));SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};require(CreateDirectoryW(p.c_str(),&sa));LocalFree(sd);}
#endif
}
int main(int argc,char**argv){try{
 require(argc==2);const fs::path root=argv[1];require(fs::is_directory(root));auto live=root/"live",source=root/"extracted";
#ifdef _WIN32
 broadDirectory(source);auto originalAcl=acl(source);
#else
 fs::create_directory(source);
#endif
 {
  Store s(live);DemoSource demo;Control control;collect(s,demo,demo.chats(),control);exportHtml(s);
  s.db.backup(source/"archive.sqlite3");fs::copy(live/"assets",source/"assets",fs::copy_options::recursive);
 }
 atomicText(source/"index.html","UNTRUSTED IMPORTED HTML MUST NOT BE USED");fs::create_directory(source/"reader");atomicText(source/"reader"/"untrusted.js","UNTRUSTED SCRIPT");
#ifdef _WIN32
 auto sourceAcls=allAcls(source);
#endif
 phase="initial valid import";auto original=hashes(source);auto result=importBackup(source,root/"restored");require(result.root==fs::absolute(root/"restored"));require(hashes(source)==original);
#ifdef _WIN32
 require(acl(source)==originalAcl&&allAcls(source)==sourceAcls);requirePrivateDirectory(result.root);bool refused=false;try{Store old(source);}catch(const Error&){refused=true;}require(refused);
#endif
 {Store restored(result.root);require(restored.check().empty());require(readText(result.root/"index.html").find("UNTRUSTED IMPORTED")==std::string::npos);require(readText(result.root/"index.html").find("connect-src 'none'")!=std::string::npos);require(!fs::exists(result.root/"reader"/"untrusted.js"));}
 phase="legacy schema1 import";auto legacy=clone(source,root/"legacy-v1");{Db db(legacy/"archive.sqlite3");db.transaction([&]{db.execute("DROP TABLE capture_state");db.execute("ALTER TABLE jobs RENAME TO previous_jobs");db.execute("CREATE TABLE jobs(id INTEGER PRIMARY KEY,chat_id TEXT NOT NULL REFERENCES chats(id),status TEXT NOT NULL,started_at TEXT NOT NULL,finished_at TEXT,coverage TEXT NOT NULL DEFAULT 'unknown',counts TEXT NOT NULL DEFAULT '{}')");db.execute("INSERT INTO jobs SELECT id,chat_id,status,started_at,finished_at,coverage,counts FROM previous_jobs");db.execute("DROP TABLE previous_jobs");db.execute("CREATE INDEX jobs_chat ON jobs(chat_id,id)");db.execute("PRAGMA user_version=1");});}auto legacyBefore=hashes(legacy);auto legacyResult=importBackup(legacy,root/"legacy-restored");{Store migrated(legacyResult.root);require(migrated.db.rows("PRAGMA user_version")[0].begin().value()==2);require(!migrated.db.rows("SELECT * FROM capture_state").empty());require(migrated.check().empty());}require(hashes(legacy)==legacyBefore);
 reject([&]{importBackup(source,result.root);});require(hashes(source)==original);
 reject([&]{importBackup(source,source/"nested");});require(!fs::exists(source/"nested"));
 auto repository=privateDir(root/"synthetic-repository");fs::create_directory(repository/".git");reject([&]{importBackup(source,repository/"out");});size_t repoEntries=0;for(const auto&e:fs::directory_iterator(repository)){++repoEntries;require(e.path().filename()==".git");}require(repoEntries==1);
 auto html=privateDir(root/"html-only");atomicText(html/"index.html","example");reject([&]{importBackup(html,root/"not-created");});require(!fs::exists(root/"not-created"));
 auto side=clone(source,root/"sidecars");atomicText(side/"archive.sqlite3-wal","transaction");auto sideBefore=hashes(side);reject([&]{importBackup(side,root/"side-out");});require(hashes(side)==sideBefore&&!fs::exists(root/"side-out"));
 phase="trigger rejection";auto trigger=clone(source,root/"trigger");{Db db(trigger/"archive.sqlite3");db.execute("CREATE TRIGGER import_reject AFTER INSERT ON jobs BEGIN SELECT 1; END");}auto triggerBefore=hashes(trigger);reject([&]{importBackup(trigger,root/"trigger-out");});require(hashes(trigger)==triggerBefore&&!fs::exists(root/"trigger-out"));
 auto version=clone(source,root/"unknown-version");{Db db(version/"archive.sqlite3");db.execute("PRAGMA user_version=999");}reject([&]{importBackup(version,root/"version-out");});require(!fs::exists(root/"version-out"));
 phase="hash corruption rejection";auto corrupt=clone(source,root/"corrupt");fs::path first;for(auto& e:fs::recursive_directory_iterator(corrupt/"assets"))if(e.is_regular_file()){first=e.path();break;}require(!first.empty());atomicText(first,"wrong bytes");auto corruptBefore=hashes(corrupt);reject([&]{importBackup(corrupt,root/"corrupt-out");});require(hashes(corrupt)==corruptBefore&&!fs::exists(root/"corrupt-out"));
 phase="malformed registered path rejection";auto badPath=clone(source,root/"bad-path");{Db db(badPath/"archive.sqlite3");db.execute("UPDATE blobs SET path='../outside.bin' WHERE id=(SELECT id FROM blobs ORDER BY id LIMIT 1)");}reject([&]{importBackup(badPath,root/"path-out");});require(!fs::exists(root/"path-out"));
 phase="cancel retention";auto cancelParent=privateDir(root/"cancel-parent");bool cancelled=false;try{importBackup(source,cancelParent/"out",[&]{for(const auto&e:fs::directory_iterator(cancelParent))if(e.is_directory())throw Cancelled();});}catch(const ImportFailure&e){cancelled=e.cancelled;require(fs::is_directory(e.retained));}require(cancelled&&!fs::exists(cancelParent/"out")&&hashes(source)==original);
 phase="destination collision race";auto raceParent=privateDir(root/"race-parent");bool raced=false;reject([&]{importBackup(source,raceParent/"out",[&]{if(!raced)for(const auto&e:fs::directory_iterator(raceParent))if(e.is_directory()){fs::create_directory(raceParent/"out");atomicText(raceParent/"out"/"keep.txt","existing destination");raced=true;break;}});});require(raced&&readText(raceParent/"out"/"keep.txt")=="existing destination"&&hashes(source)==original);
 phase="source changed during import";auto changed=clone(source,root/"changed-source"),changeParent=privateDir(root/"change-parent");bool changedOnce=false;reject([&]{importBackup(changed,changeParent/"out",[&]{if(!changedOnce&&fs::exists(changeParent)){for(const auto&e:fs::directory_iterator(changeParent))if(fs::exists(e.path()/"collection"/"index.html")){atomicText(changed/"archive.sqlite3","changed source bytes");changedOnce=true;break;}}});});require(changedOnce&&!fs::exists(changeParent/"out"));
 phase="unknown staging content retention";auto retainedParent=privateDir(root/"retained-parent");bool injected=false;auto retained=importBackup(source,retainedParent/"out",[&]{if(!injected)for(const auto&e:fs::directory_iterator(retainedParent))if(fs::exists(e.path()/"collection"/"index.html")){atomicText(e.path()/"keep.txt","do not remove unknown staging content");injected=true;break;}});require(injected&&!retained.retained.empty()&&readText(retained.retained/"keep.txt")=="do not remove unknown staging content");
 phase="source link rejection";auto link=root/"linked-source";
#ifdef _WIN32
 auto command=L"cmd.exe /d /c mklink /J \""+link.wstring()+L"\" \""+source.wstring()+L"\" >nul 2>&1";if(_wsystem(command.c_str())==0){reject([&]{importBackup(link,root/"link-out");});fs::remove(link);std::cout<<"Import source junction rejected\n";}else std::cout<<"SKIP import junction: fixture unavailable\n";
#else
 fs::create_directory_symlink(source,link);reject([&]{importBackup(link,root/"link-out");});fs::remove(link);
#endif
 std::cout<<checks<<" extracted backup import checks passed; source files/ACLs unchanged, no source HTML executed.\n";return 0;
 }catch(const std::exception&e){std::cerr<<"Import test phase ["<<phase<<"]: "<<e.what()<<"\n";return 1;}}
