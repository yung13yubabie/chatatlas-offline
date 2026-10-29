#include "line_cipher.hpp"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
using namespace atlas;
namespace {
int checks=0;
void require(bool valid,const char* message){++checks;if(!valid)throw std::runtime_error(message);}
template<class F>void rejects(F fn,const char* message){bool rejected=false;try{fn();}catch(const Error&){rejected=true;}require(rejected,message);}
template<class F>void cancels(F fn,const char* message){bool stopped=false;try{fn();}catch(const Cancelled&){stopped=true;}require(stopped,message);}
}
int main(int argc,char** argv){try{
 require(argc==2,"owned fixture path required");const fs::path root=path8(argv[1]);require(fs::is_directory(root),"fixture parent exists");
 const auto database=root/"synthetic-line.edb";const std::string secret(32,'1');
 require(LineCipher::version()=="SQLite3 Multiple Ciphers 2.6.1","pinned native cipher engine is active");
 {
  LineCipher writer(database,secret,nullptr,true);
  auto cipher=writer.rows("PRAGMA cipher");require(cipher.size()==1&&cipher[0].begin().value()=="aes128cbc","wxSQLite3 AES128 selected, not SQLCipher");
  writer.rows("CREATE TABLE _message(_id TEXT PRIMARY KEY,_chatId TEXT,_text TEXT,_createdTime INTEGER,bytes BLOB)");
  writer.rows("INSERT INTO _message VALUES(?,?,?,?,?)",{"m1","fixture-chat","人工加密訊息",1790812800123LL,J::binary({0,1,2,255})});
  writer.rows("INSERT INTO _message VALUES(?,?,?,?,?)",{"m2","fixture-chat","第二則人工訊息",1790812800124LL,J::binary({})});
 }
 const auto original=digest(database);const auto permissions=fs::status(database).permissions();
 LineCipher::preflight(database);require(true,"valid encrypted database passes secret-free preflight");
 require(readText(database).substr(0,16)!=std::string("SQLite format 3\0",16),"real encrypted file does not expose a plaintext SQLite header");
 bool ordinaryFailed=false;try{Db ordinary(database,false);ordinary.rows("SELECT count(*) FROM _message");}catch(const Error&){ordinaryFailed=true;}require(ordinaryFailed,"ordinary system SQLite cannot read the encrypted rows");
 {
  LineCipher reader(database,secret);
  auto rows=reader.rows("SELECT * FROM _message ORDER BY _createdTime");
  require(rows.size()==2&&rows[0]["_text"]=="人工加密訊息","encrypted LINE-shaped fixture decrypts natively");
  require(rows[0]["bytes"].is_binary()&&rows[0]["bytes"].get_binary()==J::binary({0,1,2,255}).get_binary(),"binary metadata preserved");
  require(rows[1]["bytes"].is_binary()&&rows[1]["bytes"].get_binary().empty(),"empty BLOB is not converted to NULL");
  require(reader.rows("SELECT null AS n, 1.5 AS f, 3 AS i")[0]==J({{"n",nullptr},{"f",1.5},{"i",3}}),"SQLite value types retained");
  reader.rows("BEGIN");require(reader.rows("SELECT count(*) AS n FROM _message")[0]["n"]==2,"coherent read transaction supported");reader.rows("COMMIT");
  require(!reader.rows("PRAGMA table_info('_message')").empty(),"fixed schema introspection allowed");
  rejects([&]{reader.rows("DELETE FROM _message");},"production database writes denied");
  rejects([&]{reader.rows("PRAGMA user_version=9");},"persistent pragma changes denied");
  rejects([&]{reader.rows("ATTACH ':memory:' AS other");},"additional database access denied");
  rejects([&]{reader.rows("SELECT 1; SELECT 2");},"stacked SQL denied");
  rejects([&]{reader.rows(std::string("SELECT 1\0ignored",16));},"embedded NUL query denied");
  rejects([&]{reader.rows("SELECT ?",J::array());},"missing bound parameters denied");
  rejects([&]{reader.rows("SELECT ?",J::array({J::object()}));},"unsupported bound types denied");
  rejects([&]{reader.rows("SELECT load_extension('not-a-library')");},"loadable extensions are absent");
  int ticks=0;cancels([&]{reader.rows("WITH RECURSIVE n(x) AS (VALUES(0) UNION ALL SELECT x+1 FROM n WHERE x<1000000) SELECT sum(x) AS n FROM n",J::array(),[&]{if(++ticks>=3)throw Cancelled();});},"SQLite VM query progress is cancellable");
  require(reader.rows("SELECT count(*) AS n FROM _message")[0]["n"]==2,"cancelled query leaves reusable connection");
  reader.rows("BEGIN");reader.rows("ROLLBACK");require(reader.rows("SELECT 1 AS n")[0]["n"]==1,"read rollback supported");
 }
 require(digest(database).hash==original.hash&&fs::status(database).permissions()==permissions,"reading and rejected writes preserve encrypted file bytes and permissions");
#ifdef _WIN32
 HANDLE writer=CreateFileW(database.c_str(),GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
 require(writer!=INVALID_HANDLE_VALUE,"synthetic live writer opens");
 try{LineCipher concurrent(database,secret);require(concurrent.rows("SELECT count(*) AS n FROM _message")[0]["n"]==2,"read-only LINE open shares an existing writer handle");}catch(...){CloseHandle(writer);throw;}CloseHandle(writer);
 HANDLE locked=CreateFileW(database.c_str(),GENERIC_READ,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);require(locked!=INVALID_HANDLE_VALUE,"exclusive source handle opens");
 try{rejects([&]{LineCipher::preflight(database);},"unreadable locked source fails before memory scanning");}catch(...){CloseHandle(locked);throw;}CloseHandle(locked);
#endif
 rejects([&]{LineCipher candidate(database,std::string(32,'2'));},"wrong passphrase fails without fallback");
 rejects([&]{LineCipher candidate(database,"invalid");},"invalid key format fails");
 rejects([&]{LineCipher candidate(root/"missing.edb",secret);},"read-only open never creates a missing LINE database");
 rejects([&]{LineCipher::preflight(root/"missing.edb");},"missing source fails before memory scanning");
 require(!fs::exists(root/"missing.edb"),"missing source remains absent");
 rejects([&]{LineCipher candidate(database,secret,nullptr,true);},"fixture creation cannot overwrite existing data");
 const auto plain=root/"plain.sqlite3";{Db db(plain);db.rows("CREATE TABLE marker(value TEXT)");}
 rejects([&]{LineCipher candidate(plain,secret);},"ordinary SQLite is never silently accepted as encrypted LINE");
 rejects([&]{LineCipher::preflight(plain);},"plaintext source fails before memory scanning");
 const auto shortFile=root/"short.edb";atomicText(shortFile,std::string(128,'x'));rejects([&]{LineCipher::preflight(shortFile);},"file smaller than a SQLite page fails before memory scanning");
 const auto brokenPage=root/"broken-page.edb";atomicText(brokenPage,std::string(513,'x'));rejects([&]{LineCipher::preflight(brokenPage);},"truncated encrypted page fails before memory scanning");
 Control stopped;stopped.stopped=true;cancels([&]{LineCipher candidate(database,secret,&stopped);},"constructor honors cancellation before I/O");
 cancels([&]{LineCipher::preflight(root/"not-accessed.edb",&stopped);},"preflight honors cancellation before I/O");
 require(digest(database).hash==original.hash,"all failed key and cancellation paths preserve original database");
#ifndef _WIN32
 const auto linked=root/"linked.edb";fs::create_symlink(database,linked);rejects([&]{LineCipher candidate(linked,secret);},"linked source rejected");
#endif
 std::cout<<checks<<" native LINE cipher checks passed; encrypted synthetic data only.\n";return 0;
 }catch(const std::exception& error){std::cerr<<"Native LINE cipher test failed: "<<error.what()<<"\n";return 1;}}
